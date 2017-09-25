#pragma once
#include <thread>
#include <tuple>
#include <dynalog/include/async/Synchronized.h>

namespace dynalog { namespace async {

	/// Replicated set of objects for balancing concurrent operation.
	///
	/// Creates several Synchronized objects for distributing concurrent
	/// access. By default, object access is indexed by the hash of the
	/// current thread id. By default, twice as many objects are created as
	/// the number of cpus.
	///
	template < typename T >
	class Replicated {
	public:
		/// Recommended number of objects to create under replication.
		///
		/// @return 2 * <number of cpus>
		///
		static size_t recommended( void ) { return 2 * std::thread::hardware_concurrency(); }

		/// Query the number of replicated objects.
		///
		/// @return Number of replicated objects.
		///
		size_t size( void ) const { return instances.size(); }

		/// Visit the object relative to a specific hash under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param hash Hash translated to index via modulo operation.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto with( const size_t hash, Func && func ) -> decltype( func( declref<T>() ) )
		{
			return instances[ hash % instances.size() ]->with( std::forward<Func>( func ) );
		}

		/// Visit the object relative to the current thread under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto with( Func && func ) -> decltype( func( declref<T>(), declref<std::unique_lock<std::mutex>>() ) )
		{
			return with( threadindex( size() ), std::forward<Func>( func ) );
		}

		/// Visit the object relative to a specific hash under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param hash Hash translated to index via modulo operation.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto with( const size_t hash, Func && func ) -> decltype( func( declref<T>(), declref<std::unique_lock<std::mutex>>() ) )
		{
			return instances[ hash % instances.size() ]->with( std::forward<Func>( func ) );
		}

		/// Visit the object relative to the current thread under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto with( Func && func ) -> decltype( func( declref<T>() ) )
		{
			return with( threadindex( size() ), std::forward<Func>( func ) );
		}

		/// Visit the object relative to a specific hash without lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param hash Hash translated to index via modulo operation.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto unprotected( const size_t hash, Func && func ) -> decltype( func( declref<T>() ) )
		{
			return instances[ hash % instances.size() ]->unprotected( std::forward<Func>( func ) );
		}

		/// Visit the object relative to the current thread without lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function
		///
		template < typename Func >
		auto unprotected( Func && func ) -> decltype( func( declref<T>() ) )
		{
			return unprotected( threadindex( size() ), std::forward<Func>( func ) );
		}

		/// Create the replicated object set with a generator function.
		///
		/// @tparam Type that generates Synchronized<T> pointers given an index.
		/// @param size Number of objects to create.
		/// @param generator Reference to generator.
		/// 
		template < typename Generator, typename = decltype( declref<Generator>().operator() ( 0 ) ) >
		Replicated( const size_t size, Generator && generator )
		{
			instances.reserve( size );
			for( size_t index = 0; index < size; ++index )
			{
				instances.emplace_back( generator( index ) );
			}
		}
		
		/// Create the replicated object set with a generator function.
		///
		/// @tparam Type that generates Synchronized<T> pointers given an index.
		/// @param generator Reference to generator.
		/// 
		template < typename Generator, typename = decltype( declref<Generator>().operator() ( 0 ) ) >
		Replicated( Generator && generator )
		: Replicated( recommended(), std::forward<Generator>( generator ) )
		{}
		
		/// Create the replicated object set with a tuple of constructor arguments.
		///
		/// @tparam Args Argument pack of constructor argument types.
		/// @param size Number of objects to create.
		/// @param args Tuple of arguments with which to invoke the constructor of every object.
		/// 
		template < typename ... Args >
		Replicated( const size_t size, const std::tuple<Args...> & args )
		: Replicated( size, [&args]( size_t ){ return apply_tuple( args, make_instance{} ); } )
		{}

		/// Create the replicated object set with a tuple of constructor arguments.
		///
		/// @tparam Args Argument pack of constructor argument types.
		/// @param args Tuple of arguments with which to invoke the constructor of every object.
		/// 
		template < typename ... Args  >
		Replicated( const std::tuple<Args...> & args )
		: Replicated( recommended(), args )
		{}

	protected:
		std::vector< std::unique_ptr< Synchronized<T> > > instances;

		/// Helper functor to allocate/construct new objects from tuple args.
		///
		struct make_instance {
			template <typename ...Args>
			Synchronized<T> * operator() ( Args && ... args )
			{
				return new Synchronized<T>{ std::forward<Args>( args )... };
			}
		};
	};

} }
