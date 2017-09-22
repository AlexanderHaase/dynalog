#pragma once

#include <mutex>
#include <vector>
#include <dynalog/include/Buffer.h>

namespace dynalog {

	/// Scope wrapper for mutex.
	///
	/// Usage: with( mutex, <lambda> ) -> <lambda result>
	///
	/// @tparam Func function to apply in mutex scope.
	/// @param mutex Protect func invoctation.
	/// @param func Invoke with mutex locked.
	/// @return Result of function.
	///
	template < typename Func >
	auto with( std::mutex & mutex, Func && func ) -> decltype( func() )
	{
		std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
		return func();
	}	

	/// Cache for buffers of a fixed size.
	///
	/// Holds upto a fixed capacity of buffers of a fixed size.
	///
	class Cache {
	public:
		/// Get-or-create a buffer with the appropriate size.
		///
		/// Creates a buffer if the size is too large or cache is empty.
		///
		/// @param size Requested buffer size.
		/// @return new or cached buffer.
		///
		Buffer::Pointer remove( size_t size );

		/// Determine if the cache natively supports the requested size.
		///
		bool supports( size_t size ) const { return capacity >= size; }

		/// Create a new cache
		///
		/// @param size Capacity of buffers to create.
		/// @param qty Number of buffers to cache.
		///
		inline Cache( size_t size, size_t qty )
		: capacity( size )
		, index( 0 )
		{ slots.resize( qty ); }

		~Cache()
		{
			while( index ) { delete slots[ --index ]; }
		}

	protected:
		/// Return a buffer to the cache
		///
		void insert( Buffer * buffer );

		std::mutex mutex;
		const size_t capacity;	///< Create buffers of this size.
		size_t index;	///< Next free slot.
		std::vector<Buffer*> slots;	///< Cached pointers.
	};
}
