#include <catch.hpp>
#include <dynalog/include/async/RingBuffer.h>
#include <memory>

SCENARIO( "ring buffers should operate as fixed-capacity fifos" )
{
	GIVEN( "a ring buffer" )
	{
		const size_t capacity = 4;
		dynalog::async::RingBuffer<size_t> buffer{ capacity };

		THEN( "new buffers should appear empty" )
		{
			REQUIRE( buffer.empty() == true );
			REQUIRE( buffer.full() == false );
			REQUIRE( buffer.capacity() == capacity );
			REQUIRE( buffer.size() == 0 );
		}

		THEN( "inserting elements should change the size" )
		{
			for( size_t index = 0; index < capacity; ++index )
			{
				REQUIRE( buffer.full() == false );
				REQUIRE( buffer.size() == index );
				buffer.emplace( index );
			}
			REQUIRE( buffer.size() == buffer.capacity() );
			REQUIRE( buffer.capacity() == capacity );
			REQUIRE( buffer.full() == true );
		}

		THEN( "buffers return elements in the order inserted" )
		{
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( index );
			}
			for( size_t index = 0; index < capacity; ++index )
			{
				REQUIRE( buffer.pop() == index );
				REQUIRE( buffer.full() == false );
			}
			REQUIRE( buffer.empty() == true );
		}

		THEN( "buffers wrap around gracefully" )
		{
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( index );
			}
			buffer.pop();
			REQUIRE( buffer.full() == false );
			buffer.emplace( capacity );
			for( size_t index = 1; index <= capacity; ++index )
			{
				REQUIRE( buffer.pop() == index );
				REQUIRE( buffer.full() == false );
			}
		}

		THEN( "buffers should be movable" )
		{
			dynalog::async::RingBuffer<size_t> other{ std::move( buffer ) };
			REQUIRE( other.capacity() == capacity );
			REQUIRE( buffer.capacity() == 0 );

			buffer = std::move( other );
			REQUIRE( buffer.capacity() == capacity );
			REQUIRE( other.capacity() == 0 );
		}
	}

	GIVEN( "a ring buffer for a move-only type" )
	{
		const size_t capacity = 4;
		dynalog::async::RingBuffer<std::unique_ptr<uint8_t[]>> buffer{ capacity };

		THEN( "emplacing and poping should function as expected" )
		{
			buffer.emplace( nullptr );
			REQUIRE( buffer.pop() == nullptr );
		}
	}

	GIVEN( "a ring buffer for a move-only type" )
	{
		const size_t capacity = 4;
		dynalog::async::RingBuffer<std::unique_ptr<uint8_t[]>> buffer{ capacity };

		THEN( "emplacing and poping should function as expected" )
		{
			buffer.emplace( nullptr );
			REQUIRE( buffer.pop() == nullptr );
		}
	}

	GIVEN( "a ring buffer and a delete-counting type" )
	{
		struct DeleteCounter {
			size_t * counter = nullptr;

			DeleteCounter( size_t * counter = nullptr )
			: counter( counter ) {}

			DeleteCounter( DeleteCounter && other )
			{
				counter = other.counter;
				other.counter = nullptr;
			}

			~DeleteCounter() { if( counter ) *counter += 1; }
		};

		const size_t capacity = 4;
		dynalog::async::RingBuffer<DeleteCounter> buffer{ capacity };
		size_t counter = 0;

		THEN( "clearing buffers should remove the elements" )
		{
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			buffer.clear();
			REQUIRE( counter == capacity );
			REQUIRE( buffer.size() == 0 );
			REQUIRE( buffer.capacity() == capacity );
		}

		THEN( "releasing buffers should remove the backing storage" )
		{
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			buffer.release();
			REQUIRE( counter == capacity );
			REQUIRE( buffer.size() == 0 );
			REQUIRE( buffer.capacity() == 0 );
		}

		THEN( "looping should not produce errors" )
		{
			for( size_t index = 0; index < capacity * 10; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
				buffer.pop();
				REQUIRE( counter == index + 1 );
				REQUIRE( buffer.size() == 0 );
			}
		}

		THEN( "clearing at varioius intervals should not produce errors" )
		{
			size_t expected = 0;
			for( size_t index = 0; index < capacity * 10; ++index )
			{
				if( index % (capacity-1) == 0 )
				{
					expected += buffer.size();
					buffer.clear();
					REQUIRE( counter == expected );
				}
				REQUIRE( buffer.full() == false );
				buffer.emplace( DeleteCounter{ &counter } );
			}
		}

		THEN( "moving buffers should not delete items" )
		{
			for( size_t index = 0; index < capacity/2; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			dynalog::async::RingBuffer<DeleteCounter> other{ std::move( buffer ) };
			REQUIRE( counter == 0 );
		}

		THEN( "destroying buffers should destroy items" )
		{
			size_t expected = capacity/2;
			for( size_t index = 0; index < expected; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			{
				dynalog::async::RingBuffer<DeleteCounter> other{ std::move( buffer ) };
			}
			REQUIRE( counter == expected );
		}

		THEN( "reshaping buffers smaller should destroy items" )
		{
			size_t expected = capacity/2;
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			buffer.reshape( capacity - expected );
			REQUIRE( counter == expected );
		}

		THEN( "reshaping buffers larger should not destroy items" )
		{
			size_t expected = 0;
			for( size_t index = 0; index < capacity; ++index )
			{
				buffer.emplace( DeleteCounter{ &counter } );
			}
			buffer.reshape( capacity * 2 );
			REQUIRE( counter == expected );
		}
	}
}
