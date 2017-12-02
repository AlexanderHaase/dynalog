#include <catch.hpp>
#include <dynalog/include/Erasure.h>
#include <sstream>
#include <iostream>


SCENARIO( "erasures should capture objects" )
{
	GIVEN( "an erasure a small object, and a large object" )
	{

    using SmallType = intptr_t;
    using LargeType = std::array<SmallType,2>;
    using Erasure = dynalog::Erasure<sizeof(SmallType)>;
		Erasure erasure;
    const SmallType small_value = 2;
    const LargeType large_value{ { 3, 4 } };

    THEN( "erasures should begin empty" )
    {
			const auto reflection = erasure.reflect();
      REQUIRE( reflection.is<nullptr_t>() );
      REQUIRE( erasure.location() == Erasure::Location::Empty );
    }

		THEN( "erasure should capture the small object internally" )
		{
      erasure = small_value;
      REQUIRE( erasure.as<SmallType>() == small_value );
      REQUIRE( erasure.location() == Erasure::Location::Internal );

      const auto reflection = erasure.reflect();
      REQUIRE( reflection.is<SmallType>() );
      REQUIRE( reflection.as<SmallType>() == small_value );
		}

    THEN( "erasure should capture the large object externally" )
    {
      erasure = large_value;
      REQUIRE( erasure.as<LargeType>() == large_value );
      REQUIRE( erasure.location() == Erasure::Location::External );

      const auto reflection = erasure.reflect();
      REQUIRE( reflection.is<LargeType>() );
      REQUIRE( reflection.as<LargeType>() == large_value );
    }
	}
}
