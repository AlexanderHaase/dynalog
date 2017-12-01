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
		virtual void emit( const Logger & logger, Message && message ) override
		{
			// Safely wrap embedded logger in shared_ptr with no-op deleter
			//
			std::shared_ptr<Logger> ptr{ const_cast<Logger*>(&logger), [](Logger*){} };
			global::configuration.insert( ptr );

      // Try to re-check level using reflection
      //
      const auto & inspector = message.content().inspect();
      const auto limit = inspector.size();
      for( size_t index = 0; index < limit; ++index )
      {
        const auto reflection = inspector.reflect( index );
        if( !reflection.is<Level>() )
        {
          continue;
        }
        else if( logger.levels.get( reflection.as<Level>() ) )
        {
          break;
        }
        else
        {
          return;
        }
      }

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
		static DefaultPolicy policy{ HandleEmitter::stdout, LevelSet{ DYNALOG_DEFAULT_LEVELS } };
		static const int priority = std::numeric_limits<int>::min();

		void setDefault( Emitter * emitter )
		{
			policy.configure( emitter );
			configuration.update( priority );
		}
		void setDefault( const LevelSet & levels )
		{
			policy.configure( levels );
			configuration.update( priority );
		}
		namespace {
		 	struct Initializer
			{
				Initializer()
				{
					// Configure the default policy
					//
					using Policy = Configuration::Policy;
					std::shared_ptr<Policy> ptr{ &policy, []( Policy * ){} };
					configuration.insert( priority, ptr );
				}

			};
			Initializer initialize{};
		}
	}	
}



::std::ostream & operator << ( ::std::ostream & stream, ::dynalog::Level level )
{
	static const std::array<const char *, 5> names{ { "CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE" } };
	const size_t index = static_cast<size_t>( level );
	return index < names.size() ? stream << names[ index ] : stream << "<invalid ::dynalog::Level(" << index << ")>";
}
