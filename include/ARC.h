#pragma once
#include <atomic>
#include <memory>
#include <cstddef>

namespace dynalog {

  /// Automated reference counting using intrusive counters.
  ///
  /// Slightly more performant and concise compared to std::shared_ptr.
  ///
  /// ARC pointer template calls acquire()/release() method on the contained
  /// type.
  ///
  /// ARC object template applies a functor to the object when reference count
  /// reaches zero.
  ///
  namespace arc {

    /// Reference counting base class that applies an action when count reaches zero.
    ///
    /// @tparam T type of parent class: 'class Parent : arc::Object<Parent>'
    /// @tparam Action functor to apply when reference count reaches zero.
    ///
    template < typename T , typename Action = std::default_delete<T> >
    class Object : protected Action {
     public:

      /// Access method for action.
      ///
      Action & action() { return static_cast<Action&>( *this ); }

      /// Increment the reference count.
      ///
      friend T * acquire( T * object )
      {
        if( object )
        {
          object->references.fetch_add( 1, std::memory_order_relaxed );
        }
        return object;
      }

      /// Decrement the reference count, applying the action if count becomes zero.
      ///
      friend void release( T * object )
      {
        if( object )
        {
          const auto prior = object->references.fetch_add( -1, std::memory_order_relaxed );
          if( prior == 1 )
          {
            object->action()( object );
          }
        }
      }
      
     protected:
      std::atomic<size_t> references{ 0 };
    };

    /// Pointer for arc semantics.
    ///
    /// Expects types with acquire() and release() methods.
    ///
    template < typename T>
    class Pointer {
     public:

      /// Access contained pointer
      ///
      /// @return raw pointer value.
      ///
      T * get() const { return ptr; }

      /// Pointer method access operator.
      ///
      T * operator -> () const { return get(); }

      /// Pointer dereference operator.
      /// 
      T & operator * () const { return get(); }

      /// Create an empty pointer
      ///
      Pointer()
      : ptr( nullptr )
      {}

      /// Explicitly create an empty pointer
      ///
      Pointer( std::nullptr_t )
      : ptr( nullptr )
      {}

      /// Construct a pointer from a raw pointer.
      ///
      Pointer( T * buffer )
      : ptr( acquire( buffer ) )
      {}

      /// Construct a pointer by copy.
      ///
      Pointer( const Pointer & other )
      : ptr( acquire( other.ptr ) )
      {}

      /// Construct a pointer by move.
      /// 
      Pointer( Pointer && other )
      : ptr( std::move( other.ptr ) )
      {
        other.ptr = nullptr;
      }

      /// Assign pointer by copy.
      ///
      Pointer & operator = ( const Pointer & other )
      {
        release( ptr );
        ptr = acquire( other.ptr );
        return *this;
      }

      /// Assign pointer by move.
      ///
      Pointer & operator = ( Pointer && other )
      {
        release( ptr );
        ptr = std::move( other.ptr );
        other.ptr = nullptr;
        return *this;
      }

      /// Assign pointer from raw pointer.
      ///
      Pointer & operator = ( T * buffer )
      {
        release( ptr );
        ptr = acquire( buffer );
        return *this;
      }

      /// Assign pointer from nullptr
      ///
      Pointer & operator = ( std::nullptr_t )
      {
        release( ptr );
        ptr = nullptr;
        return *this;
      }

      /// Destructor: releases reference.
      ///
      ~Pointer()
      {
        release( ptr );
      }
     protected:
      T * ptr;  ///< Internal managed pointer.
    };

    /// Equality operator for two pointer types.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator == ( const Pointer<T> & a, const Pointer<U> & b )
    {
      return a.get() == b.get();
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator == ( const Pointer<T> & a, U * b )
    {
      return a.get() == b;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator == ( U * b, const Pointer<T> & a )
    {
      return a.get() == b;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T>
    bool operator == ( const Pointer<T> & a, std::nullptr_t )
    {
      return a.get() == nullptr;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T>
    bool operator == ( std::nullptr_t, const Pointer<T> & a )
    {
      return a.get() == nullptr;
    }

    /// Equality operator for two pointer types.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator != ( const Pointer<T> & a, const Pointer<U> & b )
    {
      return a.get() != b.get();
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator != ( const Pointer<T> & a, U * b )
    {
      return a.get() != b;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T, typename U>
    bool operator != ( U * b, const Pointer<T> & a )
    {
      return a.get() != b;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T>
    bool operator != ( const Pointer<T> & a, std::nullptr_t )
    {
      return a.get() != nullptr;
    }

    /// Equality operator for pointer and raw pointer.
    ///
    /// TODO: Consider casting conversions?
    ///
    template <typename T>
    bool operator != ( std::nullptr_t, const Pointer<T> & a )
    {
      return a.get() != nullptr;
    }
  }
}
