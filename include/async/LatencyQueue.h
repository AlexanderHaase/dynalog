#pragma once

#include <mutex>
#include <vector>
#include <condition_variable>
#include <dynalog/include/async/Replicated.h>

namespace dynalog { namespace async {

	/// Concurrent queue oriented on maintaining a maximum latency.
	///
	/// To debounce reciever wakeup and associated contention, ingress
	/// objects are first stored in a thread-associative cache of elements.
	/// That cache is periodically drained by the receiver at an interval
	/// less than the maximum latency. Should an ingress cache reach
	/// capacity before it is drained, it's contents are pushed to a
	/// receiver and the receiver's thread is woken up.
	///
	/// The queue scales to multiple writers via thread-associative set of
	/// caches sized according to the number of hardware threads. The queue
	/// scales to a fixed number of readers by round-robin assigning each
	/// igress cache to a reader(each ingress cache is drained by one and
	/// only one reader). The queue operates with a fixed capacity
	/// reflecting a static set of internal buffers for allocation-free
	/// operation. At capacity, inserts are discarded.
	///
	/// -- Internals--
	///
	/// Within the queue, std::vectors are used as the element cache, with
	/// appropriate use of reserve() and capacity() to avoid allocation.
	/// Non-empty caches are std::swap()'d with empty caches when they
	/// are drained to minimize time under mutex. A fixed number of
	/// caches are maintained per receiver: at least 2 * number of
	/// ingress caches, so that ingress caches may be swapped.
	///
	/// Initialization parameters:
	///   - number of receivers.
	///   - number of caches.
	///   - cache capacity.
	///   - latency.
	/// 
	template < typename T, typename Clock = std::chrono::system_clock >
	class LatencyQueue {
	protected:
		struct IngressNode;
		struct ReceiveNode;
	public:
		/// Insert a value into the queue.
		///
		/// @param value R-reference to value to insert.
		/// @return Boolean indication of success.
		///
		bool insert( T && value )
		{
			return ingresses.with( capture( std::move( value ), [this]( T && value, IngressNode & ingress ) -> bool
			{
				const auto result = ingress.cache.size() <= ingress.cache.capacity() || receivers.with( ingress );
				if( result )
				{
					ingress.cache.emplace_back( std::move( value ) );
				}
				return result;
			}));
		}

		/// Remove elements until predicate indicates stop.
		///
		/// @tparam Pred Predicate class.
		/// @tparam Func Functor class for receiving items--func( T && ).
		/// @param index Recevier slot to receive at.
		/// @param pred Predicate instance to check.
		/// @param func Function to receive items.
		/// @return False if receive slot is occupied, true otherwise.
		///
		template < typename Pred, typename Func >
		bool remove( size_t index, Pred && pred, Func && func )
		{
			return receivers.with( index, Receiver<Pred,Func>{ std::forward<Pred>( pred ), std::forward<Func>( func ), *this });
		}

		LatencyQueue( const typename Clock::duration & latency, size_t capacity, size_t receivers )
		: ingresses( std::make_tuple( capacity ) )
		, receivers( receivers, std::make_tuple( capacity, (1+ingresses.size()) / receivers, typename Clock::now() + latency ) )
		, latency( latency )
		{}

	protected:
		/// Cache for inserted items. Items sit here until the cache
		/// fills or is drained by a receiver.
		///
		struct IngressNode
		{
			std::vector<T> cache;

			/// Try to push contents to a receiver, notifying them on success.
			///
			bool operator() ( ReceiveNode & receive )
			{
				const auto result = receive( *this );
				if( result )
				{
					receive.condition.notify_one();
				}
				return result;
			}

			/// Initialize with the specified capacity.
			///
			IngressNode( size_t capacity )
			{
				cache.reserve( capacity );
			}
		};

		/// Wait for, receive, and deque items. Ingress nodes offload
		/// non-empty caches into 'ready' and get empty caches from 
		/// 'empty'. 'drain', 'cache', and 'item' hold the partial
		/// progress of removing items from the queue. The receiver
		/// resumes partial progress(if any) and returns empty caches
		/// to 'empty'. Then, if no caches are immediately in 'ready',
		/// the receiver waits for the next deadline(or wakeup). If
		/// the receiver waits until deadline, it sweeps it's subset
		/// of the ingress caches into 'ready'. In all three cases(
		/// ready non-empty, wait interrupted, or sweep), the receiver
		/// prepares to drain 'ready' caches, and loops to the top.
		///
		/// At the top, and between every item in drain, the predicate
		/// is checked for exit.
		///
		struct ReceiveNode
		{
			/// Initialize with specified number of caches each of
			/// the specified capacity.
			///
			ReceiveNode( size_t capacity, size_t caches, const typename Clock::time_point & next )
			: occupied( false )
			, deadline( next )
			{
				ready.reserve( caches );
				empty.reserve( caches );
				drain.reserve( caches );

				for( size_t index = 0; index < caches; ++index )
				{
					std::vector<T> cache;
					cache.reserve( capacity );
					empty.emplace_back( std::move( cache ) );
				}
				cache = drain.begin();
			}

			std::condition_variable condition;
			std::vector< std::vector<T> > ready;	///< Caches ready to be drained
			std::vector< std::vector<T> > empty;	///< Empty caches
			std::vector< std::vector<T> > drain;	///< Caches in the process of draining.

			// Iterators for resuming draining
			//
			typename std::vector< std::vector<T> >::iterator cache;
			typename std::vector<T>::iterator item;

			bool occupied;	///<Indicate if receive is in progress.
			typename Clock::time_point deadline;	///< Next time to drain

			template < typename Pred, typename Func >
			bool remove( Pred && pred, Func && func, LatencyQueue & queue, std::unique_lock<std::mutex> & lock )
			{
				if( occupied ){ return false; }
				occupied = true;

				for(;;)
				{
					bool finished = pred();

					// If elements are left to drain, resume draining until empty or pred().
					//
					if( drain.size() )
					{
						lock.unlock();
						for(; cache != drain.end() && !finished ; ++cache )
						{
							for( ;item != cache->end() && !finished; ++item )
							{
								func( std::move( *item ) );
								finished = pred();
							}
						}
						lock.lock();
					}

					// exit if predicate indicated.
					//
					if( finished )
					{
						break;
					}

					// Return drained caches to the empty cache.
					//
					for( auto && cache : drain )
					{
						cache.clear();
						empty.emplace_back( std::move( cache ) );
					}
					drain.clear();

					// Wait & sweep ingress for non-empty caches if none are ready.
					//
					if( ready.size() == 0 )
					{
						const auto result = condition.wait_until( lock, deadline, [this]{ return drain.size(); });

						// Skip sweep if sleep was interrupted.
						//
						if( result == std::cv_status::timeout )
						{
							deadline = typename Clock::now() + queue.latency;

							// Scan only every nth ingress to divide ingresses among receivers.
							//
							const auto increment = queue.receivers.size();
							for( size_t index = threadindex( increment ); index < queue.ingresses.size(); index += increment )
							{
								queue.ingresses.with( index, *this );
							}
						}
					}

					// Prepare to drain ready caches.
					//
					std::swap( ready, drain );
					cache = drain.begin();
					if( cache != drain.end() )
					{
						item = cache->begin();
					}
				}
				occupied = false;
				return true;
			}
			
			/// Offload ingress cache contents.
			///
			/// @return True if cache was non-empty, offloaded, and replaced with an empty cache.
			///
			bool operator() ( IngressNode & ingress )
			{
				const auto result = ingress.cache.size() && ready.size() < ready.capacity() && empty.size();
				if( result )
				{
					ready.emplace_back( std::move( ingress.cache ) );
					ingress.cache = std::move( empty.back() );
					empty.pop_back();
				}
				return result;
			}
		};

		/// Proxy object to bring predicate and function into receiver scope.
		///
		template < typename Pred, typename Func >
		struct Receiver
		{
			Pred pred;
			Func func;
			LatencyQueue & queue;

			Receiver( Pred && pred, Func && func, LatencyQueue & queue )
			: pred( std::forward<Pred>( pred ) )
			, func( std::forward<Func>( func ) )
			, queue( queue )
			{}

			bool operator() ( ReceiveNode & receive, std::unique_lock<std::mutex> & lock )
			{
				return receive.remove( pred, func, queue, lock );
			}
		};

		Replicated< IngressNode > ingresses;
		Replicated< ReceiveNode > receivers;
		const typename Clock::duration latency;
	};


} }
