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
		virtual void emit( const Logger & logger, Message && message );

		/// Create a new emitter.
		///
		/// @tparam cleanup function for handle.
		/// @param handle FD to write to.
		/// @param cleanup Cleanup function for handle.
		///
		template < typename Cleanup >
		HandleEmitter( int handle, Cleanup && cleanup )
		: fd( handle )
		, cleanup( std::forward<Cleanup>( cleanup ) )
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
