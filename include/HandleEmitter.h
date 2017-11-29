#pragma once

#include <dynalog/include/Logger.h>

namespace dynalog {


	/// Emit to a file descriptor
	///
	class HandleEmitter : public Emitter {
	public:
		virtual ~HandleEmitter();

		static Emitter * const stdout;
		static Emitter * const stderr;

		/// Receive a message.
		///
		/// @param logger Source of the message.
		/// @param message Formatted message body to process.
		///
		virtual void emit( const Logger & logger, Message && message ) override;

		/// Create a new emitter.
		///
		/// @tparam Cleanup function for handle.
		/// @param handle FD to write to.
		/// @param function Cleanup function for handle.
		///
		template < typename Cleanup >
		HandleEmitter( int handle, Cleanup && function )
		: fd( handle )
		, cleanup( std::forward<Cleanup>( function ) )
		{}

		inline HandleEmitter( int handle )
		: fd( handle )
		, cleanup( nullptr )
		{} 

	protected:
		int fd;
		std::function<void(int)> cleanup;
	};
}
