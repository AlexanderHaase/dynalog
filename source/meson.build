library_sources = files(
	'Message.cpp',
	'Log.cpp',
	'Configuration.cpp',
	'HandleEmitter.cpp',
#	'Cache.cpp',
	'Timestamp.cpp',
	'async/Emitter.cpp',
	'async/Flush.cpp' )

libdynalog = shared_library( 'dynalog', library_sources, 
	include_directories : [ base_includes ],
	dependencies: dependency( 'threads' ),
	cpp_args : cpp_flags,
	install : true )
