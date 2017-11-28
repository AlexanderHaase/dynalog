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
  class Benchmark {
   public:
    using clock = std::chrono::steady_clock;
    using microseconds_double = std::chrono::duration<double, std::micro>;
    struct Sampler;

    Benchmark()
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( "<baseline>" ),
        std::forward_as_tuple( 10000, 10000 ) );

      auto & baseline = result.first->second;
      baseline.collect( [](){ clock::now(); }, [](){} );
      baseline.analyze_gaussian();
      budget = baseline.mean;
      uncertainty = clock::duration{ baseline.mean.count() / baseline.iterations };
    }

    template < typename Context, typename = decltype( std::declval<Context>()( declref<Sampler>() ) ) >
    void fixture( const std::string & name, Context && context )
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( name ),
        std::forward_as_tuple( budget, uncertainty ) );

      const auto iter = result.first;
      Sampler{ iter->second }.evaluate( context );
    }


    template < typename Callable, typename PostCondition >
    void measure( const std::string & name, Callable && callable, PostCondition && condition )
    {
      const auto result = targets.emplace( std::piecewise_construct,
        std::forward_as_tuple( name ),
        std::forward_as_tuple( budget, uncertainty ) );

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

    /// Sample set for a specific target.
    ///
    /// Samples are collected via a fixture helper.
    ///
    class Target {
     public:
      Target( const clock::duration & budget_arg, const clock::duration & uncertainty_arg )
      : budget( budget_arg )
      , uncertainty( uncertainty_arg )
      {}

      Target( size_t iterations_arg, size_t count_arg )
      : iterations( iterations_arg )
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
        stream << "{\"mean(usec)\": " << std::chrono::duration_cast<microseconds_double>( mean ).count();
        stream << ",\"stdev(usec)\": " << std::chrono::duration_cast<microseconds_double>( mean ).count();
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
          stream << "{\"elapsed\": " << std::chrono::duration_cast<microseconds_double>( sample.elapsed ).count();
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
        for( iterations = 1; (estimate = time( callable, condition )) < budget; iterations *= 2 );

        // => uncertainty = estimate / ( iterations * sqrt(count) )
        // => count = ((iterations * uncertainty)/estimate)^2
        //count = 10000;
        count = (10000 * iterations * uncertainty.count()) / estimate.count();
        //count = count * count;
        count = std::min( size_t{10000}, std::max( size_t{100}, count ) );
      }

      /// Collects the specified number of observations.
      ///
      template <typename Callable, typename PostCondition >
      clock::duration time( Callable && callable, PostCondition && condition ) const
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
          mean = clock::duration::zero();
          for( auto & sample : samples )
          {
            if( !sample.outlier )
            {
              mean += sample.elapsed;
              total += 1;
            }
          }
          mean = clock::duration{ mean.count() / total };

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
          stdev = clock::duration{ int64_t(std::sqrt( accum / total )) };

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
        clock::duration elapsed = clock::duration::zero();
        bool outlier = false;
      };
  
      clock::duration budget; ///< Budget for a single iteration.
      clock::duration uncertainty; ///< desired uncertainty.
      clock::duration estimate;
      clock::duration upper;
      clock::duration lower;
      size_t iterations;  ///< Number of iterations per observation.
      size_t count;       ///< Number of observations.
      size_t valid;
      clock::duration mean;
      clock::duration stdev;
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
    clock::duration budget;
    clock::duration uncertainty;
    std::map<std::string,Target> targets;
  };

/*
	  void log( std::ostream & stream, double baseline )
	  {
		  for( auto && result : results )
		  {
			  auto & tag = std::get<0>( result );
			  auto usec = std::get<1>( result );
			  auto relative = baseline / usec;
			  stream << std::setprecision( 5 ) << std::fixed << usec << " usec/call (";
			  stream << std::setprecision( 2 ) << std::fixed << relative;
			  stream << " x)\t" << tag << std::endl;
		  }
	  }
  };
*/
 }
