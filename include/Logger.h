#pragma once

#include <bitset>
#include <memory>
#include <atomic>
#include <dynalog/include/Core.h>
#include <dynalog/include/Message.h>

namespace dynalog {

	// Forward-declare logger for emitter.
	//
	struct Logger;

	/// Emitters route/absorb/terminate messages.
	///
	/// Emitters may discard/modify messages, perform complex logic, etc.
	/// -- fast-path filtering is done at the logger level. In concurrent
	/// applications, emitters should be deleted with care--loggers 
	/// preparing messages may carry a stacked reference!
	///
	struct Emitter
	{
		virtual ~Emitter() {}

		/// Receive a message.
		///
		/// @param logger Source of the message.
		/// @param message Formatted message body to process.
		///
		virtual void emit( const Logger & logger, Message && message ) = 0;
	};

	/// Loggers describe logging rules for a specific location.
	///
	/// Specifically, loggers provide FAST-PATH filtering behavior:
	///   - Very fast bypass of logging if disabled.
	///   - Statically initializable without context guard.
	///
	/// Great care should be taken modifying the structure--see 
	/// invocation_comparision.cpp for exploration of side effects.
	///
	/// With respect to the fast-path, a logger is enabled if emitter is
	/// non-null.
	///
	/// TODO: investigate constexpr initializer WRT context gaurd: classful
	/// access semantics might be permissible. 
	///
	struct Logger
	{
		std::atomic<Emitter*> emitter;	///< Enabled if non-null.
		LevelSet levels;                ///< Mask of enabled levels.
		Location location;	            ///< Unique indentifier for logger.
		Context context;	              ///< Group for logger.
		Tag tag;		                    ///< Tag for logger.

		/// Fast conditional logging method.
		///
		/// Compiler optimizations readily inline the nested calls into
		/// linear logic. The overall structure regularly obviates
		/// argument passing overhead(at least when used with lambdas).
		/// See Message for formatting operations.
		///
		/// @tparam MessageBuilder Functor object accepting a message reference.
		/// param builder R- or L-reference to builder invoked if logger enabled.
		///
		template < typename MessageBuilder >
		void log( const Level level, MessageBuilder && builder )
		{
			const auto destination = emitter.load( std::memory_order_relaxed );
			if( destination && levels.get( level ) )
			{
				Message message;
				builder( message );
				destination->emit( *this, std::move( message ) );
			}
		}
	};
}
