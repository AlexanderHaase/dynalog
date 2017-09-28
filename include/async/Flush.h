#pragma once

#include <ostream>
#include <mutex>
#include <condition_variable>
#include <dynalog/include/Logger.h>

namespace dynalog { namespace async {

	class Flush {
	protected: 
		class FlushImpl;
	public:
		class Token {
		public:
			Token( const std::shared_ptr<FlushImpl> & );
			Token( const Token & );
			Token( Token && ) = default;
			~Token();
		protected:
			std::shared_ptr<FlushImpl> flush;
			friend std::ostream & operator << ( std::ostream & stream, const Token & flush );
		};

		Token token() const;

		inline operator Token() const { return token(); }
		
		bool wait(const std::chrono::steady_clock::duration & timeout = std::chrono::steady_clock::duration::max());

		Flush();
		Flush( const Flush & ) = default;
		Flush( Flush && ) = default;
	protected:
		std::shared_ptr<FlushImpl> impl;
	};

	std::ostream & operator << ( std::ostream & stream, const Flush::Token & flush );
} }
