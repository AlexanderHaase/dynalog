#include <unistd.h>
#include <cstring>
#include <dynalog/include/async/Emitter.h>

namespace dynalog { namespace async {

	void Dispatcher::insert( Emitter * emitter, const Logger & logger, const Message & message )
	{
		const auto success = queue.insert( Action{ emitter, logger, message }, timeout );
		if( ! success )
		{
			// Last-resort warning
			//
			dprintf( STDERR_FILENO, "Warning: dynalog::async::Dispatcher: Queue full, dropping message!\n" );
		};
	}

	struct NoOpEmitter : Emitter {
    virtual ~NoOpEmitter() = default;
		virtual void emit( const Logger &, const Message & ) override {}
	};

	static NoOpEmitter flushEmitter;

	void Dispatcher::flush( Flush & flush )
	{
		Message message;
		message.format( flush );
		for( size_t index = 0; index < queue.size(); ++index )
		{
			queue.insert( index,
				Action{ &flushEmitter, *(const Logger*)( nullptr ), message },
				timeout );
		}
	}

	void Dispatcher::work( size_t index )
	{
		queue.remove( index,
			[]{ return true; },
			[]( Action && action ){ action.emitter->emit( action.logger, action.message ); });
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

	Dispatcher::Worker::Worker(Dispatcher & dispatch, size_t idx)
	: stop( false )
	, dispatcher( dispatch )
	, index( idx )
	, thread( [this]
	{
		dispatcher.queue.remove( index, 
			[this]{ return stop; },
			[]( Action && action ){ action.emitter->emit( action.logger, std::move( action.message ) ); });
	})
	{}

	Dispatcher::Worker::~Worker()
	{
		stop = true;
		thread.join();
	}

	Dispatcher::Dispatcher( const std::chrono::steady_clock::duration & latency, 
			const std::chrono::steady_clock::duration & timeout_arg,
			size_t capacity,
			size_t heads,
			size_t partitions )
	: queue( latency, capacity, 4, heads, partitions )
	, timeout( timeout_arg )
	{}

	void DeferredEmitter::emit( const Logger & logger, const Message & message )
	{
		dispatcher->insert( emitter, logger, message );
	}

	DeferredEmitter::DeferredEmitter( const std::shared_ptr<Dispatcher> & dispatcher_arg, Emitter * emitter_arg )
	: dispatcher( dispatcher_arg )
	, emitter( emitter_arg )
	{}

	DeferredEmitter::~DeferredEmitter() {}
} }
