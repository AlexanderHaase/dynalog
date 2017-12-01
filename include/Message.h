#pragma once
#include <dynalog/include/ObjectBuffer.h>
#include <dynalog/include/Reflection.h>
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
			virtual ~Content( void ) {}

			/// Write the enclosed contents to the provided stream
			///
			/// @param ostream Stream to write contents to.
			///
			virtual void serialize( std::ostream & ostream ) const = 0;

      /// Inspect/reflect message contents.
      ///
      virtual const Inspector & inspect() const = 0;
		};

		/// Implementation that copies and outputs streamable objects.
		///
		template < typename ... Args >
		struct Body : public Content, public Inspector
    {  
			const std::tuple<Args...> elements;

      virtual const Inspector & inspect() const { return *this; }

			/// Apply a stream operator to each passed object.
			///
			struct Streamer
			{
				std::ostream & stream;

				template <typename Type>
				void operator() ( Type && type ) { stream << type; }
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
        visit_tuple( Streamer{ stream }, elements );
			}

			/// Wrapper to return tuple size.
			///
			virtual size_t size( void ) const { return sizeof...(Args); }

			/// Reflect the object at the specified index.
			///
			struct Reflector
			{
				const size_t target;
				Reflection reflection;
				
				template <typename Type>
				void operator() ( Type && type, const size_t index )
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
				Reflector reflector{ index, Reflection{} };
        visit_tuple( reflector, elements );
				return reflector.reflection;
			}
		};

		/// Create a closure for the specified types
		///
		template < typename ... Args, typename Class = Body<Args...> >
		void format( Args && ... args )
		{
			const size_t required = sizeof( Class );
			if( buffer == nullptr || buffer->capacity() < required )
			{
				buffer = ObjectBuffer::create( required );
				//buffer = cached( required );
			}
			buffer->emplace< Body<Args...> >( std::forward<Args>( args )... );
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
		Content & content() { return buffer->as<Content>(); }

		/// Accessor for content.
		///
		const Content & content() const { return buffer->as<const Content>(); }

		/// Check if message is populated.
		///
		bool empty( void ) const { return buffer == nullptr; }

	protected:
		/// Return a cached buffer for the size, if available.
		///
		/*static Buffer::Pointer cached( size_t size );*/

		ObjectBuffer::Pointer buffer;	///< Storage for message contents.
	};


}

::std::ostream & operator << ( ::std::ostream & stream, const ::dynalog::Message & message );
