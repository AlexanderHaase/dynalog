
#include <dynalog/include/async/Flush.h>

namespace dynalog { namespace async {

	
	class Flush::FlushImpl {
	public:
		bool wait(const std::chrono::steady_clock::duration & timeout )
		{
			std::unique_lock<std::mutex> lock( mutex );
			return pending.load( std::memory_order_relaxed ) == 0 || std::cv_status::no_timeout == condition.wait_for( lock, timeout );
		}

		void notify( void )
		{
			if( pending.fetch_sub( 1, std::memory_order_relaxed ) == 1 )
			{
				condition.notify_all();
			}
		}

		void pend( void ) { pending.fetch_add( 1, std::memory_order_relaxed ); }
	protected:
		std::atomic<int> pending = { 0 };
		std::mutex mutex;
		std::condition_variable condition;
	};

	Flush::Token::Token( const std::shared_ptr<FlushImpl> & impl )
	: flush( impl )
	{
		flush->pend();
	}

	Flush::Token::Token( const Token & other )
	: flush( other.flush )
	{
		flush->pend();
	}

	Flush::Token::~Token()
	{
		if( flush )
		{
			flush->notify();
		}
	}

	Flush::Token Flush::token() const
	{
		return Token{ impl };
	}

	bool Flush::wait(const std::chrono::steady_clock::duration & timeout )
	{
		return impl->wait( timeout );
	}

	Flush::Flush()
	: impl( std::make_shared<FlushImpl>() )
	{}

	std::ostream & operator << ( std::ostream & stream, const Flush::Token & )
	{
		return stream << std::flush;
	}
} }
