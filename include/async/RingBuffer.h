#pragma once

namespace dynalog { namespace async {

	/// Fixed-capacity ring buffer implementation.
	///
	/// Based around move operations.
	///
	template <typename T>
	class RingBuffer {
	public:
		/// Place a new element in the buffer.
		///
		template < typename ...Args >
		void emplace( Args && ... args )
		{
			new( slots.end ) T{ std::forward<Args>( args )... };
			slots.advance( slots.end );
			slots.size += 1;
		}

		/// Remove and return the oldest element.
		///
		/// @return popped value.
		///
		T pop( void )
		{
			T result{ std::move( *slots.begin ) };
			slots.begin->~T();
			slots.advance( slots.begin );
			slots.size -= 1;
			return result;
		}

		/// Check if buffer is empty.
		///
		bool empty( void ) const { return slots.size == 0; }

		/// Check if buffer is full.
		///
		bool full( void ) const { return slots.size == slots.capacity; }

		/// Check number of elements available.
		///
		size_t size( void ) const { return slots.size; }

		/// Check buffer element capacity.
		///
		size_t capacity( void ) const { return slots.capacity; }

		template< typename Predicate >
		size_t erase( Predicate && predicate )
		{
			const auto limit = size();
			for( size_t index = 0; index < limit; ++index )
			{
				auto value = pop();
				if( !predicate( value ) )
				{
					emplace( std::move( value ) );
				}
			}
			return limit - size();
			/*auto ptr = slots.begin;
			for( size_t index = 0; index++ < size(); slots.advance( ptr ) )
			{
				if( predicate( *ptr ) )
				{
					slots.remove( ptr );
					return true;
				}
			}
			return false;*/
		}

		/// Change the capacity of the backing buffer.
		///
		/// Drops newer elements to fit.
		///
		void reshape( size_t capacity )
		{
			RingBuffer other{ capacity };
			while( ! empty() && ! other.full() )
			{
				other.emplace( pop() );
			}
			slots.release();
			*this = std::move( other );
		}

		/// Create a buffer with the specified capacity;
		///
		/// @param capacity Number of elements to allocate storage for.
		///
		RingBuffer( size_t capacity )
		{
			slots.init( reinterpret_cast<T*>( ::operator new( sizeof(T) * capacity ) ),
				capacity );
		}

		/// Create an empty RingBuffer
		///
		RingBuffer( void )
		{
			slots.init( nullptr, 0 );
		}
		
		/// Move construct a RingBuffer
		///
		RingBuffer( RingBuffer && other )
		{
			slots = other.slots;
			other.slots.init( nullptr, 0 );
		}

		/// Move assign a RingBuffer
		///
		RingBuffer & operator = ( RingBuffer && other )
		{
			release();
			slots = other.slots;
			other.slots.init( nullptr, 0 );
			return *this;
		}

		//template < typename = decltype( declref<T>() = declref<const T>()>
		RingBuffer( const RingBuffer & other ) = delete;
		RingBuffer & operator = ( const RingBuffer & other ) = delete;

		/// Clear contents of the buffer.
		///
		void clear( void ) { slots.clear(); }

		/// Release backing storage.
		///
		void release( void ) { slots.release(); }

	protected:
		struct Slots {
			T * storage;
			T * begin;
			T * end;
			size_t size;
			size_t capacity;

			void advance( T*& ptr, const size_t count = 1 ) const
			{
				if( (ptr += count) >= (storage + capacity) )
				{
					ptr -= capacity;
				}
			}

			void rewind( T*& ptr, const size_t count = 1 ) const
			{
				if( (ptr -= count) < storage )
				{
					ptr += capacity;
				}
			}

			void remove( T * ptr )
			{
				auto src = ptr;
				if( ptr - begin > end - ptr )
				{
					for( advance( src ); src != end; advance( src ) )
					{
						*ptr = std::move( *src );
						ptr = src;
					}
					end = ptr;
				}
				else
				{
					for( rewind( src ); ptr != begin; rewind( src ) )
					{
						*ptr = std::move( *src );
						ptr = src;
					}
					advance( begin );
				}
				src->~T();
				size -= 1;
			}
					
			void init( T * ptr, size_t count )
			{
				storage = ptr;
				begin = ptr;
				end = ptr;
				size = 0;
				capacity = count;
			}

			void clear( void )
			{
				while( size )
				{
					begin->~T();
					advance( begin );
					size -= 1;
				}
			}

			void release( void )
			{
				clear();
				::operator delete( storage );
				storage = nullptr;
				size = 0;
				capacity = 0;
			}

			~Slots( void )
			{
				release();
			}
		} slots;
	};
} }
