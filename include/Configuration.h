#pragma once

#include <mutex>
#include <bitset>
#include <memory>
#include <map>
#include <vector>
#include <unordered_set>

#include <dynalog/include/Logger.h>

namespace dynalog {

	/// Configuration manager for a set of loggers.
	///
	/// Loggers are configured by the highest priority policy that matches
	/// the logger at any given time. Since loggers emphasize fast logging,
	/// configuration changes compute and propagate state at configuration-
	/// time to loggers.
	///
	/// == Operation ==
	///
	/// Loggers may be inserted or removed at any time, they will be
	/// retained as long as at least one policy matches them.
	///
	/// Policies are evaluated only at specific times:
	///
	///   - At logger insertion/removal: The specific logger in question is
	///     evaluated against current policy set.
	///
	///   - At policy insertion: If a policy matches any logger managed by
	///     a lower priority logger, it takes over that logger.
	///
	///   - At policy removal: If any lower priority policies match a
	///     a logger managed by the removed policy, the highest priority
	///     takes over that logger.
	///
	///   - At policy rescan: During rescan, all loggers managed by a
	///     policy are re-evaluated, and all loggers at a lower priority 
	///     are re-evaluted. Additionally, any loggers orphaned from the 
	///     policy are evaluated against lower priority loggers.
	///
	class Configuration {
	public:
		using LoggerSet = std::unordered_set< std::shared_ptr<Logger> >;
		using LoggerVector = std::vector<std::shared_ptr<Logger> >;

		/// Membership changes for a policy
		///
		struct ChangeSet 
		{
			LoggerSet insert;	///< Loggers new to the policy.
			LoggerSet remove;	///< Loggers to drop from the policy.
			LoggerSet manage;	///< All other loggers managed by the policy.

			struct Insert {};
			struct Remove {};

			/// Applies indicated changes to the managed set.
			///
			inline void apply( void )
			{
				manage.insert( insert.begin(), insert.end() );
				remove.clear();
				insert.clear();
			}

			/// Rewinds pending changes to the managed set.
			///
			inline void reset( void )
			{
				manage.insert( remove.begin(), remove.end() );
				remove.clear();
				insert.clear();
			}

			/// Check if any changes are pending
			///
			bool pending( void ) const { return !remove.empty() || !insert.empty(); }

			/// Add the collection to the insert set.
			///
			template < typename Collection >
			void update( Insert, const Collection & loggers )
			{
				insert.insert( loggers.begin(), loggers.end() );
			}

			/// Add the collection to the remove set.
			///
			template < typename Collection >
			void update( Remove, const Collection & loggers )
			{
				for( auto && logger : loggers )
				{
					manage.erase( logger );
				}
				remove.insert( loggers.begin(), loggers.end() );
			}
		};
			
		/// Add a logger to the configuration
		///
		/// Without a default policy, it's possible for this to return false.
		///
		/// @param logger Instance to add.
		/// @return true if matched by a policy, false if otherwise.
		///
		bool insert( const std::shared_ptr<Logger> & logger );

		/// Remove a logger from configuration.
		///
		/// @param logger Instance to remove.
		/// @return false if logger is already unmanaged, true otherwise.
		///
		bool remove( const std::shared_ptr<Logger> & logger );

		struct Policy
		{
			virtual ~Policy() {}

			/// Given a set of loggers, select those that this policy matches
			///
			/// @param loggers Set of loggers to search for matches.
			/// @param matches Vector of matched loggers.
			///
			virtual void match( const LoggerSet & loggers, LoggerVector & matches ) = 0;

			/// Update the policy internal state based on a change set.
			///
			/// @param changes ChangeSet describing current policies.
			///
			virtual void update( const ChangeSet & changes ) = 0;
		};

		/// Insert a new policy, overriding lower-priority policies.
		///
		/// Loggers at a lower priority matched by the new policy will be
		/// moved to the new policy.
		///
		/// @param priority Higher priority policies take presidence.
		/// @param policy Policy instance to insert.
		/// @return false on conflict, true otherwise.
		///
		bool insert( int priority, const std::shared_ptr<Policy> & policy );

		/// Remove a policy, re-evaluating orphaned loggers.
		///
		/// @param priority Must match insertion priority.
		/// @param policy Instance to remove.
		/// @return false on mismtach, true otherwise.
		///
		bool remove( int priority, const std::shared_ptr<Policy> & policy );

		/// Re-evalutate policy matches for the policy at the given priority.
		///
		/// Orphaned loggers are offered to lower priority policies.
		///
		/// @param priority Priority of the policy to rescan.
		/// @return Boolean indication of if a policy was matched.
		///
		bool rescan( int priority );

		/// Remind the given priority which loggers it manages.
		///
		/// @param priority Priority of the policy to remind.
		/// @return Boolean indication of if a policy was matched.
		///
		bool update( int priority );

	protected:
		/// Storage associating loggers to a policy.
		///
		/// Helper functions extract common logic.
		///
		struct Node
		{
			std::shared_ptr<Policy> policy;
			ChangeSet changes;

			/// Accept loggers matched by this node, eliminating them from the offering.
			///
			void adopt( LoggerSet & set, LoggerVector & scratch );

			/// Have the policy rescan it's own managed state.
			///
			void rescan( LoggerVector & scratch );

			/// Update the policy and collapse changes
			///
			void update( bool force = false );

			/// Steal matched loggers form the other node
			///
			void override( Node & other, LoggerVector & scratch );
		};

		std::mutex mutex;
		LoggerVector scratch;
		std::map<int, Node, std::greater<int> > policies;
	};

	/// Match-all policy applies a single configuration to all matched loggers.
	///
	template < typename Predicate >
	class PredicatePolicy : public Configuration::Policy, protected Predicate {
	public:
		/// Allways matches.
		///
		/// @param loggers Set of loggers to search for matches.
		/// @param matches Vector of matched loggers.
		///
		virtual void match( const Configuration::LoggerSet & loggers, Configuration::LoggerVector & matches )
		{
			for( auto && logger : loggers )
			{
				if( Predicate::operator() ( logger ) )
				{
					matches.emplace_back( logger );
				}
			}
		}

		/// Update the policy internal state based on a change set.
		///
		/// @param changes ChangeSet describing current policies.
		///
		virtual void update( const Configuration::ChangeSet & changes )
		{
			for( auto && logger : changes.insert )
			{
				logger->emitter.store( emitter, std::memory_order_relaxed );
				logger->levels = levels;
			}

			for( auto && logger : changes.manage )
			{
				logger->emitter.store( emitter, std::memory_order_relaxed );
				logger->levels = levels;
			}
		}

		/// Configure the emitter for loggers matched by this policy.
		///
		/// Call Configuration::update() to take effect.
		///
		/// @param instance Emitter to configure for matched loggers.
		///
		void configure( Emitter * instance )
		{
			emitter = instance;
		}

		/// Configure enabled log levels for loggers matched by this policy.
		///
		/// Call Configuration::update() to take effect.
		///
		/// @param enabled LevelSet of enabled log levels to configure for matched loggers.
		///
		void configure( const LevelSet & enabled )
		{
			levels = enabled;
		}

		template < typename ...Args >
		PredicatePolicy( Emitter * instance, const LevelSet & enabled, Args && ...args )
		: Predicate( std::forward<Args>( args )... )
		, emitter( instance )
		, levels( enabled )
		{}

		operator Predicate &(void) { return static_cast<Predicate&>( *this ); }

	protected:
		Emitter * emitter;
		LevelSet levels;
	};

	/// Helper function to create a predicate policy
	///
	template < typename Predicate >
	auto make_policy( Emitter * instance, const LevelSet & enabled, Predicate && pred )
		-> std::shared_ptr<PredicatePolicy<Predicate>>
	{
		return std::make_shared<PredicatePolicy<Predicate>>( instance, enabled, std::forward<Predicate>( pred ) );
	}

	struct MatchAll { inline bool operator() (const std::shared_ptr<Logger> &) const { return true; } };

	/// Match-all policy applies a single configuration to all matched loggers.
	///
	struct DefaultPolicy : public PredicatePolicy<MatchAll>
	{
		using Base = PredicatePolicy<MatchAll>;
		using Base::Base;
	};

	template < typename Action >
	bool visit( Configuration & configuration, Action && action )
	{
		auto policy = std::static_pointer_cast<Configuration::Policy>( 
			make_policy( nullptr, LevelSet{}, capture( action,
				[]( Action & action, const std::shared_ptr<Logger> & logger )
				{
					action( logger );
					return false;
				})));
		const auto priority = std::numeric_limits<int>::max();
		const auto result = configuration.insert( priority, policy );
		if( result )
		{
			configuration.remove( priority, policy );
		}
		return result;
	}
}

