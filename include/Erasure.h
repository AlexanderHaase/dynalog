#pragma once
#include <cstddef>
#include <memory>
#include <dynalog/include/Reflection.h>

namespace dynalog {

  namespace details {

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
        if( InternalObject<T>::size() <= size )
        {
          replace<InternalObject<T>>( std::forward<Args>( args )... );
        }
        else
        {
          replace<ExternalObject<T>>( std::forward<Args>( args )... );
        }
      }

      /// Get reflection information for the contained object.
      ///
      Reflection reflect() const { return interface<ObjectInterface>().reflect(); }

      /// Construct an empty erasure
      ///
      BasicErasure( nullptr_t = nullptr )
      {
        new ( buffer() ) NullObject{};
      }

      /// Copy construct from another erasure.
      ///
      BasicErasure( const BasicErasure & other )
      : BasicErasure()
      {
        other.interface<ObjectInterface>().copy_to( *this );
      }

      /// Move construct from another erasure.
      ///
      BasicErasure( BasicErasure && other )
      : BasicErasure()
      {
        other.interface<ObjectInterface>().move_to( *this );
      }

      /// Construct an erasure, capturing another value.
      ///
      template < typename T, typename Base = typename std::decay<T>::type,
        typename = typename std::enable_if<!std::is_same<BasicErasure,Base>::value>::type >
      BasicErasure( T && value )
      : BasicErasure()
      {
        emplace<Base>( std::forward<T>( value ) );
      }

      /// Clear erasure via nullptr assignment.
      ///
      BasicErasure & operator = ( nullptr_t )
      {
        replace<NullObject>();
        return * this;
      }

      /// Copy assign from another erasure.
      ///
      BasicErasure & operator = ( const BasicErasure & other )
      {
        other.interface<ObjectInterface>().copy_to( *this );
        return * this;
      }

      /// Move assign from another erasure.
      ///
      BasicErasure & operator = ( BasicErasure && other )
      {
        other.interface<ObjectInterface>().move_to( *this );
        return * this;
      }

      /// Assign an erasure, capturing another value.
      ///
      template < typename T, typename Base = typename std::decay<T>::type,
        typename = typename std::enable_if<!std::is_same<BasicErasure,Base>::value>::type >
      BasicErasure & operator = ( T && value )
      {
        emplace<Base>( std::forward<T>( value ) );
        return *this;
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

        virtual void copy_to( BasicErasure & ) const = 0;
        virtual void move_to( BasicErasure & ) = 0;

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

        virtual void copy_to( BasicErasure & other ) const override
        {
          other.emplace<T>( as<T>() );
        }

        virtual void move_to( BasicErasure & other ) override
        {
          other.emplace<T>( std::move( as<T>() ) );
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

        virtual void copy_to( BasicErasure & other ) const override
        {
          other.emplace<T>( as<T>() );
        }

        virtual void move_to( BasicErasure & other ) override
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

        virtual void copy_to( BasicErasure & other ) const override
        {
          other.replace<NullObject>();
        }

        virtual void move_to( BasicErasure & other ) override
        {
          other.replace<NullObject>();
        }

        virtual Reflection reflect() const override
        {
          return Reflection{};
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

      template < typename T, typename ...Args >
      void replace( Args && ...args )
      {
        interface<ObjectInterface>().~ObjectInterface();
        new ( buffer() ) T{ std::forward<Args>( args )... };
      }

      void * buffer() { return this; }
      const void * buffer() const { return this; }

      static constexpr size_t size = (Capacity + NullObject::size() >= ExternalObject<int>::size());// : Capacity + NullObject::size() ? ExternalObject<int>::size();
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
  class Erasure : public details::BasicErasure< Capacity > {
   public:
    using super = details::BasicErasure< Capacity >;
    using super::super;
    using super::operator =;
    using super::Location;
   protected:
    uint8_t buffer[ super::size ];
  };
}
