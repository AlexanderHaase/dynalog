#include <dynalog/include/Cache.h>

namespace dynalog {

	/// Create a new cache
	///
	/// @param size Capacity of buffers to create.
	/// @param qty Number of buffers to cache.
	///
	Cache::Cache( size_t size, size_t qty )
	: capacity( size )
	, index( 0 )
	{ slots.resize( qty ); }

	Cache::~Cache()
	{
		while( index ) { Buffer::destroy( slots[ --index ] ); }
	}

	/// Get-or-create a buffer with the appropriate size.
	///
	/// Creates a buffer if the size is too large or cache is empty.
	///
	/// @param size Requested buffer size.
	/// @return new or cached buffer.
	///
	Buffer::Pointer Cache::remove( size_t size )
	{
		Buffer::Pointer result;

		if( size > capacity )
		{
			result =  Buffer::create( size );
		}
		else
		{
			result = with( mutex, [this]{ return Buffer::Pointer{ index ? slots[ --index ] : nullptr }; });
			if( result == nullptr )
			{
				result = Buffer::create( capacity, [this](Buffer * buffer) { insert( buffer ); } );
			}
		}
		return result;
	}

	/// Return a buffer to the cache
	///
	void Cache::insert( Buffer * buffer )
	{
		const bool cached = with( mutex, [&]
		{
			const bool result = ( index < slots.size() );
			if( result )
			{
				slots[ index++ ] = buffer;
			}
			return result;
		});
		if( !cached )
		{
			Buffer::destroy( buffer );
		}
	}
}
