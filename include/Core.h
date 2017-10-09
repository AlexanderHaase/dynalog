#pragma once
#include <dynalog/include/util.h>

namespace dynalog {
 
	/// Strong type for Location(unique identifier string)
	///
	using Location = NamedType<const char *, struct LocationParam>;

	/// Strong type for Context(grouping identifier)
	///
	using Context = NamedType<const char *, struct ContextParam>;

	enum class Level {
		CRITICAL,
		ERROR,
		WARNING,
		INFO,
		VERBOSE,
		LEVEL_QTY
	};

	using LevelSet = EnumSet<Level,static_cast<size_t>(Level::LEVEL_QTY)>;
}



::std::ostream & operator << ( ::std::ostream & stream, ::dynalog::Level level );
