#pragma once

#include <dynalog/include/Logger.h>
#include <dynalog/include/async/LatencyQueue.h>
#include <dynalog/include/async/Flush.h>

namespace dynalog { namespace async {

	class Dispatcher {
	public:
		void insert( Emitter * emitter, const Logger & logger, Message && message );

		inline size_t slots( void ) const  { return queue.slots(); }

		void work( size_t index = 0 );

		void run( void );

		Dispatcher( const std::chrono::steady_clock::duration & latency = std::chrono::milliseconds( 1 ), 
			const std::chrono::steady_clock::duration & timeout = std::chrono::steady_clock::duration::max(),
			size_t capacity = 128,
			size_t heads = 1,
			size_t partitions = 1 );

		void flush( Flush & flush );

	protected:
		struct Action
		{
			Emitter * emitter;
			const Logger & logger;
			Message message;
		};
		struct Worker
		{
			bool stop = false;
			Dispatcher & dispatcher;
			const size_t index;
			std::thread thread;

			Worker(Dispatcher &, size_t);
			~Worker();
		};
		LatencyQueue<Action> queue;
		std::chrono::steady_clock::duration timeout;
		std::vector<std::unique_ptr<Worker> > threads;
	};


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
