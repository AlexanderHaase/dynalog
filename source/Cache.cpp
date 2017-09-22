#include <dynalog/include/Cache.h>

namespace dynalog {

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
			delete buffer;
		}
	}
}
