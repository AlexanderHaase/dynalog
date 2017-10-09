#pragma once
#include <dynalog/include/Logger.h>
#include <dynalog/include/async/LatencyQueue.h>
#include <dynalog/include/async/Flush.h>

namespace dynalog { namespace async {

	/// Receives and invokes async differed messages.
	///
	/// Async logging offloads message formatting and emitting to another
	/// context. Dispatcher is a collection point for messages to be
	/// processed. Messages are enqueued for later processing, capturing a
	/// logger and emitter. Messages may be dequeued by either internal or
	/// external workers. Enqueing behavior is highly configurable--see
	/// LatencyQueue.
	///
	class Dispatcher {
	public:
		/// Delegate a message to be processed later.
		///
		/// Attempts to insert a message into dispatch queue. The
		/// message may be dropped if no space is available by the
		/// configured deadline.
		///
		/// @param emitter Emitter to use for the message.
		/// @param logger Logger submitting the message.
		/// @param message Message to Enqueue for defered processing.
		///
		void insert( Emitter * emitter, const Logger & logger, Message && message );

		/// Query the number of workers required to process messages.
		///
		/// LatencyQueue's design requires every slot is worked.
		///
		/// @return Number of workers required for proper operation.
		///
		inline size_t slots( void ) const  { return queue.slots(); }

		/// TODO: support external workers with predicates.
		///
		void work( size_t index = 0 );

		/// Spawn dedicated threads to process each slot.
		///
		void run( void );

		/// Construct a new dispatcher.
		///
		/// @param latency Period where workers will actively check for
		///	pending messages.
		/// @param timeout Maximum period to attempt to insert a
		///	message into a full queue.
		/// @param capacity Maximum number of messages that can be 
		///	buffered per source thread. If more messages are
		///	published prior to a worker collecting them, the
		///	insert attempts to wake and hand off buffered messages
		///	to a worker.
		/// @param heads Number of workers per partition. Workers
		///	cooperate to emit messages within a partition, but
		///	introduce output reordering.
		/// @param partitions Number of isolated pools for processing
		///	messages. A single pool can hit contention bottlenecks
		///	as thread counts and/or core counts rise. Partitions
		///	segregate threads into separate groups to reduce
		///	contention.
		///
		Dispatcher( const std::chrono::steady_clock::duration & latency = std::chrono::milliseconds( 1 ), 
			const std::chrono::steady_clock::duration & timeout = std::chrono::steady_clock::duration::max(),
			size_t capacity = 128,
			size_t heads = 1,
			size_t partitions = 1 );

		/// Insert a flush barrier into all input streams.
		///
		/// Inserting flush into an output stream is only effective for
		/// the current thread. Dispatcher::flush(...) produces a
		/// global barrier by insert a flush token into all output
		/// streams. Note that async processing allows messages after
		/// Dispatcher::flush(...) to be emitted before flush completes
		/// on all workers.
		///
		/// @param flush Flush object for barrier across all output
		///	streams.
		///
		void flush( Flush & flush );

	protected:
		/// Bundle message details for deferred processing.
		///
		struct Action
		{
			Emitter * emitter;
			const Logger & logger;
			Message message;
		};

		/// Worker class for spawning a dedicated thread.
		///
		struct Worker
		{
			bool stop = false;
			Dispatcher & dispatcher;
			const size_t index;
			std::thread thread;

			Worker(Dispatcher &, size_t);
			~Worker();
		};

		/// Queue for storing pending messages.
		///
		LatencyQueue<Action> queue;
		std::chrono::steady_clock::duration timeout;
		std::vector<std::unique_ptr<Worker> > threads;
	};

	/// Emitter proxy that submits messages to a dispatcher.
	///
	/// Wraps an emitter to use for actual emitting, and hands off messages
	/// to a dispatcher for async processing.
	///
	class DeferredEmitter : public Emitter {
	public:
		virtual ~DeferredEmitter();
		virtual void emit( const Logger & logger, Message && message );

		DeferredEmitter( const std::shared_ptr<Dispatcher> & dispatcher, Emitter * emitter );
	protected:
		std::shared_ptr<Dispatcher> dispatcher;
		Emitter * emitter;
	};

} }
