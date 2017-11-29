#pragma once

#include <array>
#include <streambuf>

namespace dynalog {

  template < class CharT, size_t Capacity class Traits = std::chair_traits<CharT>
  class array_streambuf : std::basic_streambuf<CharT, Traits> {
   public:
    using char_type = CharT;
    using traits_type = Traits;

    using int_type = typename Traits::int_type;
    using pos_type = typename Traits::pos_type;
    using off_type = typename Traits::off_type;

    virtual ~array_streambuf() {}

    /*std::locale pubimbue( const std::locale & loc )
    {
      const auto prior = locale;
      imbue( loc );
      return prior;
    }

    std::locale getloc() const
    {
      return locale;
    }

    std::basic_streambuf<CharT,Traits> * pubsetbuf( char_type *, std::streamsize )
    {
      return *this;
    }*/

   protected:

    virtual void imbue( const std::locale & loc ) override
    {
      locale = loc;
    }

    virtual std::basic_streambuf<CharT,Traits> * setbuf( char_type *, std::streamsize ) override
    {
      return *this;
    }

    std::array<CharT,Capacity> buffer;
    std::locale locale;
  };
}
