#include <memory>
#include <cstdlib>
#include <sys/time.h>
#include <iostream>
#include <atomic>
#include <type_traits>
#include <cstring>
#include <assert.h>
#include <sys/mman.h>
#include <cstdarg>
#include <bitset>

#define STR_IMPL( blah ) #blah
#define STR( blah ) STR_IMPL( blah )
#define LOCATION __FILE__ ":" STR( __LINE__ )
#define CONTEXT __PRETTY_FUNCTION__

/// Test cost of always virtual calling.
class VirtualCallBase {
public:
	virtual ~VirtualCallBase( void ) {}
	virtual int call( VirtualCallBase *& ) = 0;
};

class IncrementCall : public VirtualCallBase {
public:
	virtual ~IncrementCall( void ) {}
	virtual int call( VirtualCallBase *& ) { return ++value; }
protected:
	int value = 0;
};


class DecrementCall : public VirtualCallBase {
public:
	virtual ~DecrementCall( void ) {}
	virtual int call( VirtualCallBase *& ) { return --value; }
protected:
	int value = 0;
};


/// Randomly return an implementation to avoid devirtualization.
///
std::unique_ptr<VirtualCallBase> makeVirtualCall( void )
{
	return std::unique_ptr<VirtualCallBase>((rand() & 1) ? static_cast<VirtualCallBase*>( new IncrementCall{} ) : static_cast<VirtualCallBase*>( new DecrementCall{} ));
}

IncrementCall incrementCall;

/// Helpers for tagged pointers
///
template < typename Type, typename Tag, uintptr_t Mask = alignof( Type ) - 1, typename = typename std::enable_if< std::is_integral<Tag>::value >::type >
constexpr Type * setTag( Type * type, Tag tag )
{
	return reinterpret_cast<Type*>( (reinterpret_cast<uintptr_t>( type ) & ~Mask) | static_cast<uintptr_t>( tag ) );
}

template < typename Type, uintptr_t Mask = alignof( Type ) - 1 >
constexpr uintptr_t getTag( Type * type )
{
	return reinterpret_cast<uintptr_t>( type ) & Mask;
}

template < typename Type, uintptr_t Mask = alignof( Type ) - 1 >
constexpr Type * clearTag( Type * type )
{
	return reinterpret_cast<Type*>( reinterpret_cast<uintptr_t>( type ) & ~Mask );
}

int taggedPointerCall( VirtualCallBase * next )
{
	static VirtualCallBase * taggedPtr = &incrementCall; //setTag( &incrementCall, 0 );
	const auto ptr = clearTag( taggedPtr );
	const auto tag = getTag( taggedPtr );
	taggedPtr = next;

	int ret;

	switch( tag )
	{
	case ( 0 ):
		ret = 0;
		break;
	case ( 1 ):
		ret = ptr->call( taggedPtr );
		break;
	}
	return ret;
}

/// Test tagged union performance
///
struct CallSite;

class VirtualInitBase {
public:
	virtual ~VirtualInitBase() {}
	virtual void init( CallSite & ) = 0;
};


struct CallSite {
	union {
		VirtualInitBase * init;
		VirtualCallBase * call;
	};
	void * data;
	bool enabled;
};


class StaticInit : public VirtualInitBase {
public:
	virtual ~StaticInit() {}
	virtual void init( CallSite & site ) { site.call = setTag( value.get(), tag ); site.data = this; site.enabled = tag - 1; }

	uintptr_t tag = 1;
	std::unique_ptr<VirtualCallBase> value;
};

StaticInit staticInit;

int taggedUnionCall( void )
{
	static CallSite callSite = { &staticInit, nullptr, false };
	const auto tag = getTag( callSite.call );

	int ret;

	switch( tag )
	{
	case ( 0 ):
		clearTag( callSite.init )->init( callSite );
		ret = 0;
		break;
	case ( 1 ):
		ret = -1;
		break;
	case ( 2 ):
		ret = clearTag( callSite.call )->call( callSite.call );
		break;
	}
	return ret;
}

/// Test condition+tagged union
///

int conditionalTaggedUnionCall( void )
{
	static CallSite callSite = { &staticInit, nullptr, true };
	if( callSite.enabled )
	{
		const auto tag = getTag( callSite.call );

		int ret;

		switch( tag )
		{
		case ( 0 ):
			clearTag( callSite.init )->init( callSite );
			ret = 0;
			break;
		case ( 1 ):
			ret = -1;
			break;
		case ( 2 ):
			ret = clearTag( callSite.call )->call( callSite.call );
			break;
		}
		return ret;
	}
	else
	{
		return 0;
	}
}

template < size_t Iterations = 100000000, typename Callable >
double usecPerCall( Callable && callable )
{
	struct timeval begin, end;
	gettimeofday( &begin, nullptr );

	for( size_t index = 0; index < Iterations; ++index )
	{
		callable();
	}

	gettimeofday( &end, nullptr );

	const auto elapsed = ( end.tv_sec - begin.tv_sec ) * 1000000 + end.tv_usec - begin.tv_usec;

	return double(elapsed)/double(Iterations);
}

template < typename Label, typename Generator >
void benchmark( Label && label, Generator && generator )
{
	std::cout << usecPerCall( generator() ) << " usec/call <== " << label << std::endl;
}


/// Dynamic rewrite callsite
///
class DynamicRewrite : public VirtualCallBase {
public:
	static const std::array<uint8_t,45> nopCode;

	static const std::array<uint8_t,10> nopOffset;

	static bool writeNop( uint8_t * const where, const size_t size )
	{
		if( size > nopOffset.size() )
		{
			return false;
		}
		else
		{
			// disable write protection
			const size_t pageMask =~((1<<12)-1);
			const size_t pageSize = 1<<12;
			assert( 0 == mprotect( reinterpret_cast<void*>( reinterpret_cast<size_t>(where) & pageMask ), pageSize, PROT_READ | PROT_WRITE | PROT_EXEC ) );
			// TODO: Atomically copy data...
			memcpy( where, &nopCode[ nopOffset[ size ] ], size );
		}
	}

	virtual ~DynamicRewrite() {}
	virtual int call( VirtualCallBase *& target )
	{
		target = value.get();

		// get callsite info
		//
		auto mangled =  __builtin_return_address( 0 );
		auto caller = __builtin_extract_return_addr( mangled );

		auto bytes = reinterpret_cast<uint8_t*>( caller );
		const int offset = 8;

		// dump it to think about rewriting
		std::cout << "Call site info from " << (void*)(bytes - offset) << " to " << (void*)(bytes + offset) << "(" << caller << "):\n\t"; 
		std::cout << std::hex;
		for( int index = -offset; index <= offset; ++index )
		{
			std::cout << int( bytes[ index ] ) << " ";
		}
		std::cout << std::dec << std::endl;

		//scan backwards until we find 'ff'
		int begin;
		for( begin = -1; begin > -offset; --begin )
		{
			if( bytes[ begin ] == 0xff )
			{
				break;
			}
		}

		if( begin == -offset )
		{
			std::cout << "Failed to find callq!" << std::endl;
			return 0;
		}

		if( ! enabled )
		{
			std::cout << "Changing range [" << (void*)(bytes + begin) << "," << caller << ") to nop: ";
			std::cout << std::hex;
			for( int index = begin; index < 0; ++index )
			{
				std::cout << int( bytes[ index ] ) << " ";
			}
			std::cout << std::dec << std::endl;

			writeNop( bytes + begin, -begin );

			std::cout << "Call site info after " << (void*)(bytes - offset) << " to " << (void*)(bytes + offset) << "(" << caller << "):\n\t"; 
			std::cout << std::hex;
			for( int index = -offset; index <= offset; ++index )
			{
				std::cout << int( bytes[ index ] ) << " ";
			}
			std::cout << std::dec << std::endl;
		}
		return 1;
	}

	bool enabled;
	std::unique_ptr<VirtualCallBase> value;
};

// http://www.felixcloutier.com/x86/NOP.html
const std::array<uint8_t,45> DynamicRewrite::nopCode =
{
	0x90, // 1 byte
	0x66, 0x90, // 2 bytes
	0x0F, 0x1F, 0x00, // 3 bytes
	0x0F, 0x1F, 0x40, 0x00, // 4 bytes
	0x0F, 0x1F, 0x44, 0x00, 0x00, // 5 bytes
	0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00, // 6 bytes
	0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, // 7 bytes
	0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, // 8 bytes
	0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, // 9 bytes
};

const std::array<uint8_t,10> DynamicRewrite::nopOffset = { 0, 0, 1, 3, 6, 10, 15, 21, 28, 36 };

DynamicRewrite dynamicRewrite;

int dynmaicRewriteCall( void )
{
	static VirtualCallBase * callable = &dynamicRewrite;
	static std::atomic<VirtualCallBase*> atom = { nullptr };
	atom.store( callable, std::memory_order_relaxed );
	return callable->call( callable );
}

namespace Intended {

	struct CallSite;

	struct Message {};

	std::unique_ptr<Message> getMessage() { return std::unique_ptr<Message>{ nullptr }; }

	class Action {
	public:
		virtual ~Action() {}
		virtual void apply( CallSite & site, std::unique_ptr<Message> && ) = 0;
		
	};

	struct CallSite {
		std::atomic<Action*> action;
		const char * location;
		const char * context;
		void * data;	// TODO--other config info...
		std::bitset<8> mask;

		void call( std::unique_ptr<Message> && message )
		{
			auto callable = action.load( std::memory_order_relaxed );
			if( callable )
			{
				callable->apply( *this, std::move( message ) );
			}
		}

		template < typename MessageBuilder >
		void call( MessageBuilder && builder )
		{
			auto callable = action.load( std::memory_order_relaxed );
			if( callable )
			{
				auto message = getMessage();
				builder( message );
				callable->apply( *this, std::move( message ) );
			}
		}
	};

	class InitAction : public Action {
	public:
		virtual ~InitAction() {}
		virtual void apply( CallSite & site, std::unique_ptr<Message> && message )
		{
			site.action.store( next.get(), std::memory_order_relaxed );
			site.mask.set( 0, message == nullptr );
			site.call( std::move( message ) );
		}

		std::unique_ptr<Action> next = nullptr;
	};

	template <typename Thing>
	class WrapperAction : public Action, private Thing {
	public:
		template < typename U >
		WrapperAction( U && u )
		: Thing( std::forward<U>( u ) )
		{}

		virtual ~WrapperAction() {}
		virtual void apply( CallSite & site, std::unique_ptr<Message> && )
		{
			if( site.mask.test( 1 ) )
			{
				Thing::operator() ( site );
			}
		}
	};

	template <typename Thing>
	auto makeAction( Thing && thing ) -> std::unique_ptr< Action >
	{
		return std::unique_ptr< Action >{ new WrapperAction<Thing>{ std::forward<Thing>( thing ) } };
	}

	auto getAction( bool choice ) -> std::unique_ptr< Action >
	{
		uintptr_t value = 0;
		return choice ? 
			makeAction( [ value ] ( CallSite & site ) mutable { value += reinterpret_cast<uintptr_t>( &site ); } ) :
			makeAction( [ value ] ( CallSite & site ) mutable { value -= reinterpret_cast<uintptr_t>( &site ); } );
	}

	InitAction init;

	void testIntended( void )
	{
		static CallSite site = { { &init }, LOCATION, CONTEXT, &site, 0 };
		site.call([]( std::unique_ptr<Message> & ){} );
	}
		
}

int main( int argc, const char** argv )
{
	benchmark( "Unconditional virtual call", []{ 
		return [ callable = makeVirtualCall() ]{ auto value = callable.get(); callable->call( value ); };
	});

	benchmark( "Conditional virtual call", [&]{
		auto condition = argc % 2;
		return [condition, callable = makeVirtualCall() ]() mutable { auto value = callable.get(); return condition ? callable->call(value) : 0; };
	});

	benchmark( "Null-check virtual call", [&]{
		return [callable = (argc%2) ? makeVirtualCall() : std::unique_ptr<VirtualCallBase>( nullptr ) ]() mutable { auto value = callable.get(); return value ? value->call(value) : 0; };
	});

	benchmark( "Atomic conditional virtual call",[&]{
		return [condition = std::unique_ptr<std::atomic<bool>>( new std::atomic<bool>( argc % 2 ) ), callable = makeVirtualCall() ]()mutable { auto value = callable.get(); return condition->load( std::memory_order_relaxed ) ? callable->call( value ) : 0; };
	});

	benchmark( "Switch/tagged pointer conditional virtual call", [&]{
		const auto tag = argc % 2;
		return [tag, callable = makeVirtualCall() ]{ return taggedPointerCall( setTag( callable.get(), tag ) ); };
	});

	benchmark( "Switch/tagged union conditional virtual call", [&]{
		staticInit.tag = (argc % 2) + 1;
		staticInit.value = makeVirtualCall();
		return []{ return taggedUnionCall(); };
	});

	benchmark( "Conditional switch/tagged union virtual call", [&]{
		staticInit.tag = (argc % 2) + 1;
		staticInit.value = makeVirtualCall();
		return []{ return conditionalTaggedUnionCall(); };
	});

	benchmark( "Dynamic Rewrite virtual call", [&]{
		dynamicRewrite.enabled = (argc % 2);
		dynamicRewrite.value = makeVirtualCall();
		return []{ return dynmaicRewriteCall(); };
	});

	benchmark( "Intended implementation", [&] {
		bool enabled = argc %2;
		Intended::init.next = enabled ? Intended::getAction( rand() % 2 ) : nullptr;
		return []{ return Intended::testIntended(); };
	});

	return 0;
}


