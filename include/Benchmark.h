#pragma once

#include <dynalog/include/util.h>
#include <chrono>
#include <map>
#include <cmath> // for sqrt
#include <iostream>
#include <iomanip>

namespace dynalog {

  /// Micro-benchmarking suit.
  ///
  /// Collects multiple observations and filters results for outliers.
  ///
  template < typename Clock >
  class Benchmark {
   public:
    using clock = Clock;
    using duration = typename Clock::duration;
    using time_point = typename Clock::time_point;
    using microseconds_double = std::chrono::duration<double, std::micro>;
    class Sampler;
    struct Config
    {
      struct {
        size_t min;
        size_t max;
      } samples;
    };

    Benchmark( Config && config_arg = Config{ { 100, 10000 } } )
    : config( std::move( config_arg ) )
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( "<calibration: clock::now()>" ),
        std::forward_as_tuple( config, config.samples.max, config.samples.max ) );

      auto & baseline = result.first->second;
      baseline.collect( [](){ clock::now(); }, [](){} );
      baseline.analyze_gaussian();
      budget = baseline.mean;
      uncertainty = duration{ baseline.mean.count() / baseline.iterations };
    }

    template < typename Context, typename = decltype( std::declval<Context>()( declref<Sampler>() ) ) >
    void fixture( const std::string & name, Context && context )
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( name ),
        std::forward_as_tuple( config, budget, uncertainty ) );

      const auto iter = result.first;
      Sampler{ iter->second }.evaluate( context );
    }


    template < typename Callable, typename PostCondition >
    void measure( const std::string & name, Callable && callable, PostCondition && condition )
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( name ),
        std::forward_as_tuple( config, budget, uncertainty ) );

      const auto iter = result.first;
      Sampler{ iter->second }.measure( std::forward<Callable>( callable ), std::forward<PostCondition>( condition ) );
    }

    template < typename Callable >
    void measure( const std::string & name, Callable && callable )
    {
      measure( name, std::forward<Callable>( callable ), [](){} );
    }

    void summary( std::ostream & stream ) const
    {
      for( const auto & pair : targets )
      {
        pair.second.summary( stream );
        stream << "\t" << pair.first << std::endl;
      }
    }

    void json( std::ostream & stream ) const
    {
      bool delim = false;
      stream << "{";
      for( const auto & pair : targets )
      {
        if( delim )
        {
          stream << ", ";
        }
        delim = true;
        stream << "\"" << pair.first << "\": ";
        pair.second.json( stream );
      }
      stream << "}";
    }

    /// Sample set for a specific target.
    ///
    /// Samples are collected via a fixture helper.
    ///
    class Target {
     public:
      Target( const Config & config_arg, const duration & budget_arg, const duration & uncertainty_arg )
      : config( config_arg )
      , budget( budget_arg )
      , uncertainty( uncertainty_arg )
      {}

      Target( const Config & config_arg, size_t iterations_arg, size_t count_arg )
      : config( config_arg )
      , iterations( iterations_arg )
      , count( count_arg )
      {}

      void summary( std::ostream & stream ) const
      {
        const auto mean_usec = std::chrono::duration_cast<microseconds_double>( mean );
        const auto stdev_usec = std::chrono::duration_cast<microseconds_double>( stdev );
        stream << std::setprecision( 5 ) << std::fixed << mean_usec.count()/double(iterations) << " usec/call (stdev: " << stdev_usec.count()/double(iterations) << "), samples: (" << valid << "/" << count << "), " << iterations << " iterations/sample";
      }

      void json( std::ostream & stream ) const
      {
        const auto scale = 1.0/double( iterations );
        stream << "{\"mean(usec)\": " << std::chrono::duration_cast<microseconds_double>( mean ).count() * scale;
        stream << ",\"stdev(usec)\": " << std::chrono::duration_cast<microseconds_double>( stdev ).count() * scale;
        stream << ",\"estimate(usec)\": " << std::chrono::duration_cast<microseconds_double>( estimate ).count();
        stream << ",\"budget(usec)\": " << std::chrono::duration_cast<microseconds_double>( budget ).count();
        stream << ",\"iterations\": " << iterations;
        stream << ",\"count\": " << count;
        stream << ",\"valid\": " << valid;
        stream << ",\"samples\": [";
        bool delim = false;
        for( const auto & sample : samples )
        {
          if( delim )
          {
            stream << ", ";
          }
          delim = true;
          stream << "{\"elapsed\": " << std::chrono::duration_cast<microseconds_double>( sample.elapsed ).count() * scale;
          stream << ",\"outlier\": " <<(sample.outlier ? "true" : "false") << "}";
        }
        stream << "]}";
      }

      /// Determine number of iterations per cycle and number of observations to take.
      ///
      /// Also warms up the cache.
      ///
      /// TODO: Better algorithm for choosing number of samples.
      ///
      template <typename Callable, typename PostCondition >
      void calibrate( Callable && callable, PostCondition && condition )
      {
        // First approximation: iterations within budget, have a number of retries.
        //
        const auto retries = std::max( size_t{1}, config.samples.min/10 );
        auto retry = retries;
        iterations = 1;
        for(;;)
        {
           estimate = time( callable, condition );
            if( estimate < budget )
            {
              retry = retries;
              iterations *= 2;
            }
            else if( --retry == 0 )
            {
              break;
            }
        }

        // => uncertainty = estimate / ( iterations * sqrt(count) )
        // => count = ((iterations * uncertainty)/estimate)^2

        count = (config.samples.max * iterations * uncertainty.count()) / estimate.count();
        //count = count * count;
        count = std::min( config.samples.max, std::max( config.samples.min, count ) );
      }

      /// Collects the specified number of observations.
      ///
      template <typename Callable, typename PostCondition >
      duration time( Callable && callable, PostCondition && condition ) const
      {
        const auto begin = clock::now();

        for( size_t index = 0; index < iterations; ++index )
        {
          callable();
        }
        condition();

        const auto end = clock::now();
        return end - begin;
      }

      /// Collects the specified number of observations.
      ///
      template <typename Callable, typename PostCondition >
      void collect( Callable && callable, PostCondition && condition )
      {
        // zero/cache sample vector.
        //
        samples.resize( count );
        for( auto & sample : samples )
        {
          sample.elapsed = time( callable, condition );
        }
      }

      /// Do z-score style reduction of outliers in data set.
      ///
      void analyze_gaussian()
      {
        auto prior = samples.size();
        for(;;)
        {
          // compute mean
          //
          size_t total = 0;
          mean = duration::zero();
          for( auto & sample : samples )
          {
            if( !sample.outlier )
            {
              mean += sample.elapsed;
              total += 1;
            }
          }
          mean = duration{ mean.count() / total };

          // compute stdev
          //
          int64_t accum = 0;
          for( auto & sample : samples )
          {
            if( !sample.outlier )
            {
              const auto ticks = (sample.elapsed - mean).count();
              accum += ticks * ticks;
            }
          }
          stdev = duration{ int64_t(std::sqrt( accum / total )) };

          // Compute range
          //
          upper = mean + 2 * stdev;
          lower = mean - 2 * stdev;

          // Count the number in range
          //
          valid = 0;
          for( auto & sample : samples )
          {
            const bool outlier = sample.elapsed > upper || sample.elapsed < lower;
            sample.outlier = outlier;
            valid += !outlier;
          }

          //std::cout << "Range: [" << lower.count() << "," << upper.count() << "] Mean: " << mean.count() << " Samples: (" << valid << ", " << samples.size() << ")" << std::endl; 
          if( valid * 95 / 100 >= total || valid == prior )
          {
            //summary( std::cout );
            break;
          }
          prior = valid;
        }
      }

      struct Sample
      {
        duration elapsed = duration::zero();
        bool outlier = false;
      };

      const Config & config;
      duration budget; ///< Budget for a single iteration.
      duration uncertainty; ///< desired uncertainty.
      duration estimate;
      duration upper;
      duration lower;
      size_t iterations;  ///< Number of iterations per observation.
      size_t count;       ///< Number of observations.
      size_t valid;
      duration mean;
      duration stdev;
      std::vector<Sample> samples;  ///< Set of all observations.
    };

    class Sampler {
     public:
      Sampler( Target & target_arg )
      : target( target_arg )
      {}


      /// Collect samples within the context of a setup/teardown function.
      ///
      /// @tparam Context type of setup/teardown callable.
      /// @param context A function/functor/lambda accepting a Fixture.
      ///
      template < typename Context >
      void evaluate( Context && context )
      {
        context( *this );
      }

      /// Measure a callable function subject to a postcondition
      ///
      template <typename Callable, typename PostCondition >
      void measure( Callable && callable, PostCondition && condition )
      {
        target.calibrate( callable, condition );
        target.collect( callable, condition );
        target.analyze_gaussian();
      }

      /// Measure a callable function
      ///
      template <typename Callable >
      void measure( Callable && callable )
      {
        const auto condition = [](){};
        measure( callable, condition );
      }

     protected:
      Target & target;
    };

   protected:
    const Config config;
    duration budget;
    duration uncertainty;
    std::map<std::string,Target> targets;
  };
 }
