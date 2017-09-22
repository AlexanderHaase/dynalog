#include <unistd.h>
#include <sstream>
#include <dynalog/include/HandleEmitter.h>
#include <streambuf>
#include <ostream>
#include <cstring>

namespace dynalog {

	static HandleEmitter stdoutEmitter( STDOUT_FILENO );
	static HandleEmitter stderrEmitter( STDERR_FILENO );	

	Emitter * const HandleEmitter::stdout = &stdoutEmitter;
	Emitter * const HandleEmitter::stderr = &stderrEmitter;

	template < typename char_type >
	struct streambuf_wrapper : public std::streambuf
	{
		streambuf_wrapper( void * buffer, size_t size )
		{
			char_type * base = static_cast<char_type*>( buffer );
			setp( base, base + (size / sizeof(char_type)) );
		}
	};

	/// Receive a message.
	///
	/// @param logger Source of the message.
	/// @param message Formatted message body to process.
	///
	void HandleEmitter::emit( const Logger & logger, Message && message )
	{
		/*std::stringstream stream;
		stream << message << std::endl;
		write( fd, stream.str().c_str(), stream.str().size() );*/

		// Use stacked buffer to stream data -- more than 2x faster.
		// TODO: Overflow behavior
		//
		std::array<char,4096> buffer;
		streambuf_wrapper<char> streambuf( buffer.begin(), buffer.size() );
		std::ostream stream(&streambuf);
		stream << message << std::endl;

		const ssize_t expected = strnlen( buffer.begin(), buffer.size() );
		if( expected != write( fd, buffer.begin(), expected ) )
		{
			// Last-resort warning
			//
			dprintf( STDERR_FILENO, "Error: HandleEmitter failed(write syscall failed on fd %d)!\n", fd );
		}
	}

	HandleEmitter::~HandleEmitter( void )
	{
		if( cleanup )
		{
			cleanup( fd );
		}
	}
}


