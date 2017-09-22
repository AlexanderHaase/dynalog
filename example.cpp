#include <dynalog/include/Log.h>
#include <dynalog/include/HandleEmitter.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

template < size_t Iterations = 100000000, typename Callable >
double usecPerCall( Callable && callable )
{
	struct timeval begin, end;
	gettimeofday( &begin, nullptr );

	for( size_t index = 0; index < Iterations; ++index )
	{
		callable();
	}

	gettimeofday( &end, nullptr );

	const auto elapsed = ( end.tv_sec - begin.tv_sec ) * 1000000 + end.tv_usec - begin.tv_usec;

	return double(elapsed)/double(Iterations);
}


void callable( void )
{
	DYNALOG( "MAIN", dynalog::Level::VERBOSE, "inside callable" );
}


int main( int argc, const char ** argv )
{
	DYNALOG( "MAIN", dynalog::Level::VERBOSE, "Hello there!" );

	std::shared_ptr<dynalog::HandleEmitter> emitter;
	if( argc %2 )
	{
		emitter = std::make_shared<dynalog::HandleEmitter>( open( "/dev/null", O_WRONLY ) );
	}
	dynalog::global::policy.configure( emitter.get() );
	dynalog::global::configuration.rescan( dynalog::global::priority );

	std::cout << usecPerCall( callable ) << std::endl;
}
