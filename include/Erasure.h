#pragma once
#include <cstddef>
#include <memory>
#include <dynalog/include/Reflection.h>
#include <iostream>

namespace dynalog {

  namespace details {

    template < typename T >
    constexpr T max( T a, T b ) { return a > b ? a : b; }

    /// A wrapper that may hold any other class, preserving value semantics.
    ///
    /// Conceptually, erasures operates in manner similarly to std::function:
    /// given limited internal storage, it either captures objects internally
    /// or externally, and constructs an appropriate interface for interacting
    /// with them.
    ///
    /// Here, appropriate entails methods required for value semantics:
    ///   - copy (TODO detect/handle uncopyable objects)
    ///   - move (TODO detect/handle unmovable objects)
    ///   - destructor.
    ///
    /// Conceptually, it closely follows ObjectBuffer: Erasure uses an abstract
    /// base interface to interact with stored objects, and always contains a
    /// stored object(NullObject if empty).
    ///
    /// TODO: Gracefully handle erasures of mixed capacity.
    /// TODO: Handle proxying concepts as mix-ins(perhaps via CRTP).
    ///
    class BasicErasure {
     public:
  
      enum class Location {
        Internal,
        External,
        Empty,
      };

      /// Query where the object is stored.
      ///
      Location location() const { return interface<ObjectInterface>().location(); }

      /// Cast erasure contents to a type, unchecked.
      ///
      template < typename T >
      T & as() { return interface<ObjectInterface>().as<T>(); }

      /// Cast erasure contents to a type, unchecked--const correct.
      ///
      template < typename T >
      const T & as() const { return interface<ObjectInterface>().as<T>(); }

      /// Get reflection information for the contained object.
      ///
      Reflection reflect() const { return interface<ObjectInterface>().reflect(); }

      /// Construct an empty erasure
      ///
      BasicErasure()
      {
        new ( buffer() ) NullObject{};
      }

      BasicErasure( nullptr_t )
      : BasicErasure()
      {}

      /// Clear erasure via nullptr assignment.
      ///
      BasicErasure & operator = ( nullptr_t )
      {
        replace<NullObject>();
        return * this;
      }

      /// Destroy the erasure contents.
      ///
      ~BasicErasure()
      {
       interface<ObjectInterface>().~ObjectInterface();
      }

     protected:
      class ObjectInterface {
       public:
        virtual ~ObjectInterface() = default;

        template < typename T >
        T & as() { return *reinterpret_cast<T*>( object() ); }

        template < typename T >
        const T & as() const { return *reinterpret_cast<const T*>( object() ); }

        virtual void copy_to( BasicErasure &, size_t capacity ) const = 0;
        virtual void move_to( BasicErasure &, size_t capacity ) = 0;

        virtual Reflection reflect() const = 0;

        virtual Location location() const = 0;

       protected:
        virtual void * object() = 0;
        virtual const void * object() const = 0;
      };

      template <typename T>
      class InternalObject final : public ObjectInterface {
       public:
        static constexpr size_t size() { return sizeof(InternalObject) + sizeof(T); }

        virtual ~InternalObject()
        {
          as<T>().~T();
        }

        template < typename ...Args >
        InternalObject( Args && ...args )
        {
          new ( this + 1 ) T{ std::forward<Args>( args )... };
        }

        virtual void copy_to( BasicErasure & other, size_t capacity ) const override
        {
          other.construct<T>( capacity, as<T>() );
        }

        virtual void move_to( BasicErasure & other, size_t capacity ) override
        {
          other.construct<T>( capacity, std::move( as<T>() ) );
        }

        virtual Reflection reflect() const override
        {
          return Reflection{ as<T>() };
        }

        virtual Location location() const override { return Location::Internal; }

       protected:
        virtual void * object() override { return this + 1; }
        virtual const void * object() const override { return this + 1; }
      };

      template <typename T>
      class ExternalObject final : public ObjectInterface {
       public:
        static constexpr size_t size() { return sizeof(ExternalObject); }

        virtual ~ExternalObject() = default;

        template < typename ...Args >
        ExternalObject( Args && ...args )
        : pointer( new T{ std::forward<Args>( args )... } )
        {}

        ExternalObject( ExternalObject && ) = default;

        virtual void copy_to( BasicErasure & other, size_t capacity ) const override
        {
          other.construct<T>( capacity, as<T>() );
        }

        virtual void move_to( BasicErasure & other, size_t ) override
        {
          other.replace<ExternalObject<T>>( std::move( *this ) );
        }

        virtual Reflection reflect() const override
        {
          return Reflection{ as<T>() };
        }

        virtual Location location() const override { return Location::External; }

       protected:
        virtual void * object() override { return pointer.get(); }
        virtual const void * object() const override { return pointer.get(); }
        std::unique_ptr<T> pointer;
      };

      class NullObject final : public ObjectInterface {
       public:
        static constexpr size_t size() { return sizeof(NullObject); }

        virtual ~NullObject() = default;

        virtual void copy_to( BasicErasure & other, size_t ) const override
        {
          other.replace<NullObject>();
        }

        virtual void move_to( BasicErasure & other, size_t ) override
        {
          other.replace<NullObject>();
        }

        virtual Reflection reflect() const override
        {
          Reflection result;
          result.reflect<nullptr_t>( nullptr );
          return result;
        }

        virtual Location location() const override { return Location::Empty; }

       protected:
        virtual void * object() override { return nullptr; }
        virtual const void * object() const override { return nullptr; };
      };

      template < typename T >
      T & interface() { return *reinterpret_cast<T*>( buffer() ); }

      template < typename T >
      const T & interface() const { return *reinterpret_cast<const T*>( buffer() ); }

      
      /// Construct a new object in the erasure.
      ///
      /// Object is stored internally if size is permitted, externally otherwise.
      ///
      /// @tparam T Type of object to construct.
      /// @tparam Args Parameter pack of arguments for the constructor.
      /// @param args Arguments for the constructor.
      ///
      template < typename T, typename ...Args >
      void construct( size_t capacity, Args && ...args )
      {
        if( InternalObject<T>::size() <= capacity )
        {
          replace<InternalObject<T>>( std::forward<Args>( args )... );
        }
        else
        {
          replace<ExternalObject<T>>( std::forward<Args>( args )... );
        }
      }

      template < typename T, typename ...Args >
      void replace( Args && ...args )
      {
        interface<ObjectInterface>().~ObjectInterface();
        new ( buffer() ) T{ std::forward<Args>( args )... };
      }

      void * buffer() { return this; }
      const void * buffer() const { return this; }

      static constexpr size_t buffer_size( size_t capacity )
      {
        return max( capacity + NullObject::size(), ExternalObject<int>::size() );
      }
    };
  }

  /// A wrapper that may hold any other class, preserving value semantics.
  ///
  /// Conceptually, erasures operates in manner similarly to std::function:
  /// given limited internal storage, it either captures objects internally
  /// or externally, and constructs an appropriate interface for interacting
  /// with them.
  ///
  /// Here, appropriate entails methods required for value semantics:
  ///   - copy (TODO detect/handle uncopyable objects)
  ///   - move (TODO detect/handle unmovable objects)
  ///   - destructor.
  ///
  /// Conceptually, it closely follows ObjectBuffer: Erasure uses an abstract
  /// base interface to interact with stored objects, and always contains a
  /// stored object(NullObject if empty).
  ///
  /// TODO: Gracefully handle erasures of mixed capacity.
  /// TODO: Handle proxying concepts as mix-ins(perhaps via CRTP).
  ///
  template < size_t Capacity >
  class Erasure : public details::BasicErasure {
   public:
    using super = details::BasicErasure;
    using super::super;
    using super::operator =;
    using super::Location;

    /// Construct a new object in the erasure.
    ///
    /// Object is stored internally if size is permitted, externally otherwise.
    ///
    /// @tparam T Type of object to construct.
    /// @tparam Args Parameter pack of arguments for the constructor.
    /// @param args Arguments for the constructor.
    ///
    template < typename T, typename ...Args >
    void emplace( Args && ...args )
    {
      construct<T>( super::buffer_size( Capacity ), std::forward<Args>( args )... );
    }

    /// Copy assign from another erasure.
    ///
    Erasure & operator = ( const BasicErasure & other )
    {
      other.interface<ObjectInterface>().copy_to( *this, super::buffer_size( Capacity ) );
      return * this;
    }

    /// Move assign from another erasure.
    ///
    Erasure & operator = ( BasicErasure && other )
    {
      other.interface<ObjectInterface>().move_to( *this, super::buffer_size( Capacity ) );
      return * this;
    }

    /// Assign an erasure, capturing another value.
    ///
    template < typename T, typename Base = typename std::decay<T>::type,
      typename = typename std::enable_if<!std::is_same<Erasure,Base>::value>::type >
    Erasure & operator = ( T && value )
    {
      emplace<Base>( std::forward<T>( value ) );
      return *this;
    }

    Erasure()
    : BasicErasure()
    {}

    /// Copy construct from another erasure.
    ///
    Erasure( BasicErasure & other )
    : Erasure()
    {
      other.interface<ObjectInterface>().copy_to( *this, super::buffer_size( Capacity ) );
    }

    /// Move construct from another erasure.
    ///
    Erasure( BasicErasure && other )
    : Erasure()
    {
      other.interface<ObjectInterface>().move_to( *this, super::buffer_size( Capacity ) );
    }

    /// Construct an erasure, capturing another value.
    ///
    template < typename T, typename Base = typename std::decay<T>::type,
      typename = typename std::enable_if<!std::is_same<BasicErasure,Base>::value>::type >
    Erasure( T && value )
    : Erasure()
    {
      emplace<Base>( std::forward<T>( value ) );
    }

   protected:
    uint8_t buffer[ super::buffer_size( Capacity ) ];
  };
}
