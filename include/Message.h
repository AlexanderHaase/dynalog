#pragma once
#include <dynalog/include/ObjectBuffer.h>
#include <dynalog/include/util.h>
#include <typeindex>
#include <ostream>

namespace dynalog {

	/// Messages encapsulate data in a portable closure.
	///
	class Message {
	public:
		/// Content specifies the closure interface for encapsulated data.
		///
		struct Content
		{
			/// First-draft reflection.
			///
			/// TODO: modifier detection, special case pointer, references, etc.
			///
			struct Reflection
			{
				const std::type_info * info = nullptr;
				const void * object = nullptr;
				/*
				bool cv = false;
				bool ptr = false;
				bool ref = false;
				*/

				template < typename Type >
				void reflect( Type && type )
				{
					info = &typeid( typename std::decay<Type>::type );
					object = &type;
				}

				template < typename Type >
        bool is() const { return info && typeid(typename std::decay<Type>::type) == *info; }

				template < typename Type >
        auto as() const
          -> typename std::add_lvalue_reference<typename std::add_const<Type>::type>::type
        {
          using CastType = typename std::add_pointer<typename std::add_const<Type>::type>::type;
          return *reinterpret_cast<CastType>( object );
        }
			};

			virtual ~Content( void ) {}

			/// Write the enclosed contents to the provided stream
			///
			/// @param ostream Stream to write contents to.
			///
			virtual void serialize( std::ostream & ostream ) const = 0;

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

		/// Implementation that copies and outputs streamable objects.
		///
		template < typename ... Args >
		struct Body : Content
		{
			const std::tuple<Args...> elements;
			using AllIndexes = typename GenerateIndexSequence<sizeof...(Args)>::type;

			/// Apply a functor to the specified tuple indices.
			///
			/// In c++17, a fold expression would be equally viable...
			///
			/// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
			/// @tparam Index to process at this step.
			/// @tparam ...Remainder indicies yet to process
			/// @param func Functor to apply.
			/// @param Index specifier for deduction.
			///
			template< typename Func, size_t Index, size_t ...Remainder >
			void apply( Func && func, IndexSequence<Index, Remainder...> ) const
			{
				func( Index, std::get<Index>( elements ) );
				apply( std::forward<Func>( func ), IndexSequence<Remainder...>{} );
			}

			/// Apply a functor to the specified tuple index.
			///
			/// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
			/// @tparam Index to process at this step.
			/// @param func Functor to apply.
			/// @param Index specifier for deduction.
			///
			template< typename Func, size_t Index >
			void apply( Func && func, IndexSequence<Index> ) const
			{
				func( Index, std::get<Index>( elements ) );
			}

			/// Apply a stream operator to each passed object.
			///
			struct Streamer
			{
				std::ostream & stream;

				template <typename Type>
				void operator() ( size_t, Type && type ) { stream << type; }
			};

			/// Create and populate the closure.
			///
			Body( Args &&...args )
			: elements( std::forward<Args>( args )... )
			{}

			virtual ~Body( void ) {}

			/// Wrapper to serialize contents.
			///
			virtual void serialize( std::ostream & stream ) const
			{				
				apply( Streamer{ stream }, AllIndexes{} );
			}

			/// Wrapper to return tuple size.
			///
			virtual size_t size( void ) const { return sizeof...(Args); }

			/// Reflect the object at the specified index.
			///
			struct Reflector
			{
				const size_t target;
				Content::Reflection reflection;
				
				template <typename Type>
				void operator() ( const size_t index, Type && type )
				{
					if( index == target )
					{
						reflection.reflect( std::forward<Type>( type ) );
					}
				}
			};

			/// Wrapper to get the specified reflection.
			///
			virtual Reflection reflect( size_t index ) const
			{
				Reflector reflector{ index, Content::Reflection{} };
				apply( reflector, AllIndexes{} );
				return reflector.reflection;
			}
		};

		/// Create a closure for the specified types
		///
		template < typename ... Args, typename Class = Body<Args...> >
		void format( Args && ... args )
		{
			const size_t required = sizeof( Class );
			if( buffer.size() < required )
			{
				buffer.resize( required );
				//buffer.resize( cached( required ) );
			}
			buffer.emplace< Body<Args...> >( std::forward<Args>( args )... );
		}

		/// Helper to allow injecting of aribtrary class as the first 
		/// template argument.
		///
		template < typename Class, typename ... Args >
		void format( Args && ... args )
		{
			format<Args...,Class>( std::forward<Args>( args )... );
		}

		/// Accessor for content.
		///
		Content & content() { return buffer.as<Content>(); }

		/// Accessor for content.
		///
		const Content & content() const { return buffer.as<const Content>(); }

		/// Check if message is populated.
		///
		bool empty( void ) const { return buffer.empty(); }

	protected:
		/// Return a cached buffer for the size, if available.
		///
		static Buffer::Pointer cached( size_t size );

		ObjectBuffer buffer;	///< Storage for message contents.
	};


}

::std::ostream & operator << ( ::std::ostream & stream, const ::dynalog::Message & message );
