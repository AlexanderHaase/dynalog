#include <memory>
#include <array>
#include <mutex>
#include <thread>
#include <dynalog/include/Message.h>

/*
template<size_t Capacity, size_t Qty>
class BufferCache {
public:
	std::unique_ptr<uint8_t[]> acquire( void )
	{
		uint8_t * result;
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			result = size ?  buffers[ --size ] : nullptr;
		}
		if( ! result )
		{
			result = new uint8_t[ Capacity ];
		}
		return std::unique_ptr<uint8_t[]>{ result, [&]( uint8_t * ptr ){ release( ptr ); } };
	}

protected:
	void release( uint8_t * ptr )
	{
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			if( size < buffers.size() )
			{
				buffers[ size++ ] = ptr;
				ptr = nullptr;
			}
		}
		if( ptr )
		{
			delete[] ptr;
		}
	}

	std::mutex mutex;
	size_t size = 0;
	std::array<uint8_t*, Qty> buffers;
};

static struct {
	std::mutex mutex;
	BufferCache<4096,128> * caches = nullptr;
	size_t qty;

	void init( void )
	{
		std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
		if( caches == nullptr )
		{
			qty = 2 * std::thread::hardware_concurrency();
			caches = new BufferCache<4096,128>[ qty ];
		}
	}

	std::unique_ptr<uint8_t[]> acquire()
	{
		while( caches == nullptr )
		{
			init();
		}
		const auto id = std::this_thread::get_id();
		const auto hash = std::hash<std::thread::id>{}( id );
		const auto index = hash % qty;
		
		return caches[ index ].acquire();
	}
} globalCache;*/

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
