#!/usr/bin/python3

import sys
import os.path
import json
import numpy
import matplotlib
#matplotlib.use( 'SVG' )
from matplotlib import pyplot

def count( iterable ):
  return sum( 1 for item in iterable )

def plot_samples( title, data ):
  mean = data[ 'mean(usec)' ]
  stdev = data[ 'stdev(usec)' ]
  bin_width = data[ 'stdev(usec)' ] * 4.0 * 5.0 / 95.0
  get_elapsed = lambda obj: obj[ 'elapsed' ]
  get_outlier = lambda obj: obj[ 'outlier' ]
  get_filtered = lambda obj: not obj[ 'outlier' ]
  samples = lambda: map( get_elapsed, data[ 'samples' ] )
  min_value = min( samples() )
  real_max = max( samples() )
  max_value = min( real_max, mean + stdev * 8.0 )
  #max_value = max( samples() )
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
  pyplot.title( title )
  pyplot.legend(loc='upper right')

  line_y = axis.get_ylim()[ 1 ] *.95
  pyplot.axvline( mean, linestyle = '-', linewidth = 2, color = 'black' )
  pyplot.text( mean + bin_width/2, line_y, 'mean: {:.3e}'.format( mean ), rotation = 90 )

  pyplot.axvline( mean + 3 * stdev, linestyle = '--', linewidth = 2, color = 'black' )
  pyplot.text( mean + 3 * stdev + bin_width/2, line_y, '+3 stdev: {:.3e}'.format( mean + 3 * stdev ), rotation = 90 )
  pyplot.axvline( mean - 3 * stdev, linestyle = '--', linewidth = 2, color = 'black' )
  pyplot.text( mean - 3 * stdev + bin_width/2, line_y, '-3 stdev: {:.3e}'.format( mean - 3 * stdev ), rotation = 90 )


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
    plot_samples( *item )
    action()


