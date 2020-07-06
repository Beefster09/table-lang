Tentative name: Rhombus

This programming language is built around tables and arrays

Maintaining state in tables makes memory a lot nicer to the machine and keeps things sequential and
homogeneous. It is a fundamentally different way of thinking about your program's state and should
be very appropriate for games and scientific computing, but could have other applications.

The concurrency model is also very different. Rather than thinking in terms of threads, this
language provides metathreads for running in parallel (typically used for loops).
Metathreads generally should each have isolated state, but can share data through synchronization
that is built into the language syntax.



# Parallelism

Paralellism is the meat and potatoes of this language.

Metathreads are statically determined ahead of time. Global mutable variables must either be
assigned to a specific metathread, be thread-local, or be marked as `synchronized`. Worker threads
in a metathread cannot access unsynchronized globals unless explicitly passed by its master thread.

Each function can be assigned to a metathread or left blank, meaning it is usable from *all* threads
but cannot access globals unless passed as an argument or put behind synchronization.

## Parallel Loops

This language provides a parallel looping mechanism that allows multiple threads to write to a
pre-allocated output area. In order to maintain correctness without needing locks (losing all the
benefits of parallelism), parallel loops have the following restrictions:

* Only the current element viewed by the state of the loop may be written to.
    * All other data must be either local or treated as immutable.
* Parallel loops may not contain other parallel loops or gpu loops.
* All arrays/tables iterated over by the loop must be the same shape.
* No early returns, `goto`, or `break`. (This is why exceptions can't exist in this language)
    * `continue` (which will probably be `skip` instead) is allowed as it merely signals moving onto
      the next element.
* No I/O is allowed within the loop.
* You cannot allocate memory that outlives the loop.
    * The allocator context is set to a scratch arena that is reset at the beginning of each
      iteration of the loop.
    * Setting the allocator is not allowed within the loop or any functions it calls.
* Mutable array(s) being iterated over cannot alias any other values accessed inside the loop.

## GPU Loops

There is also a gpu loop, which will compile your code into a compute shader. In order to conform to
the GPU execution model, it has all of the restrictions of parallel loops, plus the following:

(Note: these restrictions apply to called functions as well)

* Recursive (or indirectly recursive) functions may not be called
* Pointers/references are not allowed
* No allocation whatsoever is allowed
* No variable length arrays are allowed (and, by extension, strings)
    * ...Unless they are uniform across all iterations of the loop
* All internal loops must be bounded and constant
* All data visible to the loop should be explicitly specified (maybe the compiler can infer this?)

## `async`/`await`

`async` and `await` are much more generalized than they are in other languages and do not
necessarily imply coroutines. `async` causes the next expression (i.e. a function call) to be
evaluated by the current context's executor (the thread pool executor for the metathread by default,
but this could also queue up a function in some sort of coroutine scheduler) while `await` waits for
the value returned by a previously `async`'d expression.

Pointers to mutable data cannot alias other data accessed between an `async` and its corresponding
`await` unless the data is a synchronized struct or it is called inside an `unsafe` block. If a
mutable pointer is passed into a const argument, it will be treated as const until all `async`s
referencing that data have been awaited.

`await`ing a value that was not created with `async` implies `await async`. It is a compile-time
error to not `await` a previously `async`'d expression, as this implies a resource leak.

(MAYBE) You may alternatively return the `async` value, as async is a type modifier. The caller is
then responsible for `await`ing your returned value.

`async` can also be applied to a block, creating an implicit function that auto-captures (using
implicit function arguments) all referenced variables. Aliasing rules still apply here.

(MAYBE) `cancel` signals an `async` value to stop executing

## Pipelines

You can create pipelines that chain generators. Generators consume a stream of values of the same
type and output another stream of any number of values of a certain type. This output is designated
with the `yield` keyword. `return` is not allowed in generators. Use `fail` to indicate errors.

In order to ensure that generators have the same semantics regardless of whether they're run on
demand or in parallel, in addition to memory safety, they have the following restrictions:

* Generators may not access any data not passed to them via the stream or parameters.
* The only side effect allowed is memory allocation on the temporary allocator.
* Generators must never assume responsibility for pointers fed to them.

(NOTE) There may be some need for pipeline backpressure of some sort to prevent out-of-memory errors

Syntax will look something like this:

```
func filter(predicate: $T => Bool) [value: T]: T {
    if predicate(value) {
        yield value
    }
}

func map(transform: $T => $R) [value: T]: R {
    yield transform(value)
}

foo := 1..10 ; map(x => x * x + 1) ; filter(math.isprime)  \\ foo is [2, 5, 17, 37]
```

(Pipelines should probably be implemented in userland)


# Built-in Types

* `Int` - signed/unsigned, 8/16/32/64
* `Float` - 32/64
* `Bool`
* `Vec#` - int/float, 2/3/4/8/16, e.g.:
    * `Vec4`
    * `Float32Vec2`
    * `SInt8Vec8`
    * `Float64Vec16`
    * All vector types support swizzles as pseudo-properties (mirroring OpenCL), e.g.
        * `.xxwz` -> elements 0, 0, 3, 2
        * `._1af7` -> elements 1, 10, 15, 7
        * `.lo`, `.hi`, `.even`, `.odd`
* `String` (not avaialable in GPU blocks)
* `Rune` - single codepoint of a string (also not in GPU blocks)

Built-in types are available with various spellings, e.g.

* `Int` = `SInt` = `Int64` = `SInt64` = `S64` = `Integer`
* `Float` = `Float64` = `F64`
* `String` = `Str`
* `Rune` = `Char`
* `Bool` = `Boolean`

The recommended convention is to use `PascalCase` for all types.


## Exact types

These types support `==` and `!=`

* `Int`
* `Bool`
* `String`
* `Char`
* integer `Vec` and `Mat`

## Orderable types

These types support `<`, `<=`, `>`, and `>=`

* `Int`
* `Float`
* `String` (by lexicographic unicode codepoint ordering. Use `collate` for other sorts) - Maybe?

## Arrays

Arrays can be arbitrary-dimensional

Arrays can be sized at compile time or runtime, but only 1D arrays can be resized after being created.

```
resizable: Int[]
matrix: Float[3, 4]    \\ 3 x 4 array (static)
eight_dims: Float[:8]  \\ 8-dimensional array (fixed, but size determined at runtime)
who_knows: Float[*]    \\ any-dimensional array
```

Arrays support broadcasting of functions and operators with scalars. (as in numpy)

```
foo : Int![] = [1, 2, 3, 4]
print(foo ^ 2)  \\ -> [1, 4, 9, 16]
print(8 * foo)  \\ -> [8, 16, 24, 32]
print(sqrt(foo)) \\ -> [1.0, 1.414213, 1.732050, 2.0]
```

Broadcasted op-assignments are only legal if the result of the operator returns the same type as the
array's contained type.

```
foo += 4
print(foo)  \\ -> [5, 6, 7, 8]
foo *= math.PI  \\ type mismatch error!
```

Note: `++` is the array concatenation operator and is not broadcasted

## Mutability

All values are immutable by default when explicitly typed.
To mark something as mutable, add a `!` after its type declaration.
This can be combined with nullability and pointers.

Local variables are as mutable as possible when their type is inferred.
For value types, this will always make them fully mutable, as the value will be copied.

## Nullables

Variables cannot contain null unless their type allows it.
Non-nullable variables use their default values if not initialized.

Nulls do not need to be unwrapped to be used. As long as their value is guaranteed to not be null
when a non-null value is needed, the use is legal.

`.` safely navigates null values; if the left side is null, then the whole unit is null.

Both of these are legal and equivalent:

```
if foo == null {
    return 42
}

\\ foo cannot be null at this point
bar(foo, 42)
```

```
if foo != null {
    bar(foo, 42)
}
else {
    return 42
}
```

This is also legal:

```
if @foo == null {  \\ foo: @?!Int!
    @foo = alloc(Int)
}

bar(foo, 42)
```

However, this is not legal:

```
if foo == null {
    baz()
}

bar(foo, 42)
```

Because `foo` could be null when `bar` is called.

## Pointers

`@` is the symbol designating a pointer and should be placed to the left of the type.
There is a distinction between a pointer to a nullable type and a nullable pointer to a type:

* `@Int?` - pointer to nullable int
* `@?Int` - nullable pointer to int

The underlying implementation details *may* be identical in some cases.

Memory is managed implicitly in many cases. Basic escape analysis will be used to automatically
use temporary allocators (which require no freeing) for values that do not escape but cannot live on
the stack. For values that escape in some code paths, a `free` will be implicitly inserted before
the return and a warning will be optionally emitted with `#warn implicit_free`

Unlike the C family, pointers are dereferenced by default. Each use of `@` will cancel out one level
of dereferencing, thus:

```
foo : @?!Int! = null
bar := 27
baz := 6
@foo = bar  \\ foo now points to bar
foo = 42    \\ value pointed by foo is now 42
print(bar)  \\ -> 42
@foo = baz  \\ foo now points to baz
baz += 1
print(foo)  \\ -> 7
print(@foo) \\ -> (some memory address)
```

Except when inferring a left side type, assignments with `@` on a lone identifier will automatically
add as many `@`s as needed on the right side for the types to match (assuming they are compatible)

```
bar : @@Float = heapval(heapval(13.37))
foo : @@@Float = undefined
\\ All of the following are equivalent:
@@@foo = bar
@@@foo = @bar
@@@foo = @@bar
@@@foo = @@@bar
```

Note that it is only possible to have as many @s as the right side type has, plus one more for
variables in scope (as opposed to literals and temps)

So all the following are valid:

```
@fruit_count = apples
@peter = func_that_returns_a_pointer(a, b, c)
@@jack = ptr_ptr_func()
@@indirect = some.pointer
```

But these are invalid:

```
@thingy = 43
@@jeff = alloc(Float)
@@@very_indirect = alloc(type @String)
```

When a function call requires a pointer, you can still pass a value and it will be implicitly
converted to a pointer:

```
func do_something(a: @Int): Float {
    \\ blah blah blah
}
foo : Int = undefined
set_integer(foo)
```

## Structs

Structs are POD.

* Pointers in structs may be marked as `#owned`, meaning a call to `free` on that struct will also free its contents.
* Recursive pointers (i.e. pointers to a struct of the same type, directly or indirectly) must be nullable.

Struct literals don't exist. Create struct values with a constructor-like syntax instead.

## Unions

Unions are tagged.

`Any` is a tagged union of *every type*

You can create an open union which allows any type to join it later. This allows for something like type polymorphism.

## Tables

A table is conceptually similar to a struct, but it is only stored in homogeneous memory either on
the heap or in static memory. Tables are dynamically sized by default and are ordered arbitrarily.
Tables are instanced by default.
Tables are also *always* mutable.

Tables support the typical insert, delete, update operations, but with function syntax.
They also support fast iteration. There is no garbage collection, so it is your job to delete table
entries when they are no longer needed.

```
table Entity {
    name: String #key
    position: Vec3
    rock_data: @?Rock
    player_data: @?Player
}
```

In this case, rock_data and player_data are optional foreign keys into other tables and behave
similarly to pointers. However, they also have the ability to determine if they're actually pointing
to anything valid. Tables have an implicit index/id that has fast access into entries. Sparse table
foreign keys are generational indexes. Dense tables maintain a separate hashtable for indexing
arbitrary values.

In addition, `name` is designated as a key, so you can access any entry by name. It will be indexed
with an additional hashtable, but also prevents the creation of an entry with a duplicate name.
You can mark multiple fields as a key, treating it as a combined value key.
Key fields must be equality comparable and non-nullable pure value types.
(i.e. Int, String, Char, Bool, IVec and structs containing only those)
You can have multiple sets of keys by appending a number after the #key

tables may only store structs inline. Arrays inside tables always own their contents.

```
table[100] Bullet {
    ...
}
```

The above table is limited to a maximum of 100 entries.

```
table Sprites: Renderer {
    ...
}
```

The above table is a statically defined table available only to the Renderer metathread.

```
table WeakRef: * {
    ...
}
```

The above table can be accessed from all metathreads and is protected by synchronization primitives.

`#sparse` and `#dense` are compiler hints for how to store tables. Dense tables have more consistent
iteration and sparse tables have fast foreign key access and deletion. Dense tables should generally
only be used for relatively small tables where you do not need foreign key access.
Without any hints, it will determine which to use based on usage. (almost always `#sparse`)

The compiler also has the liberty of how to arrange tables in memory based on usage. You can provide
SOA/AOS hints as well.

`#noid` forces the table to be stripped of ids, but disallows foreign keys into it.



# Errors

I haven't yet decided the design of error handling. There's a few methods of handling errors, and
I'm not a fan of any of them:

* Ignore all errors and plow through as if nothing is wrong. (Javascript, PHP, Bash)
    * Although it's pleasant for beginners not to encounter errors constantly, this makes it *a lot*
    harder to track down bugs.
    * This can be sometimes reasonable for less critical errors such as integer overflow.
* Crash immediately upon encountering any error. (Erlang)
    * Requires lightweight processes to make any sense. Not acceptable for this language.
* Force all errors to be handled on the spot. (Zig)
    * This is bad because it introduces a LOT of noise around simple functions and operators that
    fail rarely or don't have a reasonable way to be handled by the immediate caller.
    * Some of this can be mitigated with good syntactical sugar, but Zig already shows signs of
    leading to severe `try` clutter.
* Errors bundled in a tagged union. (Rust, Scala)
    * Same effect of Zig's model, but leveraging the type system instead of a language feature.
    * Error handling looks cryptic/idiomatic and verbose
* Exceptions as traditionally implemented. (Java, C#, Python, C++ when it feels like it)
    * Obscures control flow to runtime context. Acts like a computed goto, where you can never be
    exactly sure where a throw will take you or where a catch will come from since every expression
    is effectively bundled with a "but goto a context-specific catch block sometimes"
    * This model *is* pretty nice for I/O, however, since the handling of a botched file write is
    basically the same no matter which line it is writing.
    * Resource leaks are really easy to create by accident without RAII or something like `with` in
    Python.
* Return codes, whether alone or with multiple return values (C, Perl, Lua, Go, GDScript, Jai)
    * Introduces noise from explicit control flow
    * It's really easy to ignore errors
* Errors are stored in a global variable, queue, or stack (C, OpenGL)
    * Errors are often far-removed from their cause

 I would like to somehow strike a balance between all these options. Goals:

* Certain errors must be handled, e.g.
    * Cannot open file
    * Connection failed/interrupted
* Less important errors can be ignored
* No hidden control flow
* Minimal clutter/boilerplate
* Must not get in the way of composing complex expressions
    * i.e. you don't have to check lots of intermediate values for errors before feeding them into
    function arguments or operands
* Avoids use of repetitive patterns for robust error handling
* Most egregious issues are caught at compile time so that avoidable errors (such as division by
zero and null pointer dereferences) do not occur at runtime in the first place.

## Ideas for better error handling

* Error Channel
    * Behaves similarly to stderr in shell scripts, but with programmatically actionable data
    * Acts as an interrupt when errors occur
    * Errors can be ignored, continuing execution
    * Can handle errors from multiple async sources and complex expressions
    * Problem: is it a good idea to allow continuation of a called function?
    * Wouldn't this require error type hierarchies to work elegantly?
    * Syntax?
* Expression-level Return code accumulation
    * Error values are tagged with a certain degree of critical-ness
        * Sufficiently critical errors terminate an expression's execution at the first offending
        atom (acting like bash `&&`, but with all operators and functions), returning the first
        critical error value.
        * Less critical errors accumulate somehow, to be dealt with later if you're a pedant
        (or perhaps logged)
        * Acceptable error level can be adjusted per scope and per expression
    * Pro: handles operators that potentially have errors. e.g. Integer overflow, pipelines
    * Note: requires strict guidelines on what critical-ness errors should have
        * The definition is simple: an error is critical if you can't do anything useful with the
        return value.
        * Con: certain communities will likely ignore or misinterpret the recommendation, leading to
        ugly error handling in practice
    * Con: does not allow very granular control over which errors are ignored in expressions
    * Con: does not give much information about where the error occured.
* Value substitution
    * For specific cases like divide by zero and integer overflow, for example:
        * `#int_overflow saturate` to replace overflowed values with MAX_INT
        * `#int_overflow wrap` to wrap around on overflow and underflow (the default, probably)
        * `#int_overflow abort` to abort the expression evaluation on overflow and return an error
        * `#float_overflow infinity` to replace float overflow with float infinity (the default)
        * `#float_overflow abort` to abort expressions immediately on float overflow
        * `#divide_by_zero numerator` to make the result of zero division be the same as if no division has occurred
        * `#divide_by_zero saturate` to make the result of zero division be the value represented by all bits set
        * `#divide_by_zero <some value>` to make the result of any zero division be the provided value
        * `#divide_by_zero abort` to abort immediately with an error when zero division occurs
        * `#divide_by_zero unreachable` to trigger static analysis to prevent division by zero (the default, maybe)
    * These apply only at lexical scope level
* Syntax to catch errors from previous statement (usually line)
* Exceptions that don't propagate beyond the current function and act like `return`s


# Compile-time value constraints

## Pre- and Post-condition Checks

This language supports compile-time precondition and post-condition checks.

Anywhere in the body of a function, you can specify `#pre` and `#post` directives taking a single
non-recursive expression. These specify invariants/constraints on inputs and outputs which must be
satisfied for all possible sets of parameters passed into the function. It is a compiler error to
write a function that fails to satisfy its postconditions (given its preconditions) or to call a
function without provably satisfying its preconditions.

When not specifying postconditions, the compiler will assume nothing about possible outputs rather
than inferring them.

## Invariants

Data fields (in structs and tables) can also be tagged with `#invariant`, making it a compile-time
error to set the field to a value that is not guaranteed satisfy the invariant.

## Assumptions

`#assume` has a similar effect to `#invariant`, `#pre`, and `#post`, except that it never checks
that the condition will always hold; it merely assumes that it does. Use with care.

# Inline Tests

Like Zig, this language has a `test` keyword that allows you to write tests.

Test contexts also allow unconventional practices like monkeypatching system functions and data
structures with mock functions/objects, allowing for comprehensive testing of stateful code.

## `#patch`

Much like Python's `unittest.mock`, you have the ability to monkeypatch any function or operator
with a mock function, including system functions and built-in operators.

# Metaprogramming

You can generate arbitrary code at compile time with a collection of `#directives`:

* `#generate` (expression that returns an array of AST nodes) - to insert arbitrary code anywhere
* `#for` (expression) (block) - looping macro to generate nodes from a range
    * `#yield` (expression that returns AST node) - generates a node within a `#for`


# Syntax

Curly brackets *always* denote a lexical scope.
Curly brackets are *mandatory* for all control structures. The C-style parentheses are optional.

Comments start with a double backslash `\\` and continue to the end of the line.
There are no multiline comments.

Character literals may optionally omit the closing single quote.

## Semicolons

This language does not use semicolons to terminate or separate statements.
To make line continuations less error-prone, a backslash followed by any number of non-printable
characters then a newline is treated as a line continuation.
Additionally, when inside parentheses or square brackets, newlines are ignored.

Semicolons instead separate expressions with the following semantics:

* If any of the expressions fail, the whole sequence of `;` fails
* The return value is the value of the last expression in the chain.
* (MAYBE NOT) This is rendered unnecessary by using block expressions `{ ... }`, as those have
similar semantics, with the only difference being that semicolons only separate expressions,
not statements.

## String Literals

Strings are normally surrounded by double quotes, but have a few variants.

- Triple-quotes allow strings to span multiple lines
- Preceding string literals with `\` disables escapes
- Preceding string literals with `$` enables interpolation between `{` and `}`

## Operators

Operator overloads are required to be (nearly) pure functions.
They may not have side effects, except for temporary allocators and debug printing.

You can overload existing operators (except for boolean logic operators) by naming a function
with the same symbols:

```
func + (a: Foo, b: Bar) {
    \\ function body
}
```

### Word Operators

Any 2-argument function can be used as a binary operator by preceding an identifier with a backslash.
The precedence is just barely tighter than comparison operators and it is left-associative

```
    a \cross b
```

3+ arity functions can also be used as binary operators, with the third and subsequent arguments
assuming their default values

```
    a \approx b  \\ approx's third argument is an epsilon/tolerance value
```

It is a compile-time error if the function does not have default.

In all these cases, all arguments *must* be const.

### Arbitrary Symbolic Operators

In addition to being able to override existing binary and unary symbolic operators, you can create
your own from the following set of symbols: `+-*/%^~&|`

The precedence and associativity depends on the first character.
Unless otherwise stated below, the precedence is the same as the first character in isolation

### Compound Assignment

Any operator followed by an equals sign creates a compound assignment statement.
This is only valid if the left hand side is assignable and is known to already have a defined value.

### Operator Precedence

From tightest to loosest: (left-associative unless otherwise stated)

* Parentheses, array/map definitions
* `.`
* Function calls, array access
* `@` - un-dereferencing
* `^` - exponentiation (right-associative)
* Unary operators
* `*`, `/`, `%`, `&`
* `+`, `-`, `~`, `!`
* `\word` operators
* `?` - "elvis" / "or else" / null-coalescing operator (non-associative)
* `x if cond else y` ternary (non-associative)
* `|`
* `=>` - anonymous functions (right-associative)
* Comparison operators `==`, `!=`, `<`, `<=`, `>`, and `>=`  (chainable, as in Python)
* `not`
* `and`
* `or`
* `xor`
* `,` (not really an operator, but this can matter)
* Assignments (statement level only, but can chain right-associatively if all lhs's are assignable)

Boolean operators are spelled out and cannot be overloaded.
Bitwise operations are defined with intrinsic functions rather than symbolic operators
Casts are functions that look like constructors, not operators

## Default Argument Override

The `#default` directive allows you to override a default argument value for the rest of the scope:

```
{
    #default approx(epsilon=0.01)  \\ normally, epsilon=0.001

    if math.pi \approx 3.15 and math.e \approx 2.72 {
        print("This should be printed")
    }
    else {
        print("This is what would be printed in any other block")
    }
}
```

# Type Coercion Rules

`X`, bigger `X` -> bigger `X`
`UInt`, `SInt` -> `SInt` (but not orderable)
`Int`, `Float` -> `Float`
`X`, `Bool` -> `Bool`

@x = y -> @y if x and y point to *exactly* the same type, including `?` and `!` modifiers


# Context

A basic environment containing stuff like allocator state, logging, etc... is passed through the
program - either through an implicit first argument or some sort of thread-local stack.

`with` blocks allow adjustment to context (as well as resource-closing functionality like Python)

## Allocators

The context has two allocators: a primary allocator, and a temporary allocator. Expressions that
drop pointers on the floor are implicitly run inside a context where the primary allocator is set
to the current temporary allocator and the compiler will issue a warning. Temporary allocators are
guaranteed (maybe just assumed?) to be instanced for each thread and coroutine.

(TBD: details about base allocators)

Allocators own certain blocks of memory and must register themselves so that the correct `free`
function will be called.


# Aliasing

Most of the aliasing rules apply only to parallelism, however all mutable pointer parameters are 'restrict'
(in C terms) by default and it is an error if the compilier can't prove that pointers passed into
mutable arguments are not aliasing other arguments. This 'restrict' by default behavior can be
disabled for individual parameters with an `#allow_alias` directive.

Allocators are assumed to return pointers that do not alias anything else in scope.


# The Zen / Goals

* All things in moderation.
* Implicit is better than cluttered.
* Explicit is better than ambiguous.
* Pragmatic is better than traditional.
* Boring is better than trendy.
* Intuitive is better than idiomatic.
* Precise is better than intuitive.
* Pretty doesn't have to mean slow.
* Fast doesn't have to mean ugly.
* Repetition is the root of all evil.
* The things you do most often should have the plainest appearance.
* Code will evolve. Refactoring code should involve little more than search and replace.
* Your data structures tell you more about performance than your code.
* Sharing mutable data is hard. Avoid it.
* It should be hard to do error-prone things.
* Mistakes happen. Make them easy to find.
* Computers are better at finding mistakes than humans.
* Computation should be done with code, not type systems.
* Type checking is not a form of error checking.
