# Concrete syntax {#sec:syntax}

This chapter gives the text syntax of `.sg` files: lexical rules, a grammar
in Extended Backus–Naur Form (EBNF), and the rules for using the same text
on the command line. The meaning of each construct is defined in the
previous chapters; here we only fix how it is written.

## Lexical rules {#sec:lexical}

**Characters.** Files are UTF-8. Outside strings and comments, only ASCII
characters are significant; `µs` is accepted as a synonym of `us`.

**Comments.** A `#` outside a string starts a comment that runs to the end of
the line.

**Whitespace and line breaks.** Spaces and tabs separate tokens and are
otherwise ignored. A line break *separates segments* (like `;`) when it
occurs directly inside a sequence, i.e. at the top level of a waveform or
directly inside braces. A line break is ignored, and the expression
continues on the next line, when it occurs

- inside round parentheses or square brackets, or
- after a binary operator (`+ - * /`), a comma, or `=`.

Blank lines and lines holding only a comment are ignored.

**Identifiers.** `[A-Za-z_][A-Za-z0-9_]*`. Names are case-sensitive.
Keywords (such as `repeat`, `prev`, `channel`, `sweep`) are recognised only
in the positions where the grammar expects them, so that, for instance,
`from` and `to` remain usable as parameter names of `ramp`.

**Numbers.** `[0-9]+(\.[0-9]*)?([eE][+-]?[0-9]+)?` or a leading-dot form
`\.[0-9]+(...)`. A sign is an operator, not part of the number.

**Quantities.** A number followed *immediately* (without a space) by a unit
is a quantity, e.g. `500ms`, `20kHz`, `-300pA` (unary minus applied to
`300pA`). The units are:

| Dimension | Units |
|:--|:--|
| time | `s`, `ms`, `us` (`µs`), `ns`, `min` |
| frequency | `Hz`, `kHz`, `MHz` |
| phase | `rad`, `deg` |
| fraction | `%` |
| current | `fA`, `pA`, `nA`, `uA`, `mA`, `A` |
| voltage | `uV`, `mV`, `V` |
| conductance | `pS`, `nS`, `uS`, `mS`, `S` |

: Unit suffixes. Units are case-sensitive: `ms` is a millisecond, `mS` a millisiemens. {#tbl:unit-tokens}

**Strings.** Text in double quotes, with `\"` and `\\` as the only escapes.
Strings are used for file names.

## Grammar

The grammar below uses ISO EBNF conventions: `{ x }` is zero or more
repetitions, `[ x ]` is optional, `|` separates alternatives, and quoted
text is literal. `SEP` stands for `;` or a separating line break, as defined
above.

### Files

```ebnf
file        = [ header ] ( waveform | stimulus | protocol | index ) ;
header      = "sg" , version , kind , SEP ;
version     = number ;                      (* "2" for this specification *)
kind        = "waveform" | "stimulus" | "protocol" | "index" ;
```

A file without a header is a waveform. The *kind* in the header MUST match
the content.

### Waveforms

```ebnf
waveform    = sequence ;
sequence    = item , { SEP , item } ;
item        = label | segment ;
label       = "@" , identifier ;
segment     = [ label ] , [ duration ] , expr ;
duration    = quantity | substitution ;     (* of dimension time *)

expr        = term , { ( "+" | "-" ) , term } ;
term        = unary , { ( "*" | "/" ) , unary } ;
unary       = [ "-" ] , primary ;
primary     = number | quantity | "prev" | substitution
            | call | block | repeat | "(" , expr , ")" ;

call        = identifier , "(" , [ args ] , ")" ;
args        = arg , { "," , arg } ;
arg         = [ identifier , "=" ] , value ;
value       = expr | string | identifier | list ;
list        = "[" , [ value , { "," , value } ] , "]" ;

block       = "{" , sequence , "}" ;
repeat      = "repeat" , count , block ;
count       = number | substitution ;       (* a non-negative integer *)
```

`call` covers primitives (@sec:primitives), maps (@tbl:maps), and the binary
functions `min` and `max`. An `identifier` as a `value` is a keyword
argument such as `law=exp` or `clock=global`.

**Duration rule.** A segment that begins with a quantity of dimension time
takes that quantity as its duration. Since amplitudes never have the
dimension of time, this resolves the only potential ambiguity: `1s -0.5`
is a segment of 1 s with value $-0.5$, not the difference $1\,\text{s} - 0.5$.

A label on its own line labels the next segment; a label may also precede a
segment on the same line, as in `@on 500ms dc(300)`.

### Stimuli

```ebnf
stimulus    = stim_stmt , { SEP , stim_stmt } ;
stim_stmt   = "rate" , quantity
            | "seed" , ( number | substitution )
            | "duration" , quantity
            | "channel" , identifier , { attribute } , source
            | "digital" , identifier , { attribute } , source
            | "marker" , identifier , "at" , quantity , { "," , quantity } ;
attribute   = ( "unit" , "=" , unit ) | ( "rest" , "=" , expr ) ;
source      = block | ( "use" , string ) | ( "copy" , identifier ) ;
```

### Protocols

```ebnf
protocol    = prot_stmt , { SEP , prot_stmt } ;
prot_stmt   = "stimulus" , ( "{" , stimulus , "}" | string )
            | "sweep" , names , "=" , values
            | "let" , identifier , "=" , expr
            | "repeat" , number
            | "order" , ( "sequential" | "grouped" | "shuffled" | "shuffled-blocks" )
            | ( "period" | "gap" ) , quantity
            | "seed" , number
            | "noise" , ( "per-trial" | "per-condition" | "fixed" )
            | "start" , ( "immediately" | "on" , "trigger" ) ;
names       = identifier | "(" , identifier , { "," , identifier } , ")" ;
values      = list
            | "from" , expr , "to" , expr , "step" , expr
            | ( "linspace" | "logspace" ) ,
              "(" , expr , "," , expr , "," , number , ")" ;
substitution = "$" , identifier | "$(" , expr , ")" ;
```

Inside the expressions of `let` and `$( ... )`, identifiers denote
variables. Substitutions are allowed only in the stimulus template of a
protocol. They are replaced textually before the template is parsed as a
stimulus (@sec:protocol), so the expanded stimulus contains no `$`.

### Index files

```ebnf
index       = { index_stmt , SEP } ;
index_stmt  = ( "period" | "gap" ) , quantity
            | "start" , ( "immediately" | "on" , "trigger" )
            | string_or_name , [ "seed" , "=" , number ] ;
```

where `string_or_name` is a file name, quoted if it contains spaces.

## Positional and named arguments

The parameters of a call can be given by position, in the order of the
parameter table of the primitive, by name, or both, with all positional
arguments first. Each parameter may be given at most once. Thus
`sine(3, 1Hz)`, `sine(amp=3, freq=1Hz)`, and `sine(3, freq=1Hz)` are
equivalent. We recommend named arguments for every parameter after the
first two, which keeps descriptions readable months later.

## Canonical form

Every description has a *canonical form*, used in provenance records and for
comparing descriptions (@sec:output). The canonical form is obtained by:
removing comments and blank lines; writing one segment or statement per line
with single spaces between tokens; writing all arguments by name, in the
order of the parameter table, including defaults; and writing every quantity
in its canonical unit with the shortest decimal representation (e.g. `5ms`
becomes `0.005s`). Two descriptions with the same canonical form have the
same meaning.

## Command line {#sec:cli}

The same text can be passed as a command-line argument instead of a file.
Since line breaks are inconvenient there, segments are separated by `;`, and
the text is quoted to protect it from the shell:

```
sg render --rate 20kHz '2s dc(0); 500ms dc(300); 2s dc(0)'
```

Because a segment is a single argument, negative values need no special
treatment, unlike programs whose options begin with a dash.

The names and options of the reference tools are not part of this
specification. As an illustration, a minimal tool set could offer:

| Command | Purpose |
|:--|:--|
| `sg check FILE` | parse and run the static checks of @sec:static-checks |
| `sg canon FILE` | print the canonical form |
| `sg render FILE` | realise samples to an output file (@sec:output) |
| `sg render PROTOCOL` | realise every trial, one output file per trial |
| `sg render DIR` | realise the trials of a directory form |
| `sg expand PROTOCOL DIR` | write the directory form of a protocol (@sec:directory-form) |
| `sg play FILE` | play a stimulus or protocol on the configured hardware |

: An illustrative command-line tool set (informative). {#tbl:cli}
