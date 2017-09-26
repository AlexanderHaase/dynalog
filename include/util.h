#pragma once

#include <utility>
#include <mutex>
#include <vector>
#include <thread>
#include <dynalog/include/Buffer.h>

namespace dynalog {

	/// Scope wrapper for mutex.
	///
	/// Usage: with( mutex, <lambda> ) -> <lambda result>
	///
	/// @tparam Func function to apply in mutex scope.
	/// @param mutex Protect func invoctation.
	/// @param func Invoke with mutex locked.
	/// @return Result of function.
	///
	template < typename Func >
	auto with( std::mutex & mutex, Func && func ) -> decltype( func() )
	{
		std::unique_lock<std::mutex> lock( mutex );
		return func();
	}

	/// C++14 adds std::make_index_sequence< N >
	///
	template < size_t Current, size_t ... Remainder >
	struct IndexSequence {};

	template < size_t N, size_t ...Seq>
	struct GenerateIndexSequence : GenerateIndexSequence< N-1, N-1, Seq... >{};

	template < size_t ...Seq >
	struct GenerateIndexSequence<0, Seq...> { using type = IndexSequence<Seq...>; };

	template < typename Func, typename ...Args, size_t ...Selected >
	auto apply_tuple( const std::tuple<Args...> & value,
		Func && func, 
		IndexSequence<Selected...> selected )
		-> decltype( func( std::declval<Args>()... ) )
	{
			return func( std::get<Selected>( value )... );
	}

	template < typename Func, typename ...Args >
	auto apply_tuple( const std::tuple<Args...> & value, Func && func ) -> decltype( func( std::declval<Args>()... ) )
	{
			return apply_tuple( value, std::forward<Func>( func ), typename GenerateIndexSequence<sizeof...(Args)>::type{} );
	}

	template < typename T >
	typename std::add_lvalue_reference<T>::type declref() noexcept;


	/// c++11 helper: https://stackoverflow.com/questions/8640393/move-capture-in-lambda
	///
	template <typename T, typename F>
	class capture_impl
	{
	    T x;
	    F f;
	public:
	    capture_impl( T && x, F && f )
		: x{std::forward<T>(x)}, f{std::forward<F>(f)}
	    {}

	    template <typename ...Ts> auto operator()( Ts&&...args )
		-> decltype(f( x, std::forward<Ts>(args)... ))
	    {
		return f( x, std::forward<Ts>(args)... );
	    }

	    template <typename ...Ts> auto operator()( Ts&&...args ) const
		-> decltype(f( x, std::forward<Ts>(args)... ))
	    {
		return f( x, std::forward<Ts>(args)... );
	    }
	};

	template <typename T, typename F>
	capture_impl<T,F> capture( T && x, F && f )
	{
	    return capture_impl<T,F>(
		std::forward<T>(x), std::forward<F>(f) );
	}

	inline size_t threadindex( size_t modulo ) { return std::hash<std::thread::id>{}( std::this_thread::get_id() ) % modulo; }
}
