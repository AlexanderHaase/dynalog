# DynaLog #

Never delete a log statement. Reconfigure logging on the fly. DynaLog makes it fast and easy:

**Add statements:**

```c++
	DYNALOG( dynalog::Level::INFO, "Welcome to DynaLog!" );
```

**Get results:**

```sh
dynalog/build$ ./comparison
Performance comparison of formatting output(relative to slowest):
0.57376 usec/call (1.52 x)	dprintf('/dev/null')
0.17362 usec/call (5.01 x)	snprintf(<internal buffer>)
0.25876 usec/call (3.36 x)	snprintf(<internal buffer>) => write('/dev/null')
0.18724 usec/call (4.65 x)	fstream('/dev/null')
0.55611 usec/call (1.57 x)	stringstream(<internal buffer>)
0.87041 usec/call (1.00 x)	stringstream(<internal buffer>) => write('/dev/null')
0.23405 usec/call (3.72 x)	DynaLog('/dev/null')
0.03550 usec/call (24.52 x)	DynaLog(<NoOp>)
0.00086 usec/call (1016.83 x)	DynaLog(<disabled>)
0.17073 usec/call (5.09 x)	DynaLog(<async>'/dev/null')
```

---

## About ##

DynaLog is designed around instrumenting code: specifically, that builds should be programmatically instrumentable to allow targeted introspection. That way when bugs/issues/questions/requirements/etc. arise, answers are simply a configuration change away. Logging should be configurable at a fine-grain level so that logging can be targeted at the topic at hand without drowning in cruft. At the same time, logging should be cheap enough that there's little performance benefit to deleting statements. Logging should also be flexible enough to support logging for multiple purposes simultaneously: Debugging, tracing, auditing, etc. DynaLog aims to deliver on all those promises.

### Performance First ###

Realistically, in intended operation, output from the vast majority of log statements is unwanted. The program(and it's end users) shouldn't pay the penalty for that. DynaLog emphasizes performance in the disabled case: DynaLog leverages c++11 to inline checks and bypass all formatting processing when statements are disabled. Checks are localized to the statement and kept simple to minimize overhead. 

DynaLog takes performance seriously. Several design candidates were compared and rigorously tested to arrive at a honed implementation. Great care was taken to ensure statements could be initialized without context gaurds. Candidates were tested, disassembled, analyzed, refactored and tested again to squeeze out unnecessary overhead. The ultimate design costs on the order of a `load`, `test`, and `jump` instruction sequence(about the same as `if( condition )`) when disabled while remaining on par with other logging when enabled.

DynaLog is also thread-safe and built with async logging in mind: Statements are bundled into atomic messages and shipped to emitters, where they are free to be processed asynchronously. Emitters are entirely modular, allowing multiple strategies optimized for distinct use-cases to seamlessly co-exist.

### Dynamically Reconfigurable ###

DynaLog allows loggers, emitters, and policies to be reconfigured on the fly: it's easy to dynamically create a logger per task, configure emitters per functionality group, route some loggers to files and others to the network, not to mention log asynchronously. Global state is only provided for convenience; the library is entirely modular and extensible allowing applications to build out to suit their individual needs. Programmatic changes take effect immediately: policy changes find, match, and configure the relevant loggers as soon their installed.

### Simplicity ###

DynaLog works out of the box. Start adding statements and they'll hit stdout immediately. Then keep working on the application--statements are configurable down the road. Redirect them to a file by changing the default emitter. Install policies to moderate output as the application grows. Introduce dynamic loggers and emitters to better suit your application's workflow. The sky is the limit, but it's built on simplicity.

---

## Structure ##

Hype aside, DynaLog is built around two concepts: `Logger`s and `Emitter`s that move `Messages`, and `Configuration` and `Policy` objects that manage them. 

### Loggers and Emitters (and statements) ###

At it's core, DynaLog configures `Logger`s with `Emitter`s. `Logger`s are the fundamental points where configuration is evaluated: if an `Emitter` is set and level enabled, the `Logger` will prepare and send a `Message` to the `Emitter`. `Logger` logic is eactly that simple to meet performance goals. `Emitter`s are simply an interface for receiving `Message`s from one or more `Logger`s. `Emitter` implementations are free to do as they please with `Messages`.

DynaLog statements(`DYNALOG(...)`) simply generate statically scoped `Logger`s that are tied to the global configuration on their first use. It's fairly straight forward to wrap the `DYNALOG(...)` macro add a custom format, or make a modified version to customize logger initialization.

#### Messages ####

`Message`s encapsulate logging arguments to serialize later. With a synchronous `Emitter`, it's slightly more expensive compared dumping the arguments to a stack buffer and passing the result to a function pointer. However, with an async `Emitter`, it moves the majority of the cost of logging to the `Emitter`. Anywhere from half to 90% of the cost can be offloaded depending upon the contents and the async `Emitter` implementation. Either way, `Message`s simplify the `Emitter` interface while maintaining full c++ expressivity:

#### Overhead ####

The `Logger`-`Emitter`-`Message` pattern only requires two virtual calls to process a `Messages`: One when a `Logger` hands over a `Message` to an `Emitter`, and another when the `Emitter` streams the `Message` contents(encapsulation is expressed as a closure that implements an abstract base class). Creating a `Message` requires sourcing an appropriately sized buffer--usually from a thread-local cache. Combined, this overhead is tiny compared to the cost of formatting and emitting a message--benchmarks suggest less than 1/100th of a statement's cost. 

The remainder of the overhead is driven by external factors: `Message` format complexity(copy and stream operators) and the `Emitter` implementation.

### Configuration and Policy ###

The `Logger`-`Emitter` pattern stands by itself: a `Logger` internally carries all the state required to make a logging decisions. However, `Logger`s quickly pile up when used properly, and tracking and reconfiguring myriad loggers warrants an abstraction. DynaLog's native opinion on `Logger` configuration management is the unimaginatively named `Configuration` class and `Policy` interface. It's a bit bumbling, but it functional enough for now.

In concept, a `Policy` matches a set of `Logger`s and dictates their configuration. A `Configuration` is a set of `Policy` instances with various priorities. Should a `Policy` conflict, the highest priority `Policy` takes precedence. A `Configuration` also tracks which `Logger`s are associated with which `Policy`, and moderates a set managed `Logger` instances.

#### Policy Mechanics ####

The design presumes that each `Policy` is mostly static--if it matches a `Logger`, it will continue to match that `Logger`. Changes to a `Policy`'s matching criteria require that it's `Configuration` *rescan* both `Logger`s it manages and those managed by a lower priority `Policy`. Changes to the `Emitter` configuration a `Policy` provides likely also requires that it's `Configuration` *update* it on the `Logger`s it manages. In either case, changes in `Policy` must be push-propagated to the `Logger`'s under it's control.

`Policy` implementations accepting external modification must be thread-safe: `Logger`s may be inserted or removed from a `Configuration` at any time. Likewise internally tracking managed `Logger`s can prove problematic since `Configuration` has it's own internal state--likely better to have `Configuration` *update* the `Policy` on the `Logger`s it manages to ensure consistency of the effective configuration.

#### Configuration Objects ####

`Configuration` objects arbitrate `Policy` and apply it to managed `Logger`s. `Logger`s must be assciated with a `Configuration` to be managed by it(statements are associated with the `global::configuration` the first time they are used). Additionally, `Logger`s must be matched by a `Policy` to be retained by a `Configuration`--otherwise they are silently dropped. The `DefaultPolicy` class functions as a catch-all for `Logger`s.

---

## Future Directions ##

The most pressing goal is to incorporate more functionality out of the box:

  - More Emitter implementations.
  - More Policy implementations.
  - More formatting helpers.
  - More examples.
  - More tests.

More benchmarks and performance analysis is also in order.

