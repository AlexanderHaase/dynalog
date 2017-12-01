#pragma once
#include <dynalog/include/util.h>
#include <typeindex>

namespace dynalog {

  /// First-draft reflection.
  ///
  /// Support for interrogating opaque structures at runtime.
  ///
  class Reflection {
   public:

    /// Properties of the exact type of the reflected object.
    ///
    enum class Property
    {
      is_decay,     ///< If the type is equal to std::decay of iteslf.
      is_const,
      is_pointer,
      is_reference,
      is_array,
    };

    /// Number of properties
    ///
    /// TODO: better enum inspection support, maybe enum_traits<T> 
    /// or numeric_limits<..> ?
    ///
    static constexpr std::underlying_type<Property>::type PropertyCount = 5;

    /// Capture the reflection properties of the specified object.
    ///
    /// Captures a const-pointer to the specified value, and std::type_info of
    /// the object's type after applying std::decay. Also Captures properties
    /// of the original type.
    ///
    /// @tparam Type original type for reflection. std::decay is applied to obtain type_info.
    /// @param value value to reflect. A const-pointer is captured.
    ///
    template < typename Type >
    void reflect( Type && value )
    {
      info = &typeid( typename std::decay<Type>::type );
      object = &value;
      properties.set( Property::is_decay, std::is_same<Type,typename std::decay<Type>::type>::value );
      properties.set( Property::is_const, std::is_const<Type>::value );
      properties.set( Property::is_pointer, std::is_pointer<Type>::value );
      properties.set( Property::is_reference, std::is_reference<Type>::value );
      properties.set( Property::is_array, std::is_array<Type>::value );
    }

    Reflection() = default;
    Reflection( const Reflection & ) = default;
    Reflection( Reflection && ) = default;
    Reflection & operator = ( const Reflection & ) = default;
    Reflection & operator = ( Reflection && ) = default;

    /// Construct reflection from object.
    ///
    /// Note: Forbids implicit reflecting a Reflection.
    ///
    template < typename Type, typename =
      typename std::enable_if<!std::is_same<typename std::decay<Type>::type,Reflection>::value>::type >
    Reflection( Type && value )
    {
      reflect( std::forward<Type>( value ) );
    }

    /// Assign reflection from object.
    ///
    /// Note: Forbids implicit reflecting a Reflection.
    ///
    template < typename Type,typename = 
      typename std::enable_if<!std::is_same<typename std::decay<Type>::type,Reflection>::value>::type >
    Reflection & operator = ( Type && value )
    {
      reflect( std::forward<Type>( value ) );
      return *this;
    }

    /// Query a property of the original type.
    ///
    bool query( Property prop ) const { return properties.get( prop ); }

    /// Access the type info of the decayed type.
    ///
    const std::type_info & type() const { return *info; }

    /// Check if the type matches after applying std::decay.
    ///
    template < typename Type >
    bool is() const { return info && typeid(typename std::decay<Type>::type) == *info; }

    /// Access reflected value via a const reference.
    ///
    /// Note: No type checking is applied! Use is<...>() to check.
    ///
    template < typename Type >
    auto as() const
      -> typename std::add_lvalue_reference<typename std::add_const<Type>::type>::type
    {
      using CastType = typename std::add_pointer<typename std::add_const<Type>::type>::type;
      return *reinterpret_cast<CastType>( object );
    }

   protected:
    const std::type_info * info = nullptr;  ///< Captured type_info after std::decay.
    const void * object = nullptr;          ///< Captured pointer to reflected value.
    EnumSet<Property,PropertyCount> properties{ 0 };
  };

  /// First draft inspection interface.
  ///
  /// Inspectors provide reflections.
  ///
  struct Inspector
  {
    virtual ~Inspector() = default;

    /// Probe the number of elements in the closure.
    ///
    /// @return number of elements in closure.
    ///
    virtual size_t size( void ) const = 0;

    /// Get the reflection for the element at index.
    ///
    /// @param index Element to reflect.
    /// @return Reflection data for element.
    ///
    virtual Reflection reflect( size_t index ) const = 0;
  };
}
