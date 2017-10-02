#pragma once

#include <chrono>
#include <ios>
#include <ostream>
#include <dynalog/include/NamedType.h>

namespace dynalog { namespace timestamp {

	/// Given a time_point, break it down into component parts.
	///
	/// Atoms uses platform libraries to convert to/from component parts
	/// accounting for leap seconds, variation in month duration, etc.
	///
	/// Components are represented via *APPROXIMATE RATIOs* holding *ACTUAL
	/// VALUES*. Naively combinining components creates *ERRONEOUS VALUES*.
	///
	struct Atoms {
		using Year = std::chrono::duration<int,std::ratio<31536000>>;
		using Month = std::chrono::duration<int,std::ratio<2592000>>;
		using Day = std::chrono::duration<int,std::ratio<86400>>;
		using Hour = std::chrono::hours;
		using Minute = std::chrono::minutes;
		using Nanoseconds = std::chrono::nanoseconds;

		Year year;
		Month month;
		Day day;
		Hour hour;
		Minute minute;
		Nanoseconds nanoseconds;

		/// Decompose the specfied timepoint
		///
		Atoms( const std::chrono::system_clock::time_point & );

		/// Accurately convert the components to a time_point.
		///
		operator std::chrono::system_clock::time_point( void ) const;
	};

	/// Getter helper funtion for working with Atoms in templates.
	///
	/// Can template based off the time type and use get<..> to select it
	/// from Atoms.
	///
	/// @tparam Type time type to extract from Atoms.
	///
	template < typename Type >
	const Type & get( const Atoms & atoms );

	// Specializations implementing get<...>( Atoms & )
	//
	template <>
	const Atoms::Year & get<Atoms::Year>( const Atoms & atoms ) { return atoms.year; }

	template <>
	const Atoms::Month & get<Atoms::Month>( const Atoms & atoms ) { return atoms.month; }

	template <>
	const Atoms::Day & get<Atoms::Day>( const Atoms & atoms ) { return atoms.day; }

	template <>
	const Atoms::Hour & get<Atoms::Hour>( const Atoms & atoms ) { return atoms.hour; }

	template <>
	const Atoms::Minute & get<Atoms::Minute>( const Atoms & atoms ) { return atoms.minute; }

	template <>
	const Atoms::Nanoseconds & get<Atoms::Nanoseconds>( const Atoms & atoms ) { return atoms.nanoseconds; }

	/// Time formatting helpers.
	///
	/// Rather than parse/modify/repeat strftime formats, dynalog takes
	/// a template approach to formatting: Formatters are functors accepting
	/// a std::ostream and an Atoms. The format namespace provides mix-in
	/// types to construct arbitrary formatters:
	///
	///  - StreamOps specify how to format a single field of Atoms. Width,
	///    Fill, Precision, etc. modify the output stream format.
	///  - Fragments<...> specify the Atoms field and stream ops for
	///    outputing. It can also do type conversion.
	///      - Defaults provided for Year, Month, Day, Hour, Minute, Second.
	///  - L<...> expresses constant values.
	///  - Formatter<...> bundles a sequence of Fragments<...>, L<...>, and
	///    any type acting on std::ostream, Atoms.
	///      - Default provided for ISO_8601.
	///  - Proxy provides a way to virtualize/run-time select formatting.
	///  - Dynmaic wraps a Formatter<...> for use with Proxy.
	///
	namespace format {

		/// Set fill character: ostream::fill( Val )
		///
		template< char Val >
		struct Fill
		{
			void operator() ( std::ostream & stream ) const
			{
				stream.fill( Val );
			}
		};

		/// Set fill width: ostream::width( Val )
		///
		template< size_t Val >
		struct Width
		{
			void operator() ( std::ostream & stream ) const
			{
				stream.width( Val );
			}
		};

		/// Set float precision: ostream::precision( Val )
		///
		template< size_t Val >
		struct Precision
		{
			void operator() ( std::ostream & stream ) const
			{
				stream.precision( Val );
			}
		};

		/// Set one or more format flags on a stream.
		///
		/// Not an enum class--use: std::ios_base::<flag name>
		///
		template < std::ios_base::fmtflags Current, std::ios_base::fmtflags ...Remainder >
		struct Flags : Flags< Remainder...>
		{
			void operator() ( std::ostream & stream ) const
			{
				stream.setf( Current );
				Flags< Remainder... >::operator() ( stream );
			}
		};

		/// Terminate recursion for setting flags.
		///
		template < std::ios_base::fmtflags Last >
		struct Flags<Last>
		{
			void operator() ( std::ostream & stream ) const
			{
				stream.setf( Last );
			}
		};

		/// Type wrapper to disambiguate repeated base classes.
		///
		/// Example: directly using ISO_8601's L<char,':'> base is
		/// ambiguous because ISO_8601 uses it as base class twice.
		/// Virtual inheritance isn't appropriate: instances are
		/// allowed to carry run-time variables that may diverge
		/// among repeated bases. Instead, repeated bases are type-
		/// wrapped with a unique index:
		///   - IndexedType< 3, L<char,':'> >::operator() ( ... )
		///   - IndexedType< 5, L<char,':'> >::operator() ( ... )
		///
		/// @tparam Index identifying type.
		/// @tparam Type to index/derrive.
		///
		template < size_t Index, typename Type >
		struct IndexedType : Type {};

		/// Implementation of ApplyAll: uses IndexType<...> to
		/// wrap/chain Functor types.
		///
		/// @tparma Index of current type.
		/// @tparam Current type to wrap/chain.
		/// @tparam Remainder types to wrap/chain.
		///
		template < size_t Index, typename Current, typename ...Remainder >
		struct ApplyAll_Impl : IndexedType<Index, Current>, ApplyAll_Impl<Index - 1, Remainder... >
		{
			template < typename ... Args >
			void operator() ( Args && ... args ) const
			{
				IndexedType<Index,Current>::operator() ( args... );
				ApplyAll_Impl<Index - 1, Remainder...>::operator() ( args... );
			}	
		};

		/// Terminate recursion.
		///
		/// @tparma Index of current type.
		/// @tparam Last type to wrap/chain.
		///
		template < size_t Index, typename Last >
		struct ApplyAll_Impl<Index,Last> : IndexedType<Index,Last> {};

		/// Chain functor invocations in-order.
		///
		/// @tparam All Functor types to chain.
		///
		template < typename ...All >
		struct ApplyAll : ApplyAll_Impl<sizeof...(All), All...> {};

		/// Formatter specification Fragment.
		///
		/// @tparam Unit Atoms::<Unit> type to format.
		/// @tparam RepType Type to express Unit as to ostream.
		/// @tparam StreamOps One or more stream format specifiers.
		///
		template< typename Unit, typename RepType, typename ...StreamOps >
		struct Fragment : ApplyAll<StreamOps...>
		{
			void operator() ( std::ostream & stream, const Atoms & atoms ) const
			{
				ApplyAll<StreamOps...>::operator() ( stream );
				stream << RepType{get<Unit>( atoms ).count()};
			}
		};

		/// Formatter specification Fragment.
		///
		/// Specialization for std::chrono::duration RepType.
		///
		/// @tparam Unit Atoms::<Unit> type to format.
		/// @tparam Type Rep type for std::chrono::duration.
		/// @tparam Ratio Ratio type for std::chrono::duration.
		/// @tparam StreamOps One or more stream format specifiers.
		///
		template< typename Unit, typename Type, typename Ratio, typename ...StreamOps >
		struct Fragment<Unit, std::chrono::duration<Type,Ratio>, StreamOps...> : ApplyAll<StreamOps...>
		{
			void operator() ( std::ostream & stream, const Atoms & atoms ) const
			{
				ApplyAll<StreamOps...>::operator() ( stream );
				stream << std::chrono::duration<Type,Ratio>{get<Unit>( atoms )}.count();
			}
		};

		/// Insert a literal value into an input stream.
		///
		/// TODO: Expand to handle more literals.
		///
		/// @tparam Type Type of literal to insert.
		/// @tparam Val Literal to insert into the format.
		///
		template < typename Type, Type Val >
		struct L
		{
			void operator() ( std::ostream & stream, const Atoms & ) const
			{
				stream << Val;
			}
		};

		/// Bundle one or more Fragments/Literals/equivalent types.
		///
		/// Saves and restores stream state between applying/formatting
		/// an Atoms instance.
		///
		/// @tparam Parts Sequence of Fragments/Literals/etc. to use.
		///
		template< typename ...Parts >
		struct Formatter : ApplyAll<Parts...>
		{
			void operator() ( std::ostream & stream, const Atoms & atoms ) const
			{
				std::ios state( nullptr );
				state.copyfmt( stream );
				ApplyAll<Parts...>::operator() ( stream, atoms );
				stream.copyfmt( state );
			}
		};

		// Built-in format Fragments
		//
		using Year	= Fragment< Atoms::Year, Atoms::Year::rep, Width<4> >;
		using Month	= Fragment< Atoms::Month, Atoms::Month::rep, Width<2>, Fill<'0'> >;
		using Day	= Fragment< Atoms::Day, Atoms::Day::rep, Width<2>, Fill<'0'> >;
		using Hour	= Fragment< Atoms::Hour, Atoms::Hour::rep, Width<2>, Fill<'0'> >;
		using Minute	= Fragment< Atoms::Minute, Atoms::Minute::rep, Width<2>, Fill<'0'> >;
		using Second	= Fragment< Atoms::Nanoseconds,
			std::chrono::duration< float, std::ratio<1> >, 
			Width<6>,
			Fill<'0'>,
			Flags<std::ios_base::fixed>,
			Precision<3> >;

		/// Built-in ISO_8601 formatter.
		///
		using ISO_8601 = Formatter< Year, L<char,'-'>, Month, L<char,'-'>, Day,
			L<char,'T'>, Hour, L<char,':'>, Minute, L<char,':'>, Second,
			L<char,'Z'> >;

		/// Proxy format application through a virtual implementation.
		///
		struct Proxy
		{
			/// Interface for virtual formatting.
			///
			struct Impl
			{
				virtual ~Impl() = default;
				virtual void operator() ( std::ostream &, const Atoms & ) const = 0;
			};

			/// Dispatch formatting call to implementation.
			///
			inline void operator() ( std::ostream & stream, const Atoms & atoms ) const
			{
				impl( stream, atoms );
			}
			Impl & impl;
		};

		/// Wrap a static formatter into a virtual formatter.
		///
		/// Example:
		///  auto impl = std::make_shared<Dynamic<ISO_8601>>( ISO_8601{} );
		///  auto timestamp = Lazy<format::Proxy>{ Proxy{ impl.get() } };
		///
		/// @tparam Formatter to wrap.
		///
		template < typename Formatter >
		struct Dynamic : Proxy::Impl, Formatter
		{
			Dynamic( Formatter && formatter )
			: Formatter( std::forward<Formatter>( formatter ) )
			{}

			virtual void operator() ( std::ostream & stream, const Atoms & atoms ) const
			{
				Formatter::operator() ( stream, atoms );
			}
		};
	};

	/// Get the fastest coarse system time available.
	///
	std::chrono::system_clock::time_point coarse();

	/// Specify the timestamp time immediately to be serialized later.
	///
	template< typename Formatter = format::ISO_8601 >
	struct Fixed : Formatter
	{
		/// Create a timestamp with a fixed time.
		///
		/// @param at Time to capture, default now.
		///
		template < typename = decltype( Formatter{} ) >
		Fixed( const std::chrono::system_clock::time_point & at = coarse() )
		: time( at )
		{}

		/// Create a timestamp with a fixed time, passing format args.
		///
		/// @param format Formatter to capture/use.
		/// @param at Time to capture, default now.
		///
		Fixed( Formatter && format, const std::chrono::system_clock::time_point & at = coarse() )
		: Formatter( std::forward<Formatter>( format ) )
		, time( at )
		{}

		/// Capture time_point.
		///
		const std::chrono::system_clock::time_point time;
	};

	/// Lazily timestamp when serialized.
	///
	template< typename Formatter = format::ISO_8601 >
	struct Lazy : Formatter
	{
		/// Create a sentinal to timestamp when serialized.
		///
		template < typename = decltype( Formatter{} ) >
		Lazy() {}

		/// Create a sentinal to timestamp when serialized, passing format args.
		///
		/// @param format Formatter to capture/use.
		///
		Lazy( Formatter && other )
		: Formatter( std::forward<Formatter>( other ) )
		{}
	};

	/// Serialize the timestamp per it's specified format.
	///
	template < typename Formatter >
	std::ostream & operator << ( std::ostream & stream, const Fixed<Formatter> & fixed )
	{
		Atoms atoms{ fixed.time };
		fixed( stream, atoms );
		return stream;
	}

	/// Serialize the timestamp per it's specified format.
	///
	template < typename Formatter >
	std::ostream & operator << ( std::ostream & stream, const Lazy<Formatter> & lazy )
	{
		Atoms atoms{ coarse() };
		lazy( stream, atoms );
		return stream;
	}
} }
