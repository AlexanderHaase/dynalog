#pragma once
#include <mutex>
#include <dynalog/include/util.h>

namespace dynalog { namespace async {

	/// Object wrapper for synchronized access.
	///
	/// Clients pass a function into be evaluated in the synchronized
	/// context:
	///
	///	// Create a synchronized int with value 0.
	///	//	
	///	Synchronized<int> myInt{ 0 };
	///
	///	// Use a lambda to increment by 2 and return the value;
	///	//
	///	auto result = myInt.with( [&]( int & value ) { return value += 2 } );
	///
	template < typename T >
	class Synchronized {
	public:
		/// Apply a visitor function to the contained value under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function.
		///
		template < typename Func >
		auto with( Func && func ) -> decltype( func( declref<T>() ) )
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			return func( variable );
		}

		/// Apply a visitor function to the contained value under lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function.
		///
		template < typename Func >
		auto with( Func && func ) -> decltype( func( declref<T>(), declref<std::unique_lock<std::mutex>>() ) )
		{
			std::unique_lock<std::mutex> lock( mutex, std::try_to_lock );
			return func( variable, lock );
		}

		/// Apply a visitor function to the contained value without lock.
		///
		/// @tparam Func type of visitor to apply--deduced.
		/// @param func Visitor to apply to contained value.
		/// @return Result of visitor function.
		///
		template < typename Func >
		auto unprotected( Func && func ) -> decltype( func( declref<T>() ) )
		{
			return func( variable );
		}

		/// Create a synchronized instance forwarding arguments.
		///
		template < typename ... Args >
		Synchronized( Args && ... args )
		: variable( std::forward<Args>( args )... )
		{}

	protected:
		std::mutex mutex;
		T variable;
	};
} }
