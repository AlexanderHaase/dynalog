#pragma once

#include <array>
#include <streambuf>
#include <unistd.h>

namespace dynalog {

  /// Functor writes charater sequences to a handle.
  ///
  template < class CharT >
  class WriteHandle {
   public:
    using char_type = CharT;

    /// Initialize with handle.
    ///
    WriteHandle( int fd_arg = -1 )
    : fd( fd_arg )
    {}

    /// Access/set handle for writing
    ///
    /// @return reference to handle.
    ///
    int & handle() { return fd; }

    /// Functor call operator: write sequence to handle.
    ///
    /// @param sequence Character sequence to write.
    /// @param count Number of characters to write.
    /// @return true if all characters were written, false otherwise.
    ///
    bool operator() ( const char_type * const sequence, const std::streamsize count ) const
    {
      const auto size = count * sizeof(CharT);
      return static_cast<ssize_t>( size ) == ::write( fd, sequence, size );
    }

   protected:
    int fd;
  };

  /// Optimized streambuf for low-level serialization.
  ///
  /// Assumes caller will call flush() and attempts to maximally inline
  /// operations:
  ///
  ///   - sync: No op--caller will flush separately.
  ///   - xsputn: Write sequence w/o using virutal methods.
  ///   - xsputc: Write character w/o using virtual methods.
  ///   - overflow: Write character w/o using virtual methods.
  ///
  /// As such, it is not suitable as a generic streambuf base.
  ///
  /// TODO: Handle locale.
  ///
  template < class CharT, class Consumer, class Traits = std::char_traits<CharT> >
  class proxy_ostreambuf : public std::basic_streambuf<CharT, Traits>, protected Consumer {
   public:
    using char_type = CharT;
    using traits_type = Traits;
    using super = std::basic_streambuf<CharT, Traits>;
    using super::sputc;

    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

    virtual ~proxy_ostreambuf() {}

    /// Access/modify consumer.
    ///
    Consumer & consumer() { return *this; }

    /// Reset stream without writing.
    ///
    void clear( void )
    {
      setp( pbase(), epptr() );
    }

    /// Flush and reset stream.
    ///
    /// @return true if entirely flushed, false otherwise.
    ///
    bool flush()
    {
      const auto size = pptr() - pbase();
      const auto result = Consumer::operator() ( pbase(), size );
      pbump( -size );
      return result;
    }

    /// Set the internal buffer, discarding any partial data.
    ///
    /// @param sequence Beginning of buffer.
    /// @param count Capacity of buffer.
    ///
    void set_buffer( char_type * sequence, std::streamsize count )
    {
      setp( sequence, sequence + count );
    }

   protected:
    using super::pbase;
    using super::pptr;
    using super::epptr;
    using super::setp;
    using super::pbump;

    /*virtual void imbue( const std::locale & loc ) override
    {
      locale = loc;
    }*/

    /// flushes the stream on overflow.
    ///
    /// @param value character to write.
    /// @return character written or traits_type::eof() if flush failed.
    ///
		virtual int_type overflow( int_type value ) override
		{
      if( flush() )
      {
        return sputc( value );
      }
      else
      {
        return traits_type::eof();
      }
		}

    /// Write a sequence of characters.
    ///
    /// Doesn't call other virtual methods.
    ///
    virtual std::streamsize xsputn( const char_type * sequence, std::streamsize count ) override
    {
      std::streamsize result = 0;

      for(;;)
      {
        const auto copy_count = std::min( epptr() - pptr(), count );
        ::memcpy( pptr(), sequence, copy_count * sizeof(char_type) );
        //traits_type::copy( pptr(), sequence, copy_size );

        pbump( copy_count );
        result += copy_count;

        sequence += copy_count;
        count -= copy_count;

        if( count == 0 || !flush() )
        {
          break;
        }
      }
      return result;
    }

    /// Disable external sync.
    ///
    virtual int_type sync() override
    {
      return 0;
    }
  };
}
