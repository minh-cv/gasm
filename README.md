# gasm
A transpiler written in C++20 that converts between Beta assembly and Gamma, a readable superset of Beta assembly that adds syntactic sugar for common instruction patterns.
Try it now on [Compiler Explorer](https://godbolt.org/z/en8hzPWvK).

An example program of Gamma
```
.include beta.uasm

r1 = 5                      | CMOVE(5, r1)
[mem] = r1                  | ST(r1, mem)
r1 &= 0x3                   | ANDC(r1, 3, r1)
if (r1 == 0) goto mem + 4   | BEQ(r1, mem + 4)

. = 0x400
mem: LONG(0)
HALT()
```

## Feature
* Expression-based syntax: `Rc = Ra + Rb`
* Memory expression: `[expr]`
* Explicit branch statements: `if (cond) goto label`
* Supports most features in Beta, such as directives, labels and `.` symbol

<table>
  <tr>
    <th>Category</th>
    <th>Gamma format</th>
    <th>Beta format</th>
  </tr>

  <tr>
    <td rowspan="8">Arithmetic</td>
    <td><code>r1 = r2 op r3</code></td>
    <td><code>OP(r2, r3, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = r2 op lit</code></td>
    <td><code>OPC(r2, lit, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 op= r2</code></td>
    <td><code>OP(r1, r2, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 op= lit</code></td>
    <td><code>OPC(r1, lit, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = r2</code></td>
    <td><code>MOVE(r2, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = lit</code></td>
    <td><code>CMOVE(lit, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = -r2</code></td>
    <td><code>SUB(r31, r2, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = ~r2</code></td>
    <td><code>XORC(r2, -1, r1)</code></td>
  </tr>

  <tr>
    <td rowspan="7">Memory</td>
    <td><code>r1 = [r2]</code></td>
    <td><code>LD(r2, 0, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = [r2 + lit]</code></td>
    <td><code>LD(r2, lit, r1)</code></td>
  </tr>
  <tr>
    <td><code>r1 = [r2 - lit]</code></td>
    <td><code>LD(r2, -lit, r1)</code></td>
  </tr>
  <tr>
    <td><code>[r1] = r2</code></td>
    <td><code>ST(r1, 0, r2)</code></td>
  </tr>
  <tr>
    <td><code>[r1 + lit] = r2</code></td>
    <td><code>ST(r1, lit, r2)</code></td>
  </tr>
  <tr>
    <td><code>[r1 - lit] = r2</code></td>
    <td><code>ST(r1, -lit, r2)</code></td>
  </tr>
  <tr>
    <td><code>r1 = [lit]</code></td>
    <td><code>LD(lit, r1)</code></td>
  </tr>
  <tr>
    <td></td>
    <td><code>[lit] = r1</code></td>
    <td><code>ST(r1, lit)</code></td>
  </tr>

  <tr>
    <td rowspan="12">Branching</td>
    <td><code>if (r1 != 0) rc = goto label</code></td>
    <td><code>BNE(r1, label, rc)</code></td>
  </tr>
  <tr>
    <td><code>if (r1 == 0) rc = goto label</code></td>
    <td><code>BEQ(r1, label, rc)</code></td>
  </tr>
  <tr>
    <td><code>if (r1 != 0) goto label</code></td>
    <td><code>BNE(r1, label)</code></td>
  </tr>
  <tr>
    <td><code>if (r1 == 0) goto label</code></td>
    <td><code>BEQ(r1, label)</code></td>
  </tr>
  <tr>
    <td><code>if (!r1) rc = goto label</code></td>
    <td><code>BF(r1, label, rc)</code></td>
  </tr>
  <tr>
    <td><code>if (r1) rc = goto label</code></td>
    <td><code>BT(r1, label, rc)</code></td>
  </tr>
  <tr>
    <td><code>if (!r1) goto label</code></td>
    <td><code>BF(r1, label)</code></td>
  </tr>
  <tr>
    <td><code>if (r1) goto label</code></td>
    <td><code>BT(r1, label)</code></td>
  </tr>
  <tr>
    <td><code>rc = goto label</code></td>
    <td><code>BR(label, rc)</code></td>
  </tr>
  <tr>
    <td><code>goto label</code></td>
    <td><code>BR(label)</code></td>
  </tr>
  <tr>
    <td><code>r1 = goto r2</code></td>
    <td><code>JMP(r2, r1)</code></td>
  </tr>
  <tr>
    <td><code>goto r1</code></td>
    <td><code>JMP(r1)</code></td>
  </tr>
</table>

## Documentation
* For Gamma language and gasm transpiler documentation, go to `/docs` (currently unavailable).
* For examples, go to `/examples`.

## Build
The project requires CMake 3.20+ and a C++20 compiler

```bash
cmake -B build
cmake --build build
```

## Usage
```
gasm <source-file> [-b | -g | --debug]  
```
Options:
* `-g`: convert the source file to Gamma
* `-b`: convert the source file to Beta (default option).
* `--debug`: enable debug output.
The converted result is printed to stdout. Redirect to another file if needed.
```bash
gasm program.uasm -g > program.gasm
```

## Status

The language and transpiler features are mostly complete and subject only to minor changes. However, the transpiler implementation may still be refactored to improve testing and maintainability.

Possible changes include:
- `r(` may no longer be lexed as a single token.
- Improvements to the command-line interface.
- Better preservation of whitespace and newlines.