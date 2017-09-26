#include <dynalog/include/async/Emitter.h>

namespace dynalog { namespace async {

	void Dispatcher::insert( Emitter * emitter, const Logger & logger, Message && message )
	{
		queue.insert( Action{ emitter, logger, std::move( message ) } );
	}

	void Dispatcher::work( size_t index )
	{
		queue.remove( index,
			[]{ return true; },
			[]( Action && action ){ action.emitter->emit( action.logger, std::move( action.message ) ); });
	}

	void Dispatcher::run( void )
	{
		if( ! threads.size() )
		{
			for( size_t index = 0; index < slots(); ++index )
			{
				threads.emplace_back( new Worker{ *this, index });
			}
		}
	}

	Dispatcher::Worker::Worker(Dispatcher & dispatcher, size_t index)
	: stop( false )
	, dispatcher( dispatcher )
	, index( index )
	, thread( [&]
	{
		dispatcher.queue.remove( index, 
			[&]{ return stop; },
			[]( Action && action ){ action.emitter->emit( action.logger, std::move( action.message ) ); });
	})
	{}

	Dispatcher::Worker::~Worker()
	{
		stop = true;
		thread.join();
	}

	Dispatcher::Dispatcher( std::chrono::steady_clock::duration latency, size_t capacity, size_t readers )
	: queue( latency, capacity, readers )
	{}

	void DeferredEmitter::emit( const Logger & logger, Message && message )
	{
		dispatcher->insert( emitter, logger, std::move( message ) );
	}

	DeferredEmitter::DeferredEmitter( const std::shared_ptr<Dispatcher> & dispatcher, Emitter * emitter )
	: dispatcher( dispatcher )
	, emitter( emitter )
	{}

	DeferredEmitter::~DeferredEmitter() {}
} }
