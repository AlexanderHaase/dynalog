#include <catch.hpp>
#include <dynalog/include/Log.h>
#include <sstream>
#include <iostream>

#include <cstring>

struct Test
{
  static constexpr const char * TAG = "BootstrapTest";
  static void emit_tagged_message()
  {
    DYNALOG_TAG( TAG, dynalog::Level::INFO, dynalog::Level::INFO, "ignored text" );
  }
  static void emit_untagged_message()
  {
    DYNALOG_TAG( TAG, dynalog::Level::INFO, "ignored text" );
  }
};

SCENARIO( "bootstrap logger should prevent logging if level is in message body" )
{
	GIVEN( "a policy and a bootstrapped call" )
	{
    struct TestEmitter : dynalog::Emitter
    {
      size_t count = 0;
      virtual void emit( const dynalog::Logger &, dynalog::Message && ) override
      {
        count += 1;
      }
    };

    const auto emitter = std::make_shared<TestEmitter>();
    const auto policy = dynalog::make_policy( emitter.get(), dynalog::LevelSet{ 0 },
      []( const std::shared_ptr<dynalog::Logger> & logger )
      {
        return 0 == ::strcmp( logger->tag.value(), Test::TAG );
      });

    const auto priority = 0;

    THEN( "calls should be supressible ahead of time" )
    {
      REQUIRE( dynalog::global::configuration.insert( priority, policy ) );
      Test::emit_tagged_message();
      REQUIRE( emitter->count == 0 );
      Test::emit_untagged_message();
      REQUIRE( emitter->count == 1 );
      REQUIRE( dynalog::global::configuration.remove( priority, policy ) );
    }
	}
}
