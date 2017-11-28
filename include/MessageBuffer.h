#pragma once
#include <atomic>
#include <dynalog/include/ARC.h>

namespace dynalog {

  /// Buffer/erasure for Message contents.
  ///
  /// Provides buffer/erasure semantics, attempting to reduce copy/move size to
  /// a single pointer.
  ///
  /// Utilizes arc anticipating multiple message consumers.
  ///
  class MessageBuffer : public arc::Object<MessageBuffer> {
   public:

    /// Pointer type for message buffer.
    ///
    /// Reference counted: move is free, shallow copy is cheap.
    ///
    using Pointer = arc::Pointer<MessageBuffer>;

    /// Access contained type object(std::decay<T>::type).
    ///
    /// @return type_info of contained type, or typeid(nullptr_t) if empty.
    ///
    const std::type_info & type() const { return as<ObjectInterface>().type(); }

    /// Query capacity of message buffer.
    ///
    size_t capacity() const { return bytes - sizeof(ObjectInterface); }

    /// Query size of message buffer contents.
    ///
    size_t size() const { return object().size(); }

    /// Release/destroy message buffer contents.
    ///
    void clear()
    {
      object().~ObjectInterface();
      new (data()) EmptyObject{};
    }

    /// Query if message buffer is empty.
    ///
    bool empty() const { return object().empty(); }

    /// Create a new object in the message buffer, deleting the prior(if any).
    ///
    /// @tparam T Type to construct.
    /// @tparam Args Argument types for constructor arguments.
    /// @param args Constructor arguments.
    /// @return true if new object created, false otherwise.
    ///
    template < typename T, typename ...Args >
    bool emplace( Args && ...args )
    {
      const bool result = (sizeof(ObjectAdapter<T>) + sizeof(T)) <= bytes;
      if( result )
      {
        object().~ObjectInterface();
        new (data()) ObjectAdapter<T>{ std::forward<Args>( args )... };
      }
      return result;
    }

    /// Destructor: clear buffer.
    ///
    ~MessageBuffer()
    {
      object().~ObjectInterface();
    }

    /// Access contained object by cast.
    ///
    /// @tparm T Type of inner object--unchecked!
    ///
    template < typename T >
    inline T & as()
    {
      return reinterpret_cast<ObjectAdapter<T>*>( data() )->object();
    }

    /// Access contained object by cast.
    ///
    /// @tparm T Type of inner object--unchecked!
    ///
    template < typename T >
    inline const T & as() const
    {
      return reinterpret_cast<const ObjectAdapter<T>*>( data() )->object();
    }

    /// Create a new buffer with the requested capacity.
    ///
    /// @param requested Requested capacity in bytes.
    ///
    static MessageBuffer::Pointer create( size_t capacity )
    {
      const auto required = capacity + sizeof(ObjectInterface);
      const auto size = required + sizeof(MessageBuffer);
      return new (::operator new(size)) MessageBuffer{ required };
    }

    // Delete copy and move operations: not well formed without more wrapping.
    //
    MessageBuffer( const MessageBuffer & ) = delete;
    MessageBuffer( MessageBuffer && ) = delete;
    MessageBuffer & operator = ( const MessageBuffer & ) = delete;
    MessageBuffer & operator = ( MessageBuffer && ) = delete;

   protected:

    /// Create a message buffer, specifying the capacity.
    ///
    /// Only use with placement new!
    ///
    MessageBuffer( size_t size )
    : bytes( size )
    {
      new (data()) EmptyObject{};
    }

    /// Access raw buffer.
    ///
    inline void * data() { return static_cast<void*>( this + 1 ); }

    /// Access raw buffer.
    ///
    inline const void * data() const { return static_cast<const void*>( this + 1 ); }

    /// Interface for contained objects--encapsulates common traits,
    /// provides virtual destructor.
    ///
    struct ObjectInterface
    {
      virtual ~ObjectInterface() = default;
      virtual size_t size() const  = 0;
      virtual bool empty() const = 0;
      virtual const std::type_info & type() const = 0;
    };

    /// Empty object type.
    ///
    /// Constructed when no other object in place, buffer is always non-empty.
    ///
    struct EmptyObject
    {
      virtual ~EmptyObject() = default;
      virtual size_t size() const { return 0; }
      virtual bool empty() const { return true; }
      virtual const std::type_info & type() const { return typeid(std::nullptr_t); }
    };

    /// Adapts arbitrary types to the object interface.
    ///
    /// Provides virtual destructor and access to contained type.
    ///
    template < typename T >
    struct ObjectAdapter : ObjectInterface
    {
      template < typename ... Args >
      ObjectAdapter( Args && ... args )
      {
        new ( &object() ) T{ std::forward<Args>( args )... };
      }

      inline T & object()
      {
        return *reinterpret_cast<T*>( this + 1 );
      }

      inline const T & object() const
      {
        return *reinterpret_cast<const T*>( this + 1 );
      }

      virtual ~ObjectAdapter() { object().~T(); } 

      virtual size_t size() const override { return sizeof(T); }
      virtual bool empty() const override { return false; }
      virtual const std::type_info & type() const override { return typeid(T); }
    };

    static_assert( sizeof( ObjectAdapter<int> ) == sizeof( ObjectInterface ), "Size computation error" );

    /// Access buffer as object interface.
    ///
    inline ObjectInterface & object() { return *reinterpret_cast<ObjectInterface *>( data() ); }

    /// Access buffer as object interface.
    ///
    inline const ObjectInterface & object() const { return *reinterpret_cast<const ObjectInterface *>( data() ); }

    const size_t bytes;
  };
}
