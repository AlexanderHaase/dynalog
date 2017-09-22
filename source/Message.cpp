#include <thread>
#include <dynalog/include/Message.h>
#include <dynalog/include/Cache.h>

namespace dynalog {

	/// Global cache--initialize two buckets per CPU.
	///
	static struct {

		std::mutex mutex;	///< Protect initialization.
		std::vector<std::unique_ptr<Cache> > caches;	///< Caches, lookup by hash of thread id.
		size_t qty = 0;	///< Indicator of initialization.

		void init( void )
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			if( qty == 0 )
			{
				qty = 2 * std::thread::hardware_concurrency();
				while( caches.size() < qty )
				{
					caches.emplace_back( new Cache{ 4096 - sizeof(Buffer), 128 } );
				};
			}
		}

		Buffer::Pointer acquire( size_t size )
		{
			while( qty == 0 )
			{
				init();
			}
			const auto id = std::this_thread::get_id();
			const auto hash = std::hash<std::thread::id>{}( id );
			const auto index = hash % qty;
		
			return caches[ index ]->remove( size );
		}
	} cache;

	Buffer::Pointer Message::cached( size_t size )
	{
		return cache.acquire( size );
	}
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
