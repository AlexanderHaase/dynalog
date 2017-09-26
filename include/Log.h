#pragma once

#include <atomic>
#include <dynalog/include/Configuration.h>
#include <dynalog/include/async/Emitter.h>

namespace dynalog {

	/// Bootstrap initializer to associate embedded loggers with default configuration.
	///
	extern Emitter * const EmbeddedLoggerInit;

	namespace global {
		extern Configuration configuration;
		extern DefaultPolicy policy;
		const int priority = std::numeric_limits<int>::min();
	}
}

// Setup token->str macro...why isn't this cstdlib?
//
#ifndef STR
#define STR_IMPL( val ) #val
#define STR( val ) STR_IMPL( val )
#endif

// Generate a location value for generated loggers.
//
#define DYNALOG_LOCATION __FILE__ ":" STR( __LINE__ )

// Define a default level mask for generated loggers.
//
#define DYNALOG_DEFAULT_LEVELS (~0)

// Instantiate an embedded logger with the given name
//
#define DYNALOG_EMBEDDED_LOGGER(name, CONTEXT) \
	::dynalog::Logger name = { { ::dynalog::EmbeddedLoggerInit }, DYNALOG_DEFAULT_LEVELS, ::dynalog::Location{ DYNALOG_LOCATION }, ::dynalog::Context{ CONTEXT } }


// Logging macro-- comma-separated objects are concatinated with ostream <<.
//
#define DYNALOG_TAG( TAG, LEVEL, ... )	\
	do {						\
		static DYNALOG_EMBEDDED_LOGGER(dynalog_generated, TAG);	\
		dynalog_generated.log( LEVEL, [&]( ::dynalog::Message & message ) {	\
			message.format( __VA_ARGS__ );	\
		});	\
	} while( false )

#define DYNALOG( LEVEL, ... )	DYNALOG_TAG( __PRETTY_FUNCTION__, LEVEL, __VA_ARGS__ )
