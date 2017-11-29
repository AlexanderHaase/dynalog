#include <dynalog/include/Log.h>
#include <dynalog/include/Benchmark.h>
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
#include <fstream>

struct Callable
{
	void operator()() const {
		DYNALOG( dynalog::Level::VERBOSE, "MAIN", dynalog::Level::VERBOSE, "inside callable" );
	}
} callable;

struct NoOpEmitter : dynalog::Emitter {
  virtual ~NoOpEmitter() = default;
	virtual void emit( const dynalog::Logger &, const dynalog::Message & ) override {}
};

int main( int argc, const char ** argv )
{
	dynalog::Benchmark benchmark;

	DYNALOG_TAG( "<ExampleTag>", dynalog::Level::VERBOSE, "Performance comparison of formatting output(relative to slowest):" );
	//Benchmark benchmark;

	int devnull =	open( "/dev/null", O_WRONLY );
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
		std::chrono::seconds(10), 512, 2 );
	dispatcher->run();
	auto deferredEmitter = std::make_shared<dynalog::async::DeferredEmitter>( dispatcher, emitter.get() );
	dynalog::global::setDefault( deferredEmitter.get() );

  const auto sync = [&dispatcher]
	{
		dynalog::async::Flush flush;
		dispatcher->flush( flush );
		flush.wait();
	};

  sync();

	benchmark.measure( "DynaLog(<async>'/dev/null')", callable, sync );

	benchmark.summary(std::cout);

	if( argc > 1 )
	{
		std::ofstream file{ argv[ 1 ] };
		benchmark.json( file );
	} 
	//benchmark.log(std::cout);

	/*dynalog::visit( dynalog::global::configuration, []( const std::shared_ptr<dynalog::Logger> & logger )
	{
		std::cout << logger->context.value() << "\t" << logger->location.value() << std::endl;
	});*/
	return 0;
}
