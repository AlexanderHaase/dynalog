#pragma once
#include <mutex>

namespace dynalog { namespace async {

	template < typename T >
	class Synchronized {
	public:
		template < typename Func >
		auto with( Func && func ) -> decltype( func() )
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			return func( variable );
		}

		template < typename ... Args >
		Synchronized( Args && ... args )
		: variable( std::forward<Args>( args )... )
		{}

	protected:
		std::mutex mutex;
		T variable;
	};
} }
