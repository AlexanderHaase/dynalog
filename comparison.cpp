#include <dynalog/include/Log.h>
#include <dynalog/include/HandleEmitter.h>
#include <dynalog/include/async/Flush.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <unistd.h>

template < size_t Iterations = 1000000, typename Callable, typename PostCondition >
double usecPerCall( Callable && callable, PostCondition && condition )
{
	struct timeval begin, end;
	gettimeofday( &begin, nullptr );

	for( size_t index = 0; index < Iterations; ++index )
	{
		callable();
	}

	condition();

	gettimeofday( &end, nullptr );

	const auto elapsed = ( end.tv_sec - begin.tv_sec ) * 1000000 + end.tv_usec - begin.tv_usec;

	return double(elapsed)/double(Iterations);
}


struct Benchmark
{
	std::vector<std::tuple<std::string, double> > results;


	template< typename Callable, typename PostCondition >
	void measure( const std::string & tag, Callable && callable, PostCondition && condition )
	{
		results.emplace_back( tag, usecPerCall( std::forward<Callable>( callable ), std::forward<PostCondition>( condition ) ) );
	}

	template< typename Callable >
	void measure( const std::string & tag, Callable && callable )
	{
		measure( tag, std::forward<Callable>( callable ), []{} );
	}

	double worst( void )
	{
		double max = std::numeric_limits<double>::min();
		for( auto && result : results )
		{
			auto usec = std::get<1>( result );
			if( usec > max )
			{
				max = usec;
			}
		}
		return max;
	}

	void log( std::ostream & stream, double baseline )
	{
		for( auto && result : results )
		{
			auto & tag = std::get<0>( result );
			auto usec = std::get<1>( result );
			auto relative = baseline / usec;
			stream << std::setprecision( 5 ) << std::fixed << usec << " usec/call (";
			stream << std::setprecision( 2 ) << std::fixed << relative;
			stream << " x)\t" << tag << std::endl;
		}
	}

	void log( std::ostream & stream ) { log( stream, worst() ); }
};


struct Callable
{
	void operator()() const {
		DYNALOG( dynalog::Level::VERBOSE, "MAIN", dynalog::Level::VERBOSE, "inside callable" );
	}
} callable;

struct NoOpEmitter : public dynalog::Emitter
{
	virtual ~NoOpEmitter(){}
	virtual void emit( const dynalog::Logger &, dynalog::Message && ) {}
};

int main( int, const char ** )
{
	DYNALOG_TAG( "<ExampleTag>", dynalog::Level::VERBOSE, "Performance comparison of formatting output(relative to slowest):" );
	Benchmark benchmark;

	int devnull =  open( "/dev/null", O_WRONLY );
	benchmark.measure( "dprintf('/dev/null')", [devnull]() { dprintf( devnull, "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" ); } );

	benchmark.measure( "snprintf(<internal buffer>)", []()
	{
		std::array<char,1024> buffer;
		snprintf( buffer.begin(), buffer.size(), "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" ); 
	});

	benchmark.measure( "snprintf(<internal buffer>) => write('/dev/null')", [devnull]()
	{
		std::array<char,1024> buffer;
		auto length = snprintf( buffer.begin(), buffer.size(), "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" );
		return write( devnull, buffer.begin(), length );
	});

	std::fstream stream( "/dev/null", std::ios_base::out );
	benchmark.measure( "fstream('/dev/null')", [&stream]() { stream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl << std::flush; } );

	benchmark.measure( "stringstream(<internal buffer>)", []()
	{
		std::stringstream sstream;
		sstream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl;
	});
	benchmark.measure( "stringstream(<internal buffer>) => write('/dev/null')", [devnull]()
	{
		std::stringstream sstream;
		sstream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl;
		return write( devnull, sstream.str().c_str(), sstream.str().size() );
	});
	
	std::shared_ptr<dynalog::HandleEmitter> emitter;
	emitter = std::make_shared<dynalog::HandleEmitter>( devnull );
	
	dynalog::global::setDefault( emitter.get() );
	benchmark.measure( "DynaLog('/dev/null')", callable );

	NoOpEmitter nop;
	dynalog::global::setDefault( &nop );
	benchmark.measure( "DynaLog(<NoOp>)", callable );

	dynalog::global::setDefault( nullptr );
	benchmark.measure( "DynaLog(<disabled>)", callable );
	
	auto dispatcher = std::make_shared<dynalog::async::Dispatcher>( std::chrono::milliseconds( 1 ), 
		std::chrono::seconds(10), 512, 4 );
	dispatcher->run();
	auto deferredEmitter = std::make_shared<dynalog::async::DeferredEmitter>( dispatcher, emitter.get() );

	dynalog::global::setDefault( deferredEmitter.get() );
	benchmark.measure( "DynaLog(<async>'/dev/null')", callable, [&dispatcher]
	{
		dynalog::async::Flush flush;
		dispatcher->flush( flush );
		flush.wait();
	});

	benchmark.log(std::cout);

	/*dynalog::visit( dynalog::global::configuration, []( const std::shared_ptr<dynalog::Logger> & logger )
	{
		std::cout << logger->context.value() << "\t" << logger->location.value() << std::endl;
	});*/
	return 0;
}
