#pragma once
#include <memory>

namespace dynalog {

	/// Generic buffer object suitable as backing storage.
	///
	/// Provides flexible delete semantics for caching.
	///
	class Buffer {
	public:
		size_t size() const { return capacity; }
		uint8_t * data() { return reinterpret_cast<uint8_t*>( this + 1 ); }
		const uint8_t * data() const { return reinterpret_cast<const uint8_t*>( this + 1 ); }

		struct DelegateDelete
		{
			void operator () ( Buffer * buffer ) { buffer->deleter( buffer ); }
		};

		using Pointer = std::unique_ptr<Buffer,DelegateDelete>;

		template < typename Deleter >
		static Pointer create( size_t size, Deleter && del ) 
		{
			return Pointer{ new (::operator new( size + sizeof(Buffer) ) ) Buffer{ size, std::forward<Deleter>( del ) } };
		}

		static inline Pointer create( size_t size ) { return create( size, []( Buffer * buffer ){ delete buffer; }); }

	protected:
		template < typename Deleter >
		Buffer( size_t size, Deleter && del )
		: capacity( size )
		, deleter( std::forward<Deleter>( del ) )
		{}

		const size_t capacity;
		std::function<void(Buffer*)> deleter;
	};
}
