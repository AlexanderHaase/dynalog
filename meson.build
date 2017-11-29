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
	'-Wdouble-promotion',
	'-Wshadow',
	'-Wformat=2',
  #'-flto', # Doesn't play well with clang
	'-Ofast'
  ]

cpp_lint_flags = [
	'-Wlogical-op',
	'-Wdouble-promotion',
	'-Wnull-dereference',
	'-Wduplicated-cond',
	'-Wduplicated-branches',
	'-Wrestrict'
  ]

cpp_other_flags = [
	'-Wuseless-cast'
  ]

cpp_compiler = meson.get_compiler('cpp')

foreach flag : cpp_lint_flags
  if cpp_compiler.has_argument( flag, name : flag )
    cpp_flags += [ flag ]
  endif
endforeach

foreach flag : cpp_other_flags
  if cpp_compiler.compiles(
    'struct Base { Base( int const & arg ); int value; }; struct Derived : Base { using Base::Base; }; static Derived test{0}; Base::Base( int const & arg ) : value( arg ) {} int main() { return 0; }',
      args : cpp_flags + [ flag ],
      name : flag )
    cpp_flags += [ flag ]
  endif
endforeach


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

