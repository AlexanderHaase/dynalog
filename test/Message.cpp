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
      const auto & inspector = message.content().inspect();
			REQUIRE( inspector.size() == 3 );
			
			dynalog::Reflection reflection = inspector.reflect( 1 );
			REQUIRE( reflection.is<int>() );
			REQUIRE( reflection.as<int>() == 2 );
		}
	}
}
