#pragma once

#include <mutex>
#include <vector>
#include <condition_variable>
#include <dynalog/include/async/Replicated.h>
#include <cassert>

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
	template < typename T, typename Clock = std::chrono::steady_clock >
	class LatencyQueue {
	protected:
		struct IngressNode;
		struct ReceiveNode;
		struct ReadHead;
	public:
		/// Insert a value into the queue.
		///
		/// @param value R-reference to value to insert.
		/// @param timeout Maximum time to wait space in the queue.
		/// @return Boolean indication of success.
		///
		bool insert( T && value, const typename Clock::duration & timeout = Clock::duration::zero() )
		{
			struct Request
			{
				T value;
				LatencyQueue & queue;
				const typename Clock::duration & timeout;

				/// Place the value in the ingress cache if there's space before the timeout elapses.
				///
				bool operator() ( IngressNode & ingress, std::unique_lock<std::mutex> & lock )
				{
					// Forward-allocate a constant deadline so we don't retry forever.
					//
					typename Clock::time_point deadline = Clock::time_point::min();
					for(;;)
					{
						const auto result = ingress.cache.size() < ingress.cache.capacity() || queue.receivers.with( ingress );
						if( result )
						{
							ingress.cache.emplace_back( std::move( value ) );
						}
						else if( timeout != Clock::duration::zero() )
						{
							// Create deadline if not set(defer potential syscall)
							//
							if( deadline == Clock::time_point::min() )
							{
								const auto now = Clock::now();
								const auto computed = now + timeout;
								deadline = computed > now ? computed : Clock::time_point::max();
							}
							if( ingress.condition.wait_until( lock, deadline ) == std::cv_status::no_timeout )
							{
								continue;
							}
						}
						return result;
					}
				}
			};
			return ingresses.with( Request{ std::move( value ), *this, timeout } );
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
			return receivers.with( index / heads, [&]( ReceiveNode & receive, std::unique_lock<std::mutex> & lock )
			{
				return receive.remove( index % heads, pred, func, *this, lock );
			});
		}

		/// Create a latency queue as specified.
		///
		/// For now, approximate two caches per ingress, divided equally among receivers.
		///
		/// @param latency Maximum latency between queue synchronizations.
		/// @param capacity Maximum number of events to cache before synchronization.
		/// @param receivers Number of receiver slots to create.
		///
		LatencyQueue( const typename Clock::duration & latency, size_t capacity, size_t receivers, size_t heads )
		: ingresses( std::make_tuple( capacity ) )
		, receivers( receivers, std::make_tuple( heads, capacity, (1+ingresses.size()) / receivers, Clock::now() + latency ) )
		, latency( latency )
		, heads( heads )
		{}

		/// Number of receiver slots--each slot SHOULD be worked.
		///
		size_t const slots() const { return receivers.size() * heads; }

	protected:
		/// Cache for inserted items. Items sit here until the cache
		/// fills or is drained by a receiver.
		///
		struct IngressNode
		{
			std::condition_variable condition;
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

		struct ReadHead 
		{
			bool occupied;				///< Indicate if node is in use.
			std::vector< std::vector<T> > drain;	///< Caches in the process of draining.

			// Iterators for resuming draining
			//
			typename std::vector< std::vector<T> >::iterator cache;
			typename std::vector<T>::iterator item;

			ReadHead( size_t caches )
			: occupied( false )
			{
				drain.reserve( caches );
				cache = drain.end();
			}

			// Resume draining elements. Returns last value indicated by predicate.
			//
			template < typename Pred, typename Func >
			bool resume( Pred && pred, Func && func, std::unique_lock<std::mutex> & lock )
			{
				bool finished = pred();

				// If elements are left to drain, resume draining until empty or pred().
				//
				if( drain.size() )
				{
					lock.unlock();
					for(;;)
					{
						for( ;item != cache->end() && !finished; ++item )
						{
							func( std::move( *item ) );
							finished = pred();
						}
						
						if( !finished && ++cache != drain.end() )
						{
							item = cache->begin();
						}
						else
						{
							break;
						}
					}
					lock.lock();
				}

				return finished;
			}

			// Prepare to receive the indicated caches
			//
			void load( std::vector< std::vector<T> > & ready )
			{
				std::swap( ready, drain );
				cache = drain.begin();
				if( cache != drain.end() )
				{
					item = cache->begin();
				}
			}

			// Move empty cahces back for reuse.
			//
			void unload( std::vector< std::vector<T> > & empty )
			{
				
				for( auto && cache : drain )
				{
					cache.clear();
					empty.emplace_back( std::move( cache ) );
				}
				drain.clear();
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
			ReceiveNode( size_t heads, size_t capacity, size_t caches, const typename Clock::time_point & next )
			: deadline( next )
			{
				ready.reserve( caches );
				empty.reserve( caches );
				readers.reserve( heads );
				waiting.reserve( caches );

				for( size_t index = 0; index < caches; ++index )
				{
					std::vector<T> cache;
					cache.reserve( capacity );
					empty.emplace_back( std::move( cache ) );
				}
				for( size_t index = 0; index < heads; ++index )
				{
					readers.emplace_back( caches );
				}
			}

			std::condition_variable condition;
			std::vector< std::vector<T> > ready;	///< Caches ready to be drained
			std::vector< std::vector<T> > empty;	///< Empty caches

			std::vector< IngressNode * > waiting;	///< Condition nodes waiting for empty caches.
			std::vector< ReadHead > readers;

			typename Clock::time_point deadline;	///< Next time to drain

			template < typename Pred, typename Func >
			bool remove( const size_t head, Pred && pred, Func && func, LatencyQueue & queue, std::unique_lock<std::mutex> & lock )
			{
				ReadHead & reader = readers[ head ];
				if( reader.occupied ){ return false; }
				reader.occupied = true;

				for(;;)
				{
					// exit if predicate indicated.
					//
					if( reader.resume( pred, func, lock ) )
					{
						break;
					}

					// Return drained caches to the empty cache.
					//
					reader.unload( empty );

					// wake all waiting ingress since we have no idea about wait timeout...
					//
					for( auto && ingress : waiting )
					{
						ingress->condition.notify_all();
					}
					waiting.clear();

					// Wait & sweep ingress for non-empty caches if none are ready.
					//
					if( ready.size() == 0 )
					{
						const std::cv_status result = condition.wait_until( lock, deadline );

						// Skip sweep if sleep was interrupted.
						//
						if( result == std::cv_status::timeout )
						{
							deadline = Clock::now() + queue.latency;

							// Scan only every nth ingress to divide ingresses among receivers.
							//
							const auto increment = queue.receivers.size();
							lock.unlock();
							for( size_t index = threadindex( increment ); index < queue.ingresses.size(); index += increment )
							{
								queue.ingresses.with( index, [index,&queue]( IngressNode & ingress )
								{
									if( ingress.cache.size() )
									{
										queue.receivers.with( index, ingress );
									}
								});
							}
							lock.lock();
						}
					}

					// Prepare to drain ready caches.
					//
					reader.load( ready );
				}
				reader.occupied = false;
				return true;
			}
			
			/// Offload ingress cache contents, or register for wakeup.
			///
			/// @return True if cache was non-empty, offloaded, and replaced with an empty cache.
			///
			bool operator() ( IngressNode & ingress )
			{
				const auto result = ready.size() < ready.capacity() && empty.size();
				if( result )
				{
					ready.emplace_back( std::move( ingress.cache ) );
					ingress.cache = std::move( empty.back() );
					empty.pop_back();
				}
				else
				{
					waiting.emplace_back( &ingress );
				}
				return result;
			}
		};

		/// Proxy object to bring predicate and function into receiver scope.
		///
		/*template < typename Pred, typename Func >
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
		};*/

		Replicated< IngressNode > ingresses;
		Replicated< ReceiveNode > receivers;
		const typename Clock::duration latency;
		const size_t heads;
	};


} }
