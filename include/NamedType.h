#pragma once

namespace dynalog {

	/// Named type wrapper for more explicit interfaces.
	///
	/// Influence : http://www.fluentcpp.com/2016/12/08/strong-types-for-strong-interfaces/
	///
	template < typename T, typename Parameter >
	class NamedType {
	public:
		explicit NamedType( const T & value )
		: instance( value )
		{}

		template < typename Check = T, typename = typename std::enable_if<!std::is_reference<Check>::value>::type >
		explicit NamedType( Check && value )
		: instance( std::forward<Check>( value ) )
		{}

		explicit operator T& () { return value; }
		explicit operator const T & () { return value; }

		T & value() { return value; }
		const T& value() const { return value; }

	protected:
		T instance;
	};
}