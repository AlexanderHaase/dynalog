#pragma once
#include <thread>
#include <dynalog/include/async/Synchronized.h>

namespace dynalog { namespace async {

	template < typename T >
	class Replicated {
	public:
		template < typename Func >
		auto with( Func && func ) -> decltype( func() )
		{
			const auto hash = std::hash<std::thread::id>{}( std::this_thread::get_id() );
			return at( hash, std::forward<Func>( func ) );
		}

		template < typename Func >
		auto at( const size_t hash, Func && func ) -> decltype( func() )
		{
			return instances[ hash % instances.size() ]->with( std::forward<Func>( func );
		}

		template < typename Generator >
		Replicated( const size_t size, Generator && gen )
		{
			instances.reserve( size );
			for( size_t index = 0; index < size; ++index )
			{
				instances.emplace_back( new T{ gen( index ) } );
			}
		}

		template < typename ... Args >
		Replicated( const size_t size, Args && ... args )
		{
			instances.reserve( size );
			for( size_t index = 0; index < size; ++index )
			{
				instances.emplace_back( new T{ std::forward<Args>( args )... } );
			}
		}

	protected:
		std::vector< std::unique_ptr< Synchronized<T> > > instances;
	};

} }
