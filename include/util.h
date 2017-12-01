#pragma once

#include <utility>
#include <mutex>
#include <thread>
#include <bitset>
#include <tuple>
#include <dynalog/include/NamedType.h>

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
  template < size_t ... Sequence >
  struct IndexSequence {};

  template < size_t N, size_t ...Seq>
  struct GenerateIndexSequence : GenerateIndexSequence< N-1, N-1, Seq... >{};

  template < size_t ...Seq >
  struct GenerateIndexSequence<0, Seq...> { using type = IndexSequence<Seq...>; };

  template < typename ...Args >
  using IndexSequenceFor = typename GenerateIndexSequence<sizeof...(Args)>::type;

  /// Use tuple elements in the sepecified index order as arguments to a callable/functor.
  ///
  template < typename Func, typename Tuple, size_t ...Selected >
  auto apply_tuple( Func && func,
    Tuple && tuple,
    IndexSequence<Selected...> )
    -> decltype( func( std::get<Selected>( tuple )... ) )
  {
      return func( std::get<Selected>( tuple )... );
  }

  /// Use a tuple as arguments to a callable/functor.
  ///
  template < typename Func, typename Tuple, typename AllIndices =
    typename GenerateIndexSequence< std::tuple_size<typename std::decay<Tuple>::type>::value >::type >
  auto apply_tuple( Func && func, Tuple && tuple )
    -> decltype( apply_tuple(std::forward<Func>( func ), std::forward<Tuple>( tuple ), AllIndices{} ) )
  {
      return apply_tuple( std::forward<Func>( func ), std::forward<Tuple>( tuple ), AllIndices{} );
  }

  template < typename ... >
  using void_t = void;

  template < typename Visitor, typename Tuple, size_t Index, typename Check = void_t<> >
  struct enumerate_visitor : std::false_type {};

  template < typename Visitor, typename Tuple, size_t Index>
  struct enumerate_visitor<Visitor, Tuple, Index, void_t<decltype( std::declval<Visitor>()( std::get<Index>( std::declval<Tuple>() ), Index ) )> > : std::true_type {};

  /// Apply a functor to the specified tuple index.
  ///
  /// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
  /// @tparam Tuple any type compatible with std::get< *some integer *>
  /// @tparam Index to process at this step.
  /// @param func Functor to apply.
  /// @param tuple Tuple to visit with functor.
  /// @param Index specifier for deduction.
  ///
  template< typename Func, typename Tuple, size_t Index>
  auto visit_tuple( Func && func, Tuple && tuple, IndexSequence<Index> )
    -> typename std::enable_if<enumerate_visitor<Func,Tuple,Index>::value>::type
  {
    func( std::get<Index>( tuple ), Index );
  }

  /// Apply a functor to the specified tuple index.
  ///
  /// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
  /// @tparam Tuple any type compatible with std::get< *some integer *>
  /// @tparam Index to process at this step.
  /// @param func Functor to apply.
  /// @param tuple Tuple to visit with functor.
  /// @param Index specifier for deduction.
  ///
  template< typename Func, typename Tuple, size_t Index >
  auto visit_tuple( Func && func, Tuple && tuple, IndexSequence<Index> )
    -> typename std::enable_if<!enumerate_visitor<Func,Tuple,Index>::value>::type
  {
    func( std::get<Index>( tuple ) );
  }

  /// Apply a functor to the specified tuple indices.
  ///
  /// In c++17, a fold expression would be equally viable...
  ///
  /// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
  /// @tparam Tuple any type compatible with std::get< *some integer *>
  /// @tparam Index to process at this step.
  /// @tparam ...Remainder indicies yet to process
  /// @param func Functor to apply.
  /// @param tuple Tuple to visit with functor.
  /// @param Index specifier for deduction.
  ///
  template< typename Func, typename Tuple, size_t Index, size_t ...Remainder >
  auto visit_tuple( Func && func, Tuple && tuple, IndexSequence<Index, Remainder...> )
    -> typename std::enable_if<(sizeof...(Remainder) > 0 ),void>::type
  {
    visit_tuple( func, tuple, IndexSequence<Index>{} );
    visit_tuple( std::forward<Func>( func ), std::forward<Tuple>( tuple ), IndexSequence<Remainder...>{} );
  }

  /// Apply a functor to all tuple indices.
  ///
  ///
  /// @tparam Func to apply to tuple elements(template/specialize/overload as needed)
  /// @tparam Tuple any type compatible with std::get< *some integer *>
  /// @param func Functor to apply.
  /// @param tuple Tuple to visit with functor.
  ///
  template< typename Func, typename Tuple >
  void visit_tuple( Func && func, Tuple && tuple )
  {
    using AllIndices = typename GenerateIndexSequence< std::tuple_size<typename std::decay<Tuple>::type>::value >::type;
    visit_tuple( std::forward<Func>( func ), std::forward<Tuple>( tuple ), AllIndices{} );
  }

  /// Complement to std::declval<T>()
  ///
  /// In some expressions, std::add_rvalue_reference<T>::type is illegal!
  /// 
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
      capture_impl( T && x_arg, F && f_arg )
    : x{std::forward<T>(x_arg)}, f{std::forward<F>(f_arg)}
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

  /// Get the indexable identity of this thread.
  ///
  inline size_t threadindex( void ) { return std::hash<std::thread::id>{}( std::this_thread::get_id() ); }

  /// Get the indexable identity of this thread modulo a number.
  ///
  inline size_t threadindex( size_t modulo ) { return threadindex() % modulo; }

  /// Create a set that combines enum values.
  ///
  template < typename Enum, size_t Qty >
  struct EnumSet : protected std::bitset<Qty>
  {
    using All = NamedType<void,struct AllParameter>;
    using Base = std::bitset<Qty>;
    using Base::Base;

    EnumSet( std::initializer_list<Enum> list )
    : Base( 0ul )
    {
      for( auto && value : list )
      {
        set( value, true );
      }
    }

    EnumSet & set( const Enum bit, bool value )
    {
      Base::set( static_cast<size_t>( bit ), value );
      return *this;
    }

    EnumSet & set( const All, bool value )
    {
      if( value ) { Base::set(); } else { Base::reset(); }
      return *this;
    }

    bool get( const Enum bit ) const
    {
      return Base::test( static_cast<size_t>( bit ) );
    }

    EnumSet & operator += ( const Enum bit )
    {
      return set( bit, true );
    }

    EnumSet & operator -= ( const Enum bit )
    {
      return set( bit, true );
    }
  };
}
