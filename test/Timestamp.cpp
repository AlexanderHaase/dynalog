#include <catch.hpp>
#include <dynalog/include/Timestamp.h>
#include <sstream>
#include <iostream>


SCENARIO( "atoms should readily convert to/from time_points" )
{
	GIVEN( "a time_point" )
	{
		const auto time = std::chrono::system_clock::now();

		THEN( "Atoms of the time should be equivalent to the original time" )
		{
			dynalog::timestamp::Atoms atoms{ time };
			const auto reconverted = static_cast<std::chrono::system_clock::time_point>( atoms );
			auto delta = time - reconverted;
			REQUIRE( delta.count() == 0 );
		}
	}
}
