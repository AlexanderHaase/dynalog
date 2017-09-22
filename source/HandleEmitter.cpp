#include <unistd.h>
#include <sstream>
#include <dynalog/include/HandleEmitter.h>

namespace dynalog {

	static HandleEmitter stdoutEmitter( STDOUT_FILENO );
	static HandleEmitter stderrEmitter( STDERR_FILENO );	

	Emitter * const HandleEmitter::stdout = &stdoutEmitter;
	Emitter * const HandleEmitter::stderr = &stderrEmitter;

	/// Receive a message.
	///
	/// @param logger Source of the message.
	/// @param message Formatted message body to process.
	///
	void HandleEmitter::emit( const Logger & logger, Message && message )
	{
		std::stringstream stream;
		stream << message << std::endl;
		write( fd, stream.str().c_str(), stream.str().size() );
	}

	HandleEmitter::~HandleEmitter( void )
	{
		if( cleanup )
		{
			cleanup( fd );
		}
	}
}


