#include <cxxabi.h>
#include <memory>
#include <dynalog/include/Demangle.h>

namespace dynalog {

  /// Demangle the name in a std::type_info object.
  ///
  std::string demangle( const std::type_info & info )
  {
    int status = 0;
    std::unique_ptr<char, void(*)(void*)> result { abi::__cxa_demangle(info.name(), NULL, NULL, &status), std::free };

    return status == 0 ? result.get() : info.name();
  }
}
