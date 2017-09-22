#pragma once
#include <memory>
#include <mutex>
#include <vector>

namespace dynalog {

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
			uint8_t * data = new uint8_t[ size + sizeof(Buffer) ];
			return Pointer{ new (data) Buffer{ size, std::forward<Deleter>( del ) } };
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

	template < typename Func >
	auto with( std::mutex & mutex, Func && func ) -> decltype( func() )
	{
		std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
		return func();
	}	

	class Cache {
	public:

		Buffer::Pointer remove( size_t size )
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
			
		void insert( Buffer * buffer )
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

		Cache( size_t size, size_t qty )
		: capacity( size )
		, index( 0 )
		{ slots.resize( qty ); }

	protected:
		std::mutex mutex;
		const size_t capacity;
		size_t index;
		std::vector<Buffer*> slots;
	};

}
