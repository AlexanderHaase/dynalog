#include <ctime>
#include <array>
#include <cstring>
#include <dynalog/include/Timestamp.h>

namespace dynalog { namespace timestamp {

	/// Decompose the specfied timepoint
	///
	Atoms::Atoms( const std::chrono::system_clock::time_point & time )
	{
		const time_t timepoint = std::chrono::system_clock::to_time_t( time );
		struct tm parts;
		::gmtime_r( &timepoint, &parts );
		year = Year{ parts.tm_year + 1900 };
		month = Month{ parts.tm_mon + 1 };
		day = Day{ parts.tm_mday };
		hour = Hour{ parts.tm_hour };
		minute = Minute{ parts.tm_min };
		nanoseconds = (time - std::chrono::system_clock::from_time_t(timepoint));
		nanoseconds += std::chrono::seconds{ parts.tm_sec };
	}
		
	/// Accurately convert the components to a time_point.
	///
	Atoms::operator std::chrono::system_clock::time_point( void ) const
	{
		struct tm parts;
		memset( &parts, 0, sizeof(parts) );
		parts.tm_year = year.count() - 1900;
		parts.tm_mon = month.count() - 1;
		parts.tm_mday = day.count();
		parts.tm_hour = hour.count();
		parts.tm_min = minute.count();
		auto result = std::chrono::system_clock::from_time_t( ::mktime( &parts ) - ::timezone );
		result += nanoseconds;
		return result;
	}

	/// Get the fastest coarse system time available.
	///
	std::chrono::system_clock::time_point coarse()
	{
		struct timespec ts;
		::clock_gettime( CLOCK_REALTIME_COARSE, &ts );

		auto result = std::chrono::system_clock::from_time_t( ts.tv_sec );
		result += std::chrono::nanoseconds{ ts.tv_nsec };
		return result;
	}
} }
