#include <iostream>
#include <limits>
#include <dynalog/include/Log.h>
#include <dynalog/include/HandleEmitter.h>

namespace dynalog {

	/// Tie embedded loggers into the global configuration
	///
	struct BootstrapEmitter : Emitter
	{
		virtual ~BootstrapEmitter() {}
		virtual void emit( const Logger & logger, Message && message )
		{
			// safely wrap embedded logger in shared_ptr with no-op deleter
			//
			std::shared_ptr<Logger> ptr{ const_cast<Logger*>(&logger), [](Logger*){} };
			global::configuration.insert( ptr );

			// Emit message if one was configured
			//
			auto emitter = logger.emitter.load( std::memory_order_relaxed );
			if( emitter )
			{
				emitter->emit( logger, std::move( message ) );
			}
		}
	};

	static BootstrapEmitter bootstrapper;
	Emitter * const EmbeddedLoggerInit = &bootstrapper;

	namespace global {

		Configuration configuration;
		DefaultPolicy policy;


		namespace {
		 	struct Initializer
			{
				Initializer()
				{
					// Configure the default policy
					//
					using Policy = Configuration::Policy;
					std::shared_ptr<Policy> ptr{ &policy, []( Policy * ){} };
					configuration.insert( std::numeric_limits<int>::min(), ptr );
					policy.configure( HandleEmitter::stdout );
				}

			};
			Initializer initialize{};
		}
	}	
}



::std::ostream & operator << ( ::std::ostream & stream, ::dynalog::Level level )
{
	static const std::array<const char *, 5> names{ "CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE" };
	const size_t index = static_cast<size_t>( level );
	return index < names.size() ? stream << names[ index ] : stream << "<invalid ::dynalog::Level(" << index << ")>";
}
