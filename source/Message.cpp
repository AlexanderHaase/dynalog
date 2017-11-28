#include <thread>
#include <dynalog/include/Message.h>
//#include <dynalog/include/Cache.h>
#include <dynalog/include/async/Replicated.h>

namespace dynalog {

  /*
	/// Global cache--initialize two buckets per CPU.
	///
	static async::Replicated<Cache> caches{ std::make_tuple( 4096 - sizeof(Buffer), 128 ) };

	Buffer::Pointer Message::cached( size_t size )
	{
		return caches.unprotected( [size]( Cache & cache ) { return cache.remove( size ); } );
	}
  */
}

::std::ostream & operator << ( ::std::ostream & stream, const ::dynalog::Message & message )
{
	if( message.empty() )
	{
		stream << "<Empty ::dynalog::Message@" << &message << ">";
	}
	else
	{ 
		message.content().serialize( stream );
	}
	return stream;
}
