#pragma once

#include <mutex>
#include <vector>
#include <condition_variable>
#include <dynalog/include/async/Replicated.h>
#include <dynalog/include/async/RingBuffer.h>
#include <cassert>

namespace dynalog { namespace async {

	/// Concurrent queue oriented on maintaining a maximum latency.
	///
	/// TODO: update description post overhaul
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
	public:
		/// Insert a value into the queue.
		///
		/// @param value R-reference to value to insert.
		/// @param timeout Maximum time to wait space in the queue.
		/// @return Boolean indication of success.
		///
		bool insert( size_t index, T && value, const typename Clock::duration & timeout = Clock::duration::zero() )
		{
			return caches.with( index, [&]( Cache & cache, std::unique_lock<std::mutex> & lock )
			{
				auto deadline = Clock::time_point::min();
				auto ticket = cache.tickets.empty() ? std::unique_ptr<Ticket>{ new Ticket{ lock } }
					: cache.tickets.pop();
				
				bool result;
				for(;;)
				{
					if( !cache.cache.full() )
					{
						cache.cache.emplace( std::move( value ) );
						result = true;
						break;
					}

					const bool wait = depos.with( [&]( Depo & depo, std::unique_lock<std::mutex> & lock )
					{
						const bool full = depo.ready.full() || depo.spare.empty();
						if( full )
						{
							depo.wait( *ticket );
						}
						else
						{
							depo.ready.emplace( std::move( cache.cache ) );
							cache.cache = depo.spare.pop();
							if( depo.sleeping )
							{
								lock.unlock();
								depo.condition.notify_one();
							}
						}
						return full;
					});

					if( wait )
					{
						if( deadline == Clock::time_point::min() )
						{
							const auto now = Clock::now();
							const auto computed = now + timeout;
							deadline = computed > now ? computed : Clock::time_point::max();
						}

						if( ticket->wait( lock, deadline )  )
						{
							ticket->reset();
						}
						else
						{
							depos.with( [&]( Depo & depo )
							{
								depo.unwait( *ticket );
							});
							result = false;
							break;
						}
					}

				}
				if( !cache.tickets.full() )
				{
					ticket->reset();
					cache.tickets.emplace( std::move( ticket ) );
				}
				return result;
			});
		}

		/// Insert a value into the queue.
		///
		/// @param value R-reference to value to insert.
		/// @param timeout Maximum time to wait space in the queue.
		/// @return Boolean indication of success.
		///
		bool insert( T && value, const typename Clock::duration & timeout = Clock::duration::zero() )
		{
			return insert( threadindex(), std::forward<T>( value ), timeout );
		}

		size_t slots( void ) const { return depos.size() * readersPerDepo; }
		size_t size( void ) const { return caches.size(); }

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
			if( index > slots() ) { return false; }

			const size_t idex = index % depos.size();
			const size_t head = index / depos.size();

			return depos.with( idex, [&]( Depo & depo, std::unique_lock<std::mutex> & lock )
			{
				auto & reader = depo.readers[ head ];
				if( reader.occupied ) { return false; }
				reader.occupied = true;
				lock.unlock();
				for(;;)
				{
					bool finished;

					while( !(finished = pred()) && !reader.drain.empty()  )
					{
						func( reader.drain.pop() );
					}
					lock.lock();

					if( finished ) { break; }

					if( depo.ready.empty() )
					{
						depo.sleeping += 1;
						const auto status = depo.condition.wait_until( lock, reader.deadline );
						depo.sleeping -= 1;

						if( status == std::cv_status::timeout && depo.ready.empty() )
						{
							reader.deadline += latency;
							lock.unlock();
							collect( idex );
							lock.lock();
						}
					}

					Ticket * ticket = nullptr;

					if( !depo.ready.empty() )
					{
						depo.spare.emplace( std::move( reader.drain ) );
						reader.drain = depo.ready.pop();
						if( depo.waiting.size() )
						{
							ticket = depo.waiting.pop();
						}
					}
					lock.unlock();
					if( ticket ) { ticket->wake(); }
				}
				reader.occupied = false;
				return true;
			});
		}

		LatencyQueue( const typename Clock::duration & abs_latency,
			size_t capacity,
			size_t scale = 1,
			size_t readersPerDepo = 1,
			size_t nDepos = 1 )
		: latency( abs_latency * readersPerDepo )
		, caches( std::make_tuple( capacity ) )
		, depos( nDepos, std::make_tuple( abs_latency,
			capacity, 
			readersPerDepo, 
			(caches.size() + 1) / nDepos - 1,
			scale ) )
		, readersPerDepo( readersPerDepo )
		{}

	protected:

		// TODO: Compare caching tickets with exclusive conditions to shared conditions.
		//
		struct Ticket {
			std::atomic<bool> ready;
			std::condition_variable condition;
			std::mutex & mutex;

			Ticket( std::unique_lock<std::mutex> & lock )
			: ready( false )
			, mutex( *lock.mutex() )
			{}

			bool wait( std::unique_lock<std::mutex> & lock, const typename Clock::time_point & deadline )
			{
				return condition.wait_until( lock, deadline, [this] { return ready.load( std::memory_order_relaxed ); } );
			}

			void wake()
			{
				std::unique_lock<std::mutex> lock( mutex );
				ready.store( true, std::memory_order_relaxed );
				lock.unlock();
				condition.notify_one();
			}

			void reset()
			{
				ready.store( false, std::memory_order_relaxed );
			}
		};

		struct Cache
		{
			Cache( size_t capacity )
			: cache( capacity )
			, tickets( 8 )
			{}

			RingBuffer<T> cache;
			RingBuffer<std::unique_ptr<Ticket>> tickets;
		};

		struct Reader
		{
			Reader( const typename Clock::time_point & time, size_t capacity )
			: drain( capacity )
			, deadline( time )
			{}
			bool occupied = false;
			RingBuffer<T> drain;
			typename Clock::time_point deadline;
		};

		struct Depo
		{
			Depo( const typename Clock::duration & latency, size_t capacity, size_t n_readers, size_t n_waiters, size_t scale = 1 )
			: ready( n_waiters * scale )
			, spare( n_waiters * scale )
			, waiting( n_waiters * scale )
			{
				auto count = n_waiters * scale;
				for( size_t index = 0; index < count; ++ index )
				{
					spare.emplace( capacity );
				}

				const auto now = Clock::now();
				readers.reserve( n_readers );
				for( size_t index = 0; index < n_readers; ++index )
				{
					readers.emplace_back( now + latency * index, capacity );
				}
			}

			void wait( Ticket & ticket )
			{
				if( waiting.full() )
				{
					waiting.reshape( waiting.capacity() * 2 );
				}
				waiting.emplace( &ticket );
			}

			void unwait( Ticket & ticket )
			{
				waiting.erase( [&ticket]( Ticket * other ) { return &ticket == other; } );
			}


			RingBuffer<RingBuffer<T>> ready;
			RingBuffer<RingBuffer<T>> spare;
			std::vector<Reader> readers;
			RingBuffer<Ticket*> waiting;
			std::condition_variable condition;
			size_t sleeping = 0;
		};

		const typename Clock::duration latency;
		Replicated< Cache > caches;
		Replicated< Depo > depos;
		const size_t readersPerDepo;

		void collect( size_t slot )
		{
			for( size_t index = slot; index < caches.size(); index += depos.size() )
			{
				caches.with( index, [&]( Cache & cache )
				{
					if( !cache.cache.empty() )
					{
						depos.with( slot, [&cache]( Depo & depo )
						{
							if( !depo.ready.full() && !depo.spare.empty() )
							{
								depo.ready.emplace( std::move( cache.cache ) );
								cache.cache = depo.spare.pop();
							} 
						});
					}
				});
			}
		}
	};

} }
