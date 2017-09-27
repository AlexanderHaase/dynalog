library_sources = [ 'Message.cpp', 'Log.cpp', 'Configuration.cpp', 'HandleEmitter.cpp', 'Cache.cpp', 'async/Emitter.cpp' ]
libdynalog = shared_library( 'dynalog', library_sources, 
	include_directories : [ base_includes ],
	dependencies: dependency( 'threads' ),
	cpp_args : cpp_flags,
	install : true )
