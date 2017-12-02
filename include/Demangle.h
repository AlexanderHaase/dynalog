#pragma once

#include <string>
#include <typeinfo>

namespace dynalog {

    /// Demangle the name in a std::type_info object.
    ///
    std::string demangle( const std::type_info & );
}
