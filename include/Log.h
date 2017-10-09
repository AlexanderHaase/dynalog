#pragma once

#include <atomic>
#include <dynalog/include/Configuration.h>
#include <dynalog/include/async/Emitter.h>

namespace dynalog {

	/// Bootstrap initializer to associate embedded loggers with global configuration.
	///
	extern Emitter * const EmbeddedLoggerInit;

	namespace global {
		extern Configuration configuration;
		void setDefault( Emitter * emitter );
		void setDefault( const LevelSet & levels );
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

// Generate a context value for generated loggers.
//
#define DYNALOG_CONTEXT __PRETTY_FUNCTION__

// Define a default level mask for generated loggers.
//
#define DYNALOG_DEFAULT_LEVELS (~0u)

// Instantiate an embedded logger with the given name
//
#define DYNALOG_EMBEDDED_LOGGER(name, TAG) \
	::dynalog::Logger name = { { ::dynalog::EmbeddedLoggerInit }, DYNALOG_DEFAULT_LEVELS, ::dynalog::Location{ DYNALOG_LOCATION }, ::dynalog::Context{ DYNALOG_CONTEXT }, ::dynalog::Tag{ TAG } }


// Logging macro-- comma-separated objects are concatinated with ostream <<.
//
#define DYNALOG_TAG( TAG, LEVEL, ... )	\
	do {						\
		static DYNALOG_EMBEDDED_LOGGER(dynalog_generated, TAG);	\
		dynalog_generated.log( LEVEL, [&]( ::dynalog::Message & message ) {	\
			message.format( __VA_ARGS__ );	\
		});	\
	} while( false )

#define DYNALOG( LEVEL, ... )	DYNALOG_TAG( "<untagged>", LEVEL, __VA_ARGS__ )
