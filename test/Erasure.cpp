#include <catch.hpp>
#include <atomic>
#include <dynalog/include/Erasure.h>

SCENARIO( "erasures should provide value semantics for captured objects" )
{
	GIVEN( "an erasure a small object, and a large object" )
	{
    using SmallType = intptr_t;
    using LargeType = std::array<SmallType,2>;
    using Erasure = dynalog::Erasure<sizeof(SmallType)>;
		Erasure erasure;

    struct {
      const SmallType small = 2;
      const LargeType large{ { 3, 4 } };

      void check_small( const Erasure & erasure )
      {
        REQUIRE( erasure.as<SmallType>() == small );
        REQUIRE( erasure.location() == Erasure::Location::Internal );

        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<SmallType>() );
        REQUIRE( reflection.as<SmallType>() == small );
      }

      void check_large( const Erasure & erasure )
      {
        REQUIRE( erasure.as<LargeType>() == large );
        REQUIRE( erasure.location() == Erasure::Location::External );

        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<LargeType>() );
        REQUIRE( reflection.as<LargeType>() == large );
      }

      void check_empty( const Erasure & erasure )
      {
        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<nullptr_t>() );
        REQUIRE( erasure.location() == Erasure::Location::Empty );
      }

    } values;

    THEN( "erasures should begin empty" )
    {
      REQUIRE( dynalog::is_erasure<decltype(erasure)>::value );
			const auto reflection = erasure.reflect();
      REQUIRE( reflection.is<nullptr_t>() );
      REQUIRE( erasure.location() == Erasure::Location::Empty );
    }

		THEN( "erasure should capture the small object internally" )
		{
      erasure = values.small;
      values.check_small( erasure );
		}

    THEN( "erasure should capture the large object externally" )
    {
      erasure = values.large;
      values.check_large( erasure );
    }

    THEN( "erasures should be able to change contents" )
    {
      erasure = values.small;
      erasure = values.large;
      values.check_large( erasure );
    }

    THEN( "erasures should be able to clear contents" )
    {
      erasure = values.small;
      erasure = nullptr;
      values.check_empty( erasure );
    }

    THEN( "erasures should be constructable from values" )
    {
      const auto internal = Erasure{ values.small };
      const auto external = Erasure{ values.large };
      const auto empty = Erasure{ nullptr };
      values.check_small( internal );
      values.check_large( external );
      values.check_empty( empty );
    }

    THEN( "erasures should external-move large contents" )
    {
      auto initial = Erasure{ values.large };
      auto a = initial.reflect();

      erasure = std::move( initial );
      auto b = erasure.reflect();
      REQUIRE( a.type() == b.type() );
      REQUIRE( &a.as<SmallType>() == &b.as<SmallType>() );
      REQUIRE( a.as<SmallType>() == b.as<SmallType>() );
    }

    THEN( "erasures should internal-move small contents" )
    {
      auto initial = Erasure{ values.small };
      auto a = initial.reflect();

      erasure = std::move( initial );
      auto b = erasure.reflect();
      REQUIRE( a.type() == b.type() );
      REQUIRE( &a.as<SmallType>() != &b.as<SmallType>() );
      REQUIRE( a.as<SmallType>() == b.as<SmallType>() );
    }

    THEN( "erasures should external-copy large contents" )
    {
      auto initial = Erasure{ values.large };
      auto a = initial.reflect();

      erasure = initial;
      auto b = erasure.reflect();
        REQUIRE( a.type() == b.type() );
      REQUIRE( &a.as<SmallType>() != &b.as<SmallType>() );
      REQUIRE( a.as<SmallType>() == b.as<SmallType>() );
    }

    THEN( "erasures should internal-copy small contents" )
    {
      auto initial = Erasure{ values.small };
      auto a = initial.reflect();

      erasure = initial;
      auto b = erasure.reflect();
      REQUIRE( a.type() == b.type() );
      REQUIRE( &a.as<SmallType>() != &b.as<SmallType>() );
      REQUIRE( a.as<SmallType>() == b.as<SmallType>() );
    }
	}

  GIVEN( "two erasures of different size" )
  {
    using SmallType = intptr_t;
    using LargeType = std::array<SmallType,2>;
    using SmallErasure = dynalog::Erasure<sizeof(SmallType)>;
    using LargeErasure = dynalog::Erasure<sizeof(LargeType)>;
    using dynalog::details::BasicErasure;

		struct {
      SmallErasure small;
      LargeErasure large;
    } erasures;

    struct {
      const SmallType small = 2;
      const LargeType large{ { 3, 4 } };


      void check_small( const BasicErasure & erasure )
      {
        REQUIRE( erasure.as<SmallType>() == small );

        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<SmallType>() );
        REQUIRE( reflection.as<SmallType>() == small );
      }

      void check_large( const BasicErasure & erasure )
      {
        REQUIRE( erasure.as<LargeType>() == large );

        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<LargeType>() );
        REQUIRE( reflection.as<LargeType>() == large );
      }

      void check_empty( const BasicErasure & erasure )
      {
        const auto reflection = erasure.reflect();
        REQUIRE( reflection.is<nullptr_t>() );
        REQUIRE( erasure.location() == BasicErasure::Location::Empty );
      }

    } values;

    THEN( "values should convert from internal to external as appropriate" )
    {
      erasures.large = values.large;
      erasures.small = erasures.large;
      REQUIRE( erasures.small.location() == BasicErasure::Location::External );
      erasures.small = std::move( erasures.large );
      REQUIRE( erasures.small.location() == BasicErasure::Location::External );

      erasures.large = erasures.small;
      REQUIRE( erasures.large.location() == BasicErasure::Location::Internal );
      erasures.large = std::move( erasures.small );
      REQUIRE( erasures.large.location() == BasicErasure::Location::External );
    }

    THEN( "erasures should throw trying to copy non-copyable values" )
    {
      erasures.small.emplace<std::unique_ptr<int>>( new int{ 5 } );
      bool threw = false;
      try {
        erasures.large = erasures.small;
      } catch( dynalog::ErasureException e )
      {
        threw = true;
      }
      REQUIRE( threw );
      erasures.large = std::move( erasures.small );
    }

    THEN( "erasures should throw trying to move non-movable values" )
    {
      erasures.small.emplace<std::atomic<int>>( 0 );
      bool threw = false;
      try {
        erasures.large = std::move( erasures.small );
      } catch( dynalog::ErasureException e )
      {
        threw = true;
      }
      REQUIRE( threw );
    }
  }
}
