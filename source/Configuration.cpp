#include <dynalog/include/Configuration.h>

namespace dynalog {
	
	/// Add a logger to the configuration
	///
	/// Without a default policy, it's possible for this to return false.
	///
	/// @param logger Instance to add.
	/// @return true if matched by a policy, false if otherwise.
	///
	bool Configuration::insert( const std::shared_ptr<Logger> & logger )
	{
		LoggerSet available;
		available.insert( logger );

		std::unique_lock<std::mutex> lock( mutex );
		for( auto && pair : policies )
		{
			pair.second.adopt( available, scratch );
			if( available.empty() )
			{
				pair.second.update();
				break;
			}
		}
		return available.empty();
	}

	/// Remove a logger from configuration.
	///
	/// @param logger Instance to remove.
	/// @return false if logger is already unmanaged, true otherwise.
	///
	bool Configuration::remove( const std::shared_ptr<Logger> & logger )
	{
		std::unique_lock<std::mutex> lock( mutex );
		for( auto && pair : policies )
		{
			if( pair.second.changes.manage.erase( logger ) )
			{
				pair.second.changes.remove.insert( logger );
				pair.second.update();
				return true;
			}
		}
		return false;
	}

	/// Insert a new policy, overriding lower-priority policies.
	///
	/// Loggers at a lower priority matched by the new policy will be
	/// moved to the new policy.
	///
	/// @param priority Higher priority policies take presidence.
	/// @param policy Policy instance to insert.
	/// @return false on conflict, true otherwise.
	///
	bool Configuration::insert( int priority, std::shared_ptr<Policy> && policy )
	{
		std::unique_lock<std::mutex> lock( mutex );

		auto result = policies.emplace( priority, Node{ policy, ChangeSet{} } );

		if( result.second )
		{
			const auto node = result.first;
			auto iter = node;

			// Gather loggers to steal from lower priority policies.
			//
			for( ++iter; iter != policies.end(); ++iter )
			{
				node->second.assume( iter->second, scratch );
			}

			// Apply removals, then inserts
			//
			iter = node;
			for( ++iter; iter != policies.end(); ++iter )
			{
				iter->second.update();
			}
			node->second.update();
		}
		return result.second;
	}

	/// Remove a policy, re-evaluating orphaned loggers.
	///
	/// @param priority Must match insertion priority.
	/// @param policy Instance to remove.
	/// @return false on mismtach, true otherwise.
	///
	bool Configuration::remove( int priority, std::shared_ptr<Policy> && policy )
	{
		std::unique_lock<std::mutex> lock( mutex );

		const auto node = policies.find( priority );
		const bool result = node != policies.end() && node->second.policy == policy;

		if( result )
		{
			auto iter = node;

			// Offer loggers to lower priority policies.
			//
			for( ++iter; iter != policies.end(); ++iter )
			{
				iter->second.assume( node->second, scratch );
			}

			// Move remaining loggers to remove state
			//
			node->second.changes.remove.insert( node->second.changes.manage.begin(), node->second.changes.manage.end() );
			node->second.changes.manage.clear();

			// Update all policies
			//
			for( iter = node; iter != policies.end(); ++iter )
			{
				iter->second.update();
			}

			policies.erase( node );
		}
		return result;
	}

	/// Re-evalutate policy matches for the policy at the given priority.
	///
	/// Orphaned loggers are offered to lower priority policies.
	///
	bool Configuration::rescan( int priority )
	{
		std::unique_lock<std::mutex> lock( mutex );

		const auto node = policies.find( priority );
		const bool result = node != policies.end();

		if( result )
		{
			// Find orphans and copy to whittle down
			//
			node->second.rescan( scratch );
			auto orphans = node->second.changes.remove;

			// scan lower priority policies
			//
			auto iter = node;
			for( ++iter; iter != policies.end(); ++iter )
			{
				iter->second.adopt( orphans, scratch );
				node->second.assume( iter->second, scratch );
			}

			// Carefully update policies--insert only after removal
			//
			auto delayed = std::move( node->second.changes.insert );

			for( iter = node; iter != policies.end(); ++iter )
			{
				iter->second.update();
			}

			node->second.changes.insert = std::move( delayed );
			node->second.update( true );
		}
		return result;
	}



	/// Remind the given priority which loggers it manages.
	///
	/// @param priority Priority of the policy to remind.
	/// @return Boolean indication of if a policy was matched.
	///
	bool Configuration::update( int priority )
	{
		std::unique_lock<std::mutex> lock( mutex );

		const auto node = policies.find( priority );
		const bool result = node != policies.end();

		if( result )
		{
			node->second.update( true );
		}
		return result;
	}

	/// Accept loggers matched by this node, eliminating them from the offering.
	///
	void Configuration::Node::adopt( LoggerSet & set, LoggerVector & scratch_space )
	{
		policy->match( set, scratch_space );
		changes.update( ChangeSet::Insert{}, scratch_space );
		for( auto && logger : scratch_space )
		{
			set.erase( logger );
		}
		scratch_space.clear();
	}

	/// Have the policy rescan it's own managed state.
	///
	void Configuration::Node::rescan( LoggerVector & scratch_space )
	{
		policy->match( changes.manage, scratch_space );
		std::swap( changes.manage, changes.remove );
		for( auto && logger : scratch_space )
		{
			changes.manage.insert( logger );
			changes.remove.erase( logger );
		}
		scratch_space.clear();
	}

	/// Update the policy and collapse changes
	///
	void Configuration::Node::update( bool force )
	{
		if( changes.pending() || force )
		{
			policy->update( changes );
			changes.apply();
		}
	}

	/// Steal matched loggers form the other node
	///
	void Configuration::Node::assume( Node & other, LoggerVector & scratch_space )
	{
		policy->match( other.changes.manage, scratch_space );
		changes.update( ChangeSet::Insert{}, scratch_space );
		other.changes.update( ChangeSet::Remove{}, scratch_space );
		scratch_space.clear();
	}
}
