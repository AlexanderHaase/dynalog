test_includes = include_directories( '../Catch/single_include/' )
test_sources = files( 'main.cpp', 
	'Message.cpp',
	'Timestamp.cpp',
	'RingBuffer.cpp',
  'Log.cpp' )

test_exe = executable( 'all_tests', test_sources,
	include_directories : [ base_includes, test_includes ],
	cpp_args : cpp_flags,
	link_with : libdynalog )

test( 'combined tests', test_exe )
