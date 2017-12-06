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
  virtual void emit( const dynalog::Logger &, dynalog::Message && ) override {}
};

template < typename Last >
std::ostream & cat_impl( std::ostream & stream, Last && last )
{
  return stream << last;
}

template < typename Current, typename ...Remainder >
std::ostream & cat_impl( std::ostream & stream, Current && current, Remainder && ...remainder )
{
  stream << current;
  return cat_impl( stream, std::forward<Remainder>( remainder )... );
}

template < typename ...Args >
std::string cat( Args && ...args )
{
  std::ostringstream stream;
  cat_impl( stream, std::forward<Args>( args )... );
  return stream.str();
}

int main( int argc, const char ** argv )
{
  const std::string path = argc > 2 ? argv[ 2 ] : "/dev/null";
  //dynalog::Benchmark<std::chrono::high_resolution_clock> benchmark;
  dynalog::Benchmark<std::chrono::steady_clock> benchmark;

  DYNALOG_TAG( "<ExampleTag>", dynalog::Level::VERBOSE, "Performance comparison of formatting output:" );

  int fd =  open( path.c_str(), O_WRONLY | O_CREAT, 0666 );
  benchmark.measure( cat( "dprintf('", path, "')"), [fd]() { dprintf( fd, "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" ); } );

  benchmark.measure( "snprintf(<internal buffer>)", []()
  {
    std::array<char,1024> buffer;
    snprintf( buffer.begin(), buffer.size(), "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" ); 
  });

  benchmark.measure( cat("snprintf(<internal buffer>) => write('",path,"')"), [fd]()
  {
    std::array<char,1024> buffer;
    auto length = snprintf( buffer.begin(), buffer.size(), "%s%s%s\n", "MAIN", "VERBOSE", "inside callable" );
    return write( fd, buffer.begin(), length );
  });

  std::fstream stream( path, std::ios_base::out );
  benchmark.measure( cat("fstream('", path, "')" ), [&stream]() { stream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl << std::flush; } );

  benchmark.measure( "stringstream(<internal buffer>)", []()
  {
    std::stringstream sstream;
    sstream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl;
  });
  benchmark.measure( cat("stringstream(<internal buffer>) => write('", path, "')"), [fd]()
  {
    std::stringstream sstream;
    sstream << "MAIN" << dynalog::Level::VERBOSE << "inside callable" << std::endl;
    return write( fd, sstream.str().c_str(), sstream.str().size() );
  });
  
  std::shared_ptr<dynalog::HandleEmitter> emitter;
  emitter = std::make_shared<dynalog::HandleEmitter>( fd );
  
  dynalog::global::setDefault( emitter.get() );
  benchmark.measure( cat("DynaLog('", path, "')" ), callable );

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

  benchmark.measure( cat("DynaLog(<async>'", path, "')"), callable, sync );

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
