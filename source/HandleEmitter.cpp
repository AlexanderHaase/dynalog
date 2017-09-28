#include <unistd.h>
#include <cstring>
#include <sstream>
#include <dynalog/include/HandleEmitter.h>
#include <dynalog/include/async/Replicated.h>
#include <streambuf>
#include <ostream>

namespace dynalog {

	static HandleEmitter stdoutEmitter( STDOUT_FILENO );
	static HandleEmitter stderrEmitter( STDERR_FILENO );	

	Emitter * const HandleEmitter::stdout = &stdoutEmitter;
	Emitter * const HandleEmitter::stderr = &stderrEmitter;

	/// Still a bit of a hack...should really just make an array wrapper
	/// that properly implements streambuf ops.
	///
	template < typename char_type >
	struct streambuf_wrapper : public std::streambuf
	{
		streambuf_wrapper( void * buffer, size_t size )
		{
			char_type * base = static_cast<char_type*>( buffer );
			setp( base, base + (size / sizeof(char_type)) );
		}

		std::tuple<char_type*,size_t> formatted() const
		{
			auto begin = pbase();
			auto end = pptr();
			return std::make_tuple( begin, end - begin );
		}

		bool write( int fd ) const
		{
			const auto area = formatted();
			return ssize_t(std::get<1>( area )) == ::write( fd, std::get<0>( area ), std::get<1>( area ) );
		}

		void reset( void )
		{
			setp( pbase(), epptr() );
		}

		/* This is slower than writing above??
		int fd;
		virtual int sync()
		{
			const auto area = formatted();
			const bool result = ssize_t(std::get<1>( area )) == ::write( fd, std::get<0>( area ), std::get<1>( area ) );
			if( result )
			{
				setp( pbase(), epptr() );
			}
			return result ? 0 : -1;
		}

		virtual typename traits::int_type overflow( int ch )
		{
			return sync() == 0 ? base::sputc( ch ) : traits::eof();
		}
		*/
	};

	template <typename Char, size_t size, typename Traits = std::char_traits<Char> >
	struct ArrayStream
	{
		std::array<Char,size> buffer;
		streambuf_wrapper<Char> streambuf;
		std::ostream stream;

		ArrayStream()
		: streambuf( buffer.begin(), buffer.size() )
		, stream( &streambuf )
		{}
	};

	/// Thread-safe buffer for emitters
	///
	static async::Replicated<ArrayStream<char,4096> > streams( std::tuple<>{} );

	/// Receive a message.
	///
	/// @param logger Source of the message.
	/// @param message Formatted message body to process.
	///
	void HandleEmitter::emit( const Logger & logger, Message && message )
	{
		// Use stacked buffer to stream data -- more than 3x faster than stringstream.
		// TODO: Overflow behavior, sync behavior
		//
		auto result = streams.with( [&]( ArrayStream<char,4096> & stream )
		{
			stream.streambuf.reset();
			stream.stream << message << std::endl;
			return stream.streambuf.write( fd );
		});

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


