#include <catch.hpp>
#include <dynalog/include/Message.h>
#include <sstream>
#include <iostream>

SCENARIO( "messages should function as portable erasures" )
{
	GIVEN( "a message and an ostream" )
	{
		dynalog::Message message;
		std::stringstream stream;

		THEN( "messages should create streamable erasures" )
		{
			message.format( "Hello world, here is a number: ", 1, " and a string continuation" );
			message.content().serialize( stream );
			REQUIRE( stream.str() == "Hello world, here is a number: 1 and a string continuation" );
		}
		
		THEN( "messages should be reflectable" )
		{
			message.format( std::string( "hi" ), 2, 0.1 );
			REQUIRE( message.content().size() == 3 );
			
			dynalog::Message::Content::Reflection reflection = message.content().reflect( 1 );
			REQUIRE( reflection.info == &typeid(int) );
			REQUIRE( *reinterpret_cast<const int*>( reflection.object ) == 2 );
		}
	}
}
