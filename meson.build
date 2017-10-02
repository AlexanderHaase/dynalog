project( 'c++ DynaLog', 'cpp', 
	version : '0.2.1', 
	license : 'BSD 3-clause' )

cpp_flags = [ '-std=c++11', '-g', '-Og' ]

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
