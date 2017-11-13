project( 'c++ DynaLog', 'cpp', 
	version : '0.2.4', 
	license : 'BSD 3-clause' )

# TODO: add compiler version tests
#
cpp_flags = [ '-std=c++11',
	'-g',
	'-Wall',
	'-Wextra',
	'-Werror',
	'--pedantic',
	'-Wlogical-op',
	'-Wuseless-cast',
	'-Wdouble-promotion',
	'-Wshadow',
	'-Wformat=2',
	#gcc 6 '-Wnull-dereference',
	#gcc 6 '-Wduplicated-cond',
	#gcc 7 '-Wduplicated-branches',
	#gcc 7 '-Wrestrict',
	'-O3' ]

base_includes = include_directories( '../' ) 

#install_subdir( 'include', 'dynalog' )
subdir( 'source' )
subdir( 'test' )



# Example program
#
example_exe = executable( 'comparison', 'comparison.cpp',
	include_directories : base_includes,
	cpp_args : cpp_flags,
	link_with : libdynalog )
