#!/usr/bin/python3

import sys
import os.path
import json
import numpy
import matplotlib
import itertools
import collections
import math

#matplotlib.use( 'SVG' )
import sklearn.mixture
from matplotlib import pyplot

def consume(iterator, n = None):
  "Advance the iterator n-steps ahead. If n is none, consume entirely."
  # Use functions that consume iterators at C speed.
  if n is None:
    # feed the entire iterator into a zero-length deque
    collections.deque(iterator, maxlen=0)
  else:
    # advance to the empty slice starting at position n
    next(itertools.islice(iterator, n, n), None)
  return iterator

def count( iterable ):
  return sum( 1 for item in iterable )


def model_distribution( title, data ):
  get_elapsed = lambda obj: obj[ 'elapsed' ]
  samples = numpy.fromiter( map( get_elapsed, data[ 'samples' ] ), numpy.double )
  samples.sort()

  hist, bins = numpy.histogram( samples, 100 )

  figure, axis = pyplot.subplots()

  #xy = numpy.zeros( shape = (len(hist),2) )
  #for index in range(0,len(hist)):
  #  center =  numpy.mean( bins[ index : index + 1 ] )
  #  xy[ index ] = [ center, hist[ index ] ]

  xy = numpy.zeros( shape = (len(samples),2) )
  xy[:,0] = samples
  #  center =  numpy.mean( bins[ index : index + 1 ] )
  #  xy[ index ] = [ center, hist[ index ] ]
  
  #print( xy )

  gmm = sklearn.mixture.GMM( n_components=4 ).fit( xy )
  labels = gmm.predict( xy )
  x = samples
  y = numpy.arange( data['count'] )
  #pyplot.scatter( xy[:,0], xy[:,1], c=labels, s=40 )
  pyplot.scatter( x, y, c=labels, s=40, linewidth = 0 )


def plot_samples( title, data ):
  mean = data[ 'mean(usec)' ]
  stdev = data[ 'stdev(usec)' ]

  get_elapsed = lambda obj: obj[ 'elapsed' ]
  get_outlier = lambda obj: obj[ 'outlier' ]
  get_filtered = lambda obj: not obj[ 'outlier' ]
  samples = lambda: map( get_elapsed, data[ 'samples' ] )

  min_value = min( samples() )
  real_max = max( samples() )
  if stdev:
    max_value = min( real_max, mean + stdev * 8.0 )
    bin_width = stdev * 4.0 * 5.0 / 95.0
  else:
    max_value = real_max
    bin_width = (max_value - min_value)/100.0
  bin_count = int(( max_value - min_value ) / bin_width + 1)

  outliers = list( map( get_elapsed, filter( get_outlier, data[ 'samples' ] ) ) )
  filtered = list( map( get_elapsed, filter( get_filtered, data[ 'samples' ] ) ) )
  bins = numpy.linspace( min_value, max_value, bin_count )

  plot_opts = dict(
    bins = bins,
    alpha = 0.5,
    log = False,
  )

  figure, axis = pyplot.subplots()
  axis.hist( filtered, label = "filtered", **plot_opts )
  axis.hist( outliers, label = "outliers", **plot_opts )

  xlabel = 'usec/call'
  if real_max != max_value:
    percent = count( filter( lambda x: x > max_value, samples() ) ) / count( samples() )
    xlabel += ' ({:.1f}% truncated, max = {:.3e}usec)'.format( percent*100, real_max )
  pyplot.xlabel( xlabel )
  pyplot.ylabel( 'samples (x{})'.format( data[ 'iterations' ] ) )

  # plot +-3 stdev
  #
  stdev_opts = dict(
    color = 'black',
    linewidth = 2
  )

  line_y = axis.get_ylim()[ 1 ] *.95
  pyplot.axvline( mean, linestyle = '-', **stdev_opts )
  pyplot.text( mean + bin_width/2, line_y, 'mean: {:.3e}'.format( mean ), rotation = 90 )

  pyplot.axvline( mean + 3 * stdev, linestyle = '--', **stdev_opts )
  pyplot.text( mean + 3 * stdev + bin_width/2, line_y, '+3 stdev: {:.3e}'.format( mean + 3 * stdev ), rotation = 90 )
  pyplot.axvline( mean - 3 * stdev, linestyle = '--', **stdev_opts )
  pyplot.text( mean - 3 * stdev + bin_width/2, line_y, '-3 stdev: {:.3e}'.format( mean - 3 * stdev ), rotation = 90 )

  # plot percentiles
  #
  sorted_data = sorted( samples() )

  axis2 = axis.twinx()
  axis2.hist( sorted_data, label = "cumulative",cumulative=1, histtype='step', color='orange', **plot_opts )
  axis2.set_ylabel( "percentile" )
  axis2.set_ylim( ( 0, data['count'] ) )
  axis2.set_yticks( numpy.linspace( 0, data['count'], 11 ) )
  axis2.set_yticklabels( list( map( "{}%".format, range(0,110,10) ) ) )

  percentile_opts = dict(
    color = 'blue',
    linestyle = ':',
    linewidth = 1
  )

  percentiles = [ .5, .75, .9, .99 ]
  indicies = [ math.floor(data['count'] * percentile) for percentile in percentiles ]

  prior_xaxis = axis2.get_xlim()
  limit = axis2.get_xlim()[ 1 ];
  for index in indicies:
    value = sorted_data[ index ]
    if value < limit:
      x = numpy.linspace( value, limit, 10 )
      y = numpy.repeat( index, 10 )
      axis2.plot( x, y, **percentile_opts )
      x = numpy.repeat( value, 10 )
      y = numpy.linspace( 0, index, 10 )
      axis2.plot( x, y, **percentile_opts )
  axis2.set_xlim( prior_xaxis )

  # legend and title
  legends, labels = axis.get_legend_handles_labels()
  legends2, labels2 = axis2.get_legend_handles_labels()
  legends.extend( legends2 )
  labels.extend( labels2 )  
  pyplot.title( title )
  pyplot.legend( legends, labels, loc='right')

def plot_comparison( records ):
  values = list( map( lambda item: item[ 1 ][ 'mean(usec)' ], records ) )
  error = list( map( lambda item: item[ 1 ][ 'stdev(usec)' ] * 2, records ) )
  labels = list( map( lambda item: item[ 0 ], records ) )

  y_pos = numpy.arange( len(labels) )
  y_ticks = [ '' for item in labels ]

  figure, axis = pyplot.subplots()
  axis.barh( y_pos, values, xerr = error, align='center', alpha = 0.5 )
  axis.set_yticks( y_pos )
  axis.set_yticklabels( y_ticks )
  axis.invert_yaxis()
  axis.set_xlabel( 'usec/call' )
  axis.set_title( 'Comparison' )
  axis.set_ylim( ( max( y_pos ) + 1, min( y_pos ) - 1  ) )

  offset = axis.get_xlim()[ 0 ] + axis.get_xlim()[ 1 ] * .015

  for index, label in enumerate( labels ):
    axis.text( offset, index - 0.07, label, alpha = 0.5 )

  max_value = max( values )

  for index, mean in enumerate( values ):
    axis.text( offset, index + .35, "{:.3e} usec (x{:.2f})".format( mean, max_value/mean ), alpha = 0.5 )
    

def action_show():
  pyplot.show()

class action_save( object ):
  def __init__( self, path, suffix = 'png' ):
    self.path = path
    self.suffix = suffix
    self.index = 0

  def __call__( self ):
    file_name = "{}.{}".format( self.index, self.suffix )
    file_path = os.path.join( self.path, file_name )
    pyplot.savefig( file_path )
    self.index += 1
    

if __name__ == '__main__':
  
  try:
    action = action_save( sys.argv[ 2 ] )
  except IndexError:
    action = action_show

  with open( sys.argv[ 1 ], 'r' ) as handle:
    records = json.load( handle )

  # change to sorted
  #
  records = list( records.items() )
  records.sort( key = lambda item: item[ 0 ] )

  plot_comparison( records )
  action()

  for item in records:
    model_distribution( *item )
    action()
    plot_samples( *item )
    action()


