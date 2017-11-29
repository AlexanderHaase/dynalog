#include <unistd.h>
#include <cstring>
#include <sstream>
#include <dynalog/include/HandleEmitter.h>
#include <dynalog/include/async/Replicated.h>
#include <dynalog/include/ProxyOstreambuf.h>
#include <streambuf>
#include <ostream>

namespace dynalog {

	static HandleEmitter stdoutEmitter( STDOUT_FILENO );
	static HandleEmitter stderrEmitter( STDERR_FILENO );	

	Emitter * const HandleEmitter::stdout = &stdoutEmitter;
	Emitter * const HandleEmitter::stderr = &stderrEmitter;

  /// Buffer
	template <typename CharT, size_t Capacity, typename Traits = std::char_traits<CharT> >
	class array_ostream : public std::basic_ostream<CharT,Traits> {
	 public:
    using super = std::basic_ostream<CharT,Traits>;

    void setfd( int fd )
    {
      streambuf.consumer().handle() = fd;
    }

    bool flush()
    {
      return streambuf.flush();
    }

    void clear()
    {
      streambuf.clear();
    }

    array_ostream()
		: super( &streambuf )
		{
      streambuf.set_buffer( buffer.begin(), buffer.size() );
    }

   protected:
    proxy_ostreambuf<CharT, WriteHandle<CharT> > streambuf;
		std::array<CharT,Capacity> buffer;
	};

	/// Thread-safe buffer for emitters
	///
	static async::Replicated<array_ostream<char,4096> > streams( std::tuple<>{} );

	static thread_local array_ostream<char,4096> stream;

	/// Receive a message.
	///
	/// @param logger Source of the message.
	/// @param message Formatted message body to process.
	///
	void HandleEmitter::emit( const Logger &, Message && message )
	{
		// Use stacked buffer to stream data -- more than 3x faster than stringstream.
		// TODO: Overflow behavior, sync behavior
		//
		//auto result = streams.with( [&]( ArrayStream<char,4096> & stream )
		//{
			stream.clear();
      stream.setfd( fd );
			stream << message << "\n"; //std::endl;
			auto result = stream.flush();
			//return stream.streambuf.write( fd );
		//});

		if( ! result )
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


