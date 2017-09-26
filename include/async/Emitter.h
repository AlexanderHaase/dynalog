#pragma once

#include <dynalog/include/Logger.h>
#include <dynalog/include/async/LatencyQueue.h>

namespace dynalog { namespace async {

	class Dispatcher {
	public:
		void insert( Emitter * emitter, const Logger & logger, Message && message );

		inline size_t slots( void ) const  { return queue.slots(); }

		void work( size_t index = 0 );

		void run( void );

		Dispatcher( std::chrono::steady_clock::duration latency = std::chrono::duration_cast<std::chrono::steady_clock::duration>( std::chrono::milliseconds( 1 ) ), size_t capacity = 128, size_t readers = 1 );

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
