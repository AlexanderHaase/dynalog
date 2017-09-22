#pragma once
#include <memory>
#include <dynalog/include/Buffer.h>

namespace dynalog {

	/// Re-usable buffer for instantiating objects of varying size.
	///
	/// ObjectBuffers can be used to in-place construct objects of varying
	/// size, conceptually in-conjunction with an object-buffer caching 
	/// scheme. ObjectBuffers safely deconstruct the stored object and
	/// free internal storage at their own destruction. Resizing or
	/// emplacing
	///
	/// Future: consider expansion to a generational allocator for multiple objects.
	///
	class ObjectBuffer {
	public:
		/// Resize the object buffer, destroying any stored object.
		///
		/// @param size intended capacity of the object buffer.
		///
		inline void resize( size_t size )
		{
			clear();
			data = std::unique_ptr<uint8_t[]>( new uint8_t[ size ] );
			capacity = size;
		}

		/// Destroy the contained object, if any.
		///
		inline void clear( void )
		{
			if( ! empty() ) {
				destructor( data.get() );
				destructor = nullptr;
				info = nullptr;
			}
		}

		/// Access the type_info of the contained object, if any.
		///
		/// @return std::type_info for the object or nullptr if empty.
		///
		inline const std::type_info * type( void ) const { return info; };

		/// Check if the buffer is in use.
		///
		/// @return true if buffer is occupied, false otherwise.
		///
		inline bool empty( void ) const { return destructor == nullptr; }

		/// Invoke a Type's destructor on an arbitrary pointer.
		///
		/// @tparam Type Type for which to invoke destructor.
		/// @param pointer Pointer to apply destructor to.
		///
		template < typename Type >
		static void call_destructor( void * pointer )
		{
			reinterpret_cast<Type*>( pointer )->~Type();
		}

		/// Create an object in the buffer.
		///
		/// clears and resizes the buffer if appropriate.
		///
		/// @tparam Type The type of object to construct.
		/// @tparam Args... Type pack for instantiating the object.
		/// @param args... Parameter pack for instantiation the object.
		/// @return Reference to the instantiated object.
		///
		template < typename Type, typename ... Args >
		Type & emplace( Args && ...args )
		{
			if( sizeof(Type) > capacity )
			{
				resize( sizeof(Type) );
			}
			else
			{
				clear();
			}
			destructor = &call_destructor<Type>;
			info = &typeid(Type);
			return *(new (data.get()) Type( std::forward<Args>( args )... ));
		}

		/// Access the buffer as an instance of Type.
		///
		/// @tparam Type Presumed type of stored object--NOT checked!
		/// @return Reference to the stored object.
		/// 
		template < typename Type >
		Type & as( void ) { return *reinterpret_cast<Type*>( data.get() ); }

		/// Const access the buffer as an instance of Type.
		///
		/// @tparam Type Presumed type of stored object--NOT checked!
		/// @return Reference to the stored object.
		/// 
		template < typename Type >
		const Type & as( void ) const { return *reinterpret_cast<const Type*>( data.get() ); }

		inline ~ObjectBuffer() { clear(); }

		/// Keep default constructor.
		///
		constexpr ObjectBuffer() = default;

		/// Construct from a buffer.
		///
		/// @param buffer Unique pointer to data area.
		/// @param size Capacity of the data area.
		///
		inline ObjectBuffer( std::unique_ptr<uint8_t[]> && buffer, size_t size )
		: data( std::move( buffer ) )
		, capacity( size )
		{}

		/// Replace the internal buffer(destructs stored object).
		///
		/// @param buffer Unique pointer to data area.
		/// @param size Capacity of the data area.
		///
		inline void resize( std::unique_ptr<uint8_t[]> && buffer, size_t size )
		{
			clear();
			data = std::move( buffer );
			capacity = size;
		}

	protected:
		const std::type_info * info = nullptr;
		void (* destructor)(void *) = nullptr;
		std::unique_ptr<uint8_t[]> data;
		size_t capacity = 0;
	};
}
