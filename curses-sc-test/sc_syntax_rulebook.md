# SuperCollider Syntax Rulebook

## 1. Basic Syntax Rules

### Statement Termination
- Statements must end with a semicolon `;`
- Multiple statements on one line require semicolons between them
- The last statement in a code block can omit the semicolon

### Comments
- Single-line comments: `// comment`
- Multi-line comments: `/* comment */`

### Case Sensitivity
- SuperCollider is case-sensitive
- Class names must start with uppercase letters
- Variable and method names typically start with lowercase letters

## 2. Variables

### Declaration and Assignment
- Local variables: `var name;` or `var name = value;`
- Multiple declarations: `var a, b, c;`
- Environment variables: `~varName = value;` (no declaration needed)
- Global variables: Start with lowercase letter, no declaration needed (discouraged)
- Class variables: Must be declared in class definition with `classvar`
- Instance variables: Must be declared in class definition with `var`

### CRITICAL: Variable Declaration Placement Rule
**ALL `var` declarations MUST appear at the VERY BEGINNING of their scope, before ANY other statements or expressions.**

```
// WRONG - var after statement
{
    var x = 5;
    x = x + 1;
    var y = 10;  // ERROR: cannot declare var after statements
}

// CORRECT - all vars declared first
{
    var x, y;
    x = 5;
    x = x + 1;
    y = 10;
}

// WRONG - var with initialization mixed with statements
{
    var x = msg[0];
    var y = msg[1];
    var z = x + y;  // This is a statement (assignment)
    var w = z * 2;  // ERROR: var after statement
}

// CORRECT - declare all vars, then assign
{
    var x, y, z, w;
    x = msg[0];
    y = msg[1];
    z = x + y;
    w = z * 2;
}
```

### Variable Scope Rules
- Variables declared with `var` are scoped to their containing function or code block
- Variables must be declared at the beginning of their scope (before any other statements)
- Environment variables (`~name`) are globally accessible within the current environment

### Valid Variable Names
- Must start with a letter (a-z, A-Z) or underscore
- Can contain letters, numbers, and underscores
- Cannot be reserved keywords

## 3. Data Types and Literals

### Numbers
- Integers: `42`, `-17`, `0`
- Floats: `3.14`, `-0.5`, `1.0`
- Exponential notation: `1e6`, `3.2e-4`
- Hexadecimal: `0x1F` (prefix with 0x)
- Radix notation: `2r1010` (binary), `16rFF` (hex)

### Strings
- Single quotes: `'hello'`
- Double quotes for formatted strings: `"hello"`
- String concatenation: `"hello" ++ "world"`
- Escape sequences: `\n`, `\t`, `\\`, `\'`, `\"`

### Symbols
- Literal symbols: `\symbol`, `'symbol'`
- Symbols are unique and interned
- Use for identifiers, keys, and names

### Booleans
- `true` and `false` (lowercase)

### Nil
- `nil` represents null/nothing

### Collections
- Arrays: `[1, 2, 3]` or `Array[1, 2, 3]`
- Lists: `List[1, 2, 3]`
- Sets: `Set[1, 2, 3]`
- Dictionaries: `Dictionary[\key -> value]`
- Events: `(key: value, key2: value2)`

## 4. Operators

### Arithmetic
- `+`, `-`, `*`, `/`, `%` (modulo), `**` (power)

### Comparison
- `==` (equal), `!=` (not equal)
- `<`, `>`, `<=`, `>=`
- `===` (identity comparison)

### Logical
- `&&` (and), `||` (or), `!` (not)
- `and:`, `or:` (short-circuit versions)

### Nil-Safe Operators
- `!?` (inWhile) - execute if not nil: `object !? _.method`
- `??` (nil coalescing) - use default if nil: `object ?? { defaultValue }`
- **IMPORTANT:** SuperCollider does NOT support `?.` optional chaining operator

### Assignment
- `=` (assignment)
- Compound: `+=`, `-=`, `*=`, `/=`

### Precedence
- Use parentheses to override default precedence
- Method calls have higher precedence than binary operators

## 5. Functions

### Function Definition
```
// Basic function
f = { arg x, y; x + y };

// With local variables
f = { arg x, y;
    var result;
    result = x + y;
    result;
};

// Shorthand argument syntax
f = { |x, y| x + y };

// Default arguments
f = { arg x = 0, y = 1; x + y };
```

### Function Calls
- `func.value(arg1, arg2)`
- `func.(arg1, arg2)` (shorthand)
- Keyword arguments: `func.value(x: 10, y: 20)`

### Return Values
- Functions return the value of the last expression
- Explicit return: `^value` (caret symbol)

## 6. Control Structures

### Conditionals
```
// if statement
if(condition, { trueAction }, { falseAction });

// if without else
if(condition, { action });

// case statement
case
    { condition1 } { action1 }
    { condition2 } { action2 }
    { defaultAction };
```

### Loops
```
// do loop
5.do({ arg i; i.postln });

// for loop
for(0, 9, { arg i; i.postln });

// while loop
while({ condition }, { action });

// loop forever
loop({ action });
```

### Iteration
```
// Iterate over collection
[1, 2, 3].do({ arg item, index; ... });

// Collect results
[1, 2, 3].collect({ arg item; item * 2 });

// Select/filter
[1, 2, 3].select({ arg item; item > 1 });
```

## 7. Classes and Objects

### Object Creation
- `ClassName.new(args)`
- `ClassName(args)` (shorthand for new)

### Method Calls
- Instance methods: `object.method(args)`
- Class methods: `ClassName.method(args)`
- Chaining: `object.method1.method2.method3`

### Message Syntax
- Unary messages: `receiver.message`
- Binary messages: `receiver + argument`
- Keyword messages: `receiver.method(arg1, arg2)`

## 8. Server-Specific Syntax

### UGen Graphs
```
// SynthDef structure
SynthDef(\name, {
    var signal;
    signal = SinOsc.ar(440, 0, 0.5);
    Out.ar(0, signal);
}).add;
```

### Multi-channel Expansion
- Arrays create multiple channels: `SinOsc.ar([440, 442])`
- Automatic expansion: `Pan2.ar(sig, [-1, 1])`

### Control Rate vs Audio Rate
- `.ar` for audio-rate UGens
- `.kr` for control-rate UGens
- `.ir` for initialization-rate (computed once)

## 9. Common Syntax Errors

### Missing Semicolons
**Error:** Statements not properly terminated
```
// Wrong
var x = 5
var y = 10

// Correct
var x = 5;
var y = 10;
```

### Variable Declaration Placement
**Error:** Variables declared after statements
```
// Wrong
x = 5;
var x;

// Correct
var x;
x = 5;
```

### Mismatched Brackets
**Error:** Unbalanced parentheses, brackets, or braces
- Always ensure opening and closing pairs match
- Use proper nesting

### Incorrect Class Names
**Error:** Class names starting with lowercase
```
// Wrong
myClass.new

// Correct
MyClass.new
```

### Return Statement Misuse
**Error:** Using `return` instead of `^`
```
// Wrong
{ arg x; return x * 2; }

// Correct
{ arg x; ^(x * 2); }
```

### Undefined Variables
**Error:** Using variables before declaration
- Always declare variables with `var` or use environment variables (`~name`)

### Negating Variables in Method Arguments
**Error:** Using `-variable` directly in method argument lists causes parser ambiguity
```
// Wrong - parser cannot distinguish between literal and negation
SinOsc.kr(440).range(-depth, depth)

// Correct - use .neg method
SinOsc.kr(440).range(depth.neg, depth)

// Also correct - other alternatives
SinOsc.kr(440).range(0 - depth, depth)
SinOsc.kr(440).range(depth * -1, depth)
```
**Why:** The parser cannot disambiguate `-variable` from a negative number literal like `-0.5` in argument position. Always use `.neg`, subtraction, or multiplication for variable negation in method calls.

### Invalid Optional Chaining Syntax
**Error:** Using `?.` optional chaining operator (not valid in SuperCollider)
```
// Wrong - ?. does not exist in SuperCollider
object?.method;
~var?.free;

// Correct - use !? (inWhile operator)
object !? _.method;
~var !? _.free;

// Alternative - explicit nil check
if(object.notNil) { object.method };
```
**Why:** SuperCollider does not support the `?.` optional chaining syntax found in Swift, Kotlin, or JavaScript. Use the `!?` operator instead, which executes the following function only if the receiver is not nil.

## 10. Best Practices

### Code Organization
- Declare all variables at the start of their scope
- Use meaningful variable and function names
- Keep functions focused and concise
- Comment complex logic

### Naming Conventions
- Classes: `UpperCamelCase`
- Methods and variables: `lowerCamelCase`
- Constants: `kConstantName` or `CONSTANT_NAME`
- Private methods: `prMethodName`

### Server Communication
- Always boot the server before creating Synths: `s.boot;`
- Wait for server to respond: `s.waitForBoot({ ... });`
- Free resources when done: `synth.free;`

### Memory Management
- Free Buffers and Busses when no longer needed
- Use `.free` method on resources
- Clear node tree when resetting: `s.freeAll;`

## 11. Expression Evaluation

### Blocks vs Parentheses
- `{ }` creates a Function object (not evaluated immediately)
- `( )` groups expressions (evaluated immediately)

### Implicit Returns
- Last expression in function is returned
- Use `^` for early return

### Method Chaining
- Methods return values that can be chained
- Chain works left to right: `object.method1.method2`

## 12. Special Syntax

### Nil-Safe Operations
SuperCollider provides operators for safe nil handling:

**inWhile operator (`!?`)** - Execute only if not nil:
```
// Basic usage
object !? _.method;          // calls method if object is not nil
~var !? _.free;              // frees if ~var exists

// With blocks
value !? { |v| v.squared };  // executes block if value is not nil

// Common pattern
~synth !? _.set(\freq, 440); // set parameter only if synth exists
```

**Nil coalescing operator (`??`)** - Provide default if nil:
```
// Basic usage
result = object ?? { defaultValue };  // use default if object is nil
~param = ~param ?? 0.5;               // initialize with default

// Chaining
value = x ?? y ?? z ?? 0;             // first non-nil value or 0
```

**NEVER use `?.` optional chaining** - This syntax does NOT exist in SuperCollider and will cause parse errors.

### Argument Lists
- Named arguments: `object.method(argName: value)`
- Rest arguments: `{ arg ...args; }`

### Partial Application
- `_.method` creates a partial application
- Example: `[1, 2, 3].collect(_.squared)`

### adverb syntax
- Collection operations with adverbs: `+ .s`, `* .f`

### Array Series
- Arithmetic series: `(1, 3..10)` creates `[1, 3, 5, 7, 9]`
- Geometric series: `(1, 2, 4..16)`

## 13. Debugging Tips

### Check for:
- Unbalanced brackets, braces, or parentheses
- Missing semicolons (especially after variable declarations)
- Variables used before declaration
- Incorrect capitalization in class names
- Mismatched argument counts in function calls
- Attempting to use undefined methods
- Server not booted when creating Synths
- Missing `.add` on SynthDefs before use

### Use Post Window
- `.postln` to print values
- `.debug("label")` to inspect values
- `.class` to check object type
- `.dump` to see object structure

## 14. Quick Reference

### Reserved Keywords
`var`, `arg`, `classvar`, `const`, `nil`, `true`, `false`, `inf`, `this`, `super`, `thisProcess`, `thisThread`, `thisMethod`, `thisFunction`, `thisFunctionDef`

### Common Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`, `===`
- Logical: `&&`, `||`, `!`, `and:`, `or:`
- Nil-safe: `!?` (if not nil), `??` (nil coalescing)
- **Invalid:** `?.` (does not exist in SuperCollider)

### Common Methods
- Collections: `.do`, `.collect`, `.select`, `.reject`, `.size`, `.at`, `.put`
- Math: `.abs`, `.neg`, `.reciprocal`, `.sqrt`, `.sin`, `.cos`, `.tan`
- Control: `.value`, `.play`, `.stop`, `.free`, `.set`
- Nil checking: `.isNil`, `.notNil`

### Message Precedence
1. Unary messages (highest)
2. Binary messages
3. Keyword messages (lowest)
4. Use parentheses to override