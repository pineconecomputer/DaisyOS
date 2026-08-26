
# DaisyBASIC Programmer's Reference Manual

---

## INTRODUCTION

DaisyBASIC is an interactive programming language for the Daisy computer. Programs
are composed of numbered lines that are stored and executed in numerical order. You
can type commands directly (immediate mode) or enter numbered lines to build a
program.

### Screen Layout

The display is **40 columns wide** by **25 rows tall**. Column and row numbers
start at **0**. The home position (top-left corner) is column 0, row 0.

### Line Numbers

Line numbers must be integers from **1 to 65535**. Lines are stored and executed
in ascending numerical order. Entering a line number alone (with no statement)
deletes that line.

    10 PRINT "HELLO"
    20 GOTO 10

### Multiple Statements Per Line

Separate multiple statements on one line with a colon (`:`).

    10 FOR I=1 TO 5 : PRINT I : NEXT

### Running a Program

    RUN             — start from the first line
    RUN 100         — start from line 100
    CONT            — continue after a BREAK (Shift+AC)
    LIST            — list all lines
    LIST 10-50      — list lines 10 through 50
    NEW             — erase program and all variables
    CLR             — erase variables (keep program)

### Stopping a Program

Press **BREAK** (Shift+AC) to interrupt a running program. Use `CONT` to resume.
Press **Scroll Lock** to pause; press Scroll Lock or BREAK again to resume.

---

## VARIABLES

### Naming Rules

Variable names start with a letter and may contain letters and digits, up to a
maximum of **13 characters including any `$` or `%` suffix**. Longer names are
truncated. A trailing `$` means string; a trailing `%` means integer. All three
base-name variants exist independently of one another.

| Suffix | Type    | Example  |
|--------|---------|----------|
| (none) | Numeric | `SCORE`  |
| `%`    | Integer | `COUNT%` |
| `$`    | String  | `NAME$`  |

Numeric variables without `%` are stored as floating-point when needed; values that
fit in a 16-bit integer are stored as integers automatically.

Strings are limited to **80 characters**. This applies equally to string scalars
and to string **array** elements. Assigning a longer string silently truncates it
to 80 characters — no error is raised.

Variables do not need to be declared; they spring into existence when first
assigned. An undefined numeric variable has value **0**; an undefined string
variable has value **""**.

### LET — Assign a Variable

    LET var = expression
    var = expression            ← implicit LET (LET keyword optional)

    10 LET X = 3.14
    20 NAME$ = "DAISY"
    30 SCORE% = 100

### Coexistence Rule

A scalar and an array may share the same base name; they are completely independent.

    A = 5           ← scalar A
    A(0) = 99       ← element 0 of array A()  (requires prior DIM)

---

## OPERATORS

### Arithmetic

| Operator | Meaning        | Example      |
|----------|----------------|--------------|
| `+`      | Addition       | `A + B`      |
| `-`      | Subtraction    | `A - B`      |
| `*`      | Multiplication | `A * B`      |
| `/`      | Division       | `A / B`      |
| `^`      | Exponentiation | `2 ^ 8`      |
| `MOD`    | Modulo         | `10 MOD 3`   |

Operator precedence (highest to lowest): `^`, `*` `/` `MOD`, `+` `-`.
Use parentheses to override.

`MOD` is spelled as a word, as in GW-BASIC, which leaves `%` to mean the
integer type suffix everywhere it appears. `MOD` is a reserved word and
cannot be used as a variable name.

### Comparison (used in IF and WHILE/UNTIL)

| Operator | Meaning              |
|----------|----------------------|
| `=`      | Equal                |
| `<`      | Less than            |
| `>`      | Greater than         |
| `<=`     | Less than or equal   |
| `>=`     | Greater than or equal|
| `<>`     | Not equal            |

String comparisons are alphabetical (case-sensitive).

### Logical (for compound conditions)

    IF A>0 AND B>0 THEN ...
    IF X=1 OR Y=1 THEN ...
    IF NOT FLAG THEN ...

### Bitwise

| Operator | Meaning          | Example          |
|----------|------------------|------------------|
| `AND`    | Bitwise AND      | `A AND 255`      |
| `OR`     | Bitwise OR       | `A OR 128`       |
| `XOR`    | Bitwise XOR      | `A XOR 255`      |
| `NOT`    | Bitwise NOT      | `NOT A`          |
| `<<`     | Left shift       | `1 << 3`         |
| `>>`     | Right shift      | `128 >> 2`       |

---

## ARRAYS

### DIM — Declare an Array

    DIM name(n)             ← 1-D array; indices 0 to n-1
    DIM name(rows, cols)    ← 2-D array; indices (0,0) to (rows-1,cols-1)
    DIM A(10), B%(5), C$(8) ← multiple arrays in one statement

`n` gives the **total number of elements** (indices 0 through n-1). String arrays
use `$`; integer arrays use `%`; float arrays have no suffix.

    10 DIM SCORE%(10)
    20 FOR I=0 TO 9 : SCORE%(I)=0 : NEXT

### Accessing Array Elements

    A(i)          ← 1-D element i
    M%(row, col)  ← 2-D element

### Bulk Assignment

    LET A$(0) = "RED","GREEN","BLUE"

Assigns comma-separated values starting at index 0. Gives `?BAD SUBSCRIPT ERROR` if
the list would exceed the array's declared size.

### CLEARARR — Zero an Array

    CLEARARR name [(startIndex)] [, name [(startIndex)] ...]

Zeroes all elements from `startIndex` (default 0) to the end of each named
array. Silently ignores arrays that have not been `DIM`'d. Accepts multiple
arrays separated by commas.

    CLEARARR SCORE%            ← zero the entire SCORE% array
    CLEARARR BUF$(2)           ← zero from index 2 onwards

### SWAPARR — Swap or Rotate Arrays

    SWAPARR a [(idx)], b [(idx)]             ← swap a and b
    SWAPARR a [(idx)], b [(idx)], c [(idx)]  ← rotate: a←b, b←c, c←a

Swaps the contents of two arrays element-by-element, or performs a
three-way rotation. An optional starting index limits the operation to that
portion of each array. Arrays need not be the same size; shorter arrays are
zero-filled in excess slots.

    SWAPARR OLD%, NEW%         ← swap two sprite buffers
    SWAPARR A, B, C            ← rotate: A gets B's values, B gets C's, C gets A's

---

## INPUT AND OUTPUT

### PRINT — Display Output

    PRINT                       ← blank line
    PRINT expr                  ← print value, then newline
    PRINT expr ; expr           ← print values with no separator
    PRINT expr , expr           ← print values at tab stops (every 10 columns)
    PRINT expr ;                ← print without a trailing newline

`expr` may be a number, a string literal `"..."`, or a variable.

**AT** and **TAB** modifiers:

    PRINT AT(col, row) "TEXT"   ← move cursor to col,row then print
    PRINT TAB(col) expr         ← advance cursor to column col

    10 PRINT AT(0,0) "SCORE:"; SCORE

### INPUT — Read from Keyboard

    INPUT var
    INPUT var1, var2            ← comma-separated values on one line
    INPUT var ;                 ← do not print newline after entry
    INPUT A$(0)                 ← read into array element

Waits for the user to type a value and press Enter. Multiple variables receive
fields separated by commas. Ctrl-C aborts input.

### GET — Read a Single Keypress (Non-Blocking)

    GET var

Reads one key from the keyboard buffer without waiting. If no key is available,
numeric variables receive **0** and string variables receive **""**.

    10 GET K : IF K=0 THEN 10   ← wait for a key
    20 PRINT "KEY CODE:"; K

Key codes for special keys:

| Key        | Code |
|------------|------|
| Up arrow   | 1    |
| Right arrow| 2    |
| Down arrow | 3    |
| Left arrow | 4    |
| Return     | 13   |
| Space      | 32   |

**Note:** BREAK (Shift+AC) is intercepted by the system for program break and
is never returned by `GET`.

### LOCATE — Move the Cursor

    LOCATE col, row

Moves the text cursor to the specified column (0–39) and row (0–24) without
printing anything.

    LOCATE 20, 12 : PRINT "CENTER"

### CLS — Clear Screen

    CLS

Clears the screen and moves the cursor to the home position (0, 0).

---

## PROGRAM FLOW

### REM — Remark (Comment)

    REM anything

The rest of the line is ignored. Use remarks to annotate your program.

    10 REM *** MAIN LOOP ***

### END — Stop the Program

    END

Stops program execution and returns to the `Ready.` prompt.

### GOTO — Jump to a Line

    GOTO linenum

Unconditionally jumps to the given line number.

    10 PRINT "LOOPING"
    20 GOTO 10

### IF / THEN — Conditional Execution

    IF condition THEN statement
    IF condition THEN linenum
    IF condition THEN stmt1 : stmt2 : ...

Evaluates `condition`. If true, executes the THEN clause; otherwise skips to
the next line. Conditions may be combined with `AND` and `OR`.

    10 IF SCORE > 100 THEN PRINT "HIGH SCORE!" : GOSUB 9000
    20 IF A$ = "YES" OR A$ = "Y" THEN GOTO 100

### FOR / NEXT — Counted Loop

    FOR var = start TO end
      statements
    NEXT [var]

    FOR var = start TO end STEP step
      statements
    NEXT [var]

Loops with `var` running from `start` to `end` inclusive. `STEP` may be
negative to count downward. If `NEXT` is given a variable name it must match
the most recent `FOR`. Loops may be nested.

    10 FOR I = 1 TO 10
    20   PRINT I
    30 NEXT I

    ← single-line form also works:
    10 FOR N = 0 TO 3 : PRINT N : NEXT

### WHILE / WEND — Pre-Test Loop

    WHILE condition
      statements
    WEND

Evaluates `condition` before each iteration. If false at the start, the body
is never executed. Program mode only.

    10 I = 0
    20 WHILE I < 5
    30   PRINT I
    40   I = I + 1
    50 WEND

### DO / UNTIL — Post-Test Loop

    DO
      statements
    UNTIL condition

Executes the body at least once, then repeats until `condition` is true.
Program mode only.

    10 DO
    20   GET K
    30 UNTIL K <> 0

### EXIT — Break Out of a Loop

    EXIT

Immediately exits the innermost `WHILE`/`WEND` or `DO`/`UNTIL` loop, jumping
to the statement after the closing keyword. Program mode only. Works from
inside an `IF THEN`.

    10 WHILE 1
    20   GET K
    30   IF K = 27 THEN EXIT
    40 WEND

### GOSUB / RETURN — Subroutine Call

    GOSUB linenum
    RETURN
    RETURN expr
    RETURN expr1, expr2

`GOSUB` saves the return address and jumps to `linenum`. `RETURN` resumes
execution at the statement following the `GOSUB`. Subroutines may be nested.

`RETURN` may optionally pass up to two numeric values back to the caller.
The values are retrieved with `RESULT(1)` and `RESULT(2)` after the `GOSUB`
returns. Every `RETURN` resets the result state; call `RESULT` before the
next `GOSUB` if you need the values.

    10 GOSUB 1000
    20 PRINT RESULT(1), RESULT(2)
    30 END
    1000 RETURN 42, 7

    10 GOSUB 1000
    20 END
    1000 PRINT "SUBROUTINE"
    1010 RETURN

### ON ... GOTO / ON ... GOSUB — Computed Jump

    ON expression GOTO line1, line2, line3, ...
    ON expression GOSUB line1, line2, line3, ...

Evaluates `expression` as an integer index (0 = first entry). Jumps to or calls
the corresponding line. If the index is out of range, gives `?ILLEGAL QUANTITY ERROR`.

    10 ON CHOICE GOTO 100, 200, 300

---

## DATA AND READ

Use `DATA` to embed constant values in a program. `READ` fetches the next value.
`RESTORE` resets the pointer.

### DATA — Embed Constants

    DATA value, value, ...
    DATA "string", number, ...

`DATA` lines may appear anywhere in the program; they are skipped during normal
execution. Values are read in order of line number.

### READ — Fetch the Next Data Value

    READ var
    READ var1, var2, ...

Reads the next item(s) from `DATA` into the given variables. Gives
`?OUT OF DATA ERROR` if no more items remain.

    10 DATA 10, 20, 30
    20 READ A, B, C
    30 PRINT A; B; C

### RESTORE — Reset the Data Pointer

    RESTORE             ← reset to first DATA statement
    RESTORE linenum     ← reset cursor to the first DATA at or after linenum

Allows re-reading `DATA` from the beginning or from a specific line.
`linenum` does not have to be a DATA line itself — the next READ scans
forward from there for the next DATA statement. `?UNDEF'D STATEMENT
ERROR` if `linenum` is past the end of the program.

### READMAT — Read a Whole Array from DATA

    READMAT arrayname

Reads exactly as many values as the array contains (product of its dimensions),
filling elements in order (row-major for 2-D arrays).

    10 DIM V(5)
    20 DATA 1, 2, 3, 4, 5
    30 READMAT V
    40 FOR I=0 TO 4 : PRINT V(I) : NEXT

---

## USER-DEFINED FUNCTIONS

### DEF FN — Define a Function

    DEF FN name(param) = expression

Defines a numeric function that can be called with `FN name(value)`. Up to 16
functions may be defined. Functions are cleared by `RUN`, `NEW`, and `CLR`.

    10 DEF FN SQ(X) = X * X
    20 PRINT FN SQ(7)           ← prints 49

The parameter name in the expression must match exactly.

---

## NUMERIC FUNCTIONS

These return a number and may be used anywhere a number is expected.

### ABS(n) — Absolute Value

    ABS(-5)     → 5

### INT(n) — Integer Part

    INT(3.7)    → 3
    INT(-3.7)   → -3

Truncates toward zero (does not round down).

### SQR(n) — Square Root

    SQR(16)     → 4

### SIN(n), COS(n), TAN(n) — Trigonometry

By default arguments are in **radians**. Use `DEG` to switch to degrees mode.

    SIN(3.14159)  → ~0
    COS(0)        → 1
    DEG : PRINT SIN(90)    → 1

### DEG / RAD — Trig Angle Mode

    DEG     ← switch SIN/COS/TAN to degrees mode
    RAD     ← switch back to radians mode (default)

When `DEG` is active, `SIN`, `COS`, and `TAN` accept angles in degrees. The
mode resets to radians when `RUN`, `NEW`, or `CLR` is executed.

    DEG
    PRINT SIN(90)    ← prints 1
    PRINT COS(180)   ← prints -1
    RAD

### LOG(n) — Common (Base-10) Logarithm

    LOG(100)    → 2

### LN(n) — Natural Logarithm

    LN(2.71828) → ~1

### RND(min, max) — Random Integer

    RND(1, 6)   → random integer 1–6 inclusive

Both bounds are inclusive. `RND(0, 1)` returns 0 or 1. If `min` is greater than
`max` the two are swapped automatically rather than raising an error, so
`RND(6, 1)` behaves the same as `RND(1, 6)`.

### LEN(s$) — String Length

    LEN("HELLO")  → 5

### ASC(s$) — ASCII Code of First Character

    ASC("A")    → 65
    ASC("")     → 0

### VAL(s$) — String to Number

    VAL("3.14")  → 3.14

Gives `?TYPE MISMATCH ERROR` if the string is not a valid number.

### DEC(s$) — Hex or Binary String to Integer

    DEC("$FF")    → 255       ← hex (prefix $)
    DEC("b1010")  → 10        ← binary (prefix b or B)

### INSTR(s1$, s2$[, start]) — Find Substring

Returns the 1-based position of `s2$` within `s1$`, or **0** if not found.
The optional `start` parameter (1-based) sets where to begin searching.

    INSTR("HELLO","LL")      → 3
    INSTR("ABCABC","B",3)    → 5

### FRE(0) — Free Memory

    FRE(0)     → bytes of heap remaining

### PRESSED(col, row) — Physical Key State

    PRESSED(col, row)  → 1 if key at matrix position (col, row) is pressed

Reads the live hardware state of a specific key in the 4×13 keyboard matrix
(columns 0–3, rows 0–12). Returns 1 if the key is currently held down, 0
otherwise. Program mode only.

    10 IF PRESSED(0, 0) THEN PRINT "KEY AT 0,0 IS DOWN"

### KEYDOWN(code) — Key Held Now

    KEYDOWN(1)   → 1 if Up arrow is currently pressed

Reads the live keyboard state by scanning the keyboard matrix for a key that
produces character code `code`. Arrow codes: UP=1, RIGHT=2, DOWN=3, LEFT=4.

**Note:** Every call to `KEYDOWN` clears the keyboard buffer. Do not mix
`KEYDOWN` and `GET` in the same loop — buffered keystrokes will be lost.

### JOY(n) — Joystick State

    JOY(1)   → 1 if the stick is pushed up, 0 if not
    JOY(0)   → all five buttons at once, as a decimal bit pattern

Reads the debounced state of the joystick port. Button numbers are:

    1 = up      2 = down      3 = left      4 = right      5 = trigger

`JOY(0)` returns the whole port as one number, with bit 0 = up through
bit 4 = trigger, so several directions can be tested in a single read:

    value   1    2     4     8      16
    button  up   down  left  right  trigger

A diagonal shows up as the sum of its parts — up+left reads as 1+4 = 5.
Use `AND` to pick out one bit, or compare the whole value:

    10 J = JOY(0)
    20 IF J AND 1 THEN PRINT "UP"
    30 IF J = 5 THEN PRINT "UP AND LEFT"

The port is sampled and debounced by the same 1 kHz interrupt that scans
the keyboard, so a button must be held for about 3 ms before it registers
(and released for about 3 ms before it clears) — contact bounce never
reaches your program. Unlike `KEYDOWN`, `JOY` does not touch the keyboard
buffer, so it is safe to mix with `GET` in the same loop.

`JOY` reports what the stick is doing *at the moment you call it*; it does
not queue presses. A game loop should call it every frame.

An `n` outside 0–5 raises `?ILLEGAL QUANTITY ERROR`.

    10 REM move a character around the screen with the stick
    20 X = 20 : Y = 12
    30 CLS
    40 PLOTCHAR X, Y, 32
    50 IF JOY(1) AND Y > 0 THEN Y = Y - 1
    60 IF JOY(2) AND Y < 24 THEN Y = Y + 1
    70 IF JOY(3) AND X > 0 THEN X = X - 1
    80 IF JOY(4) AND X < 39 THEN X = X + 1
    90 PLOTCHAR X, Y, 81
    100 IF JOY(5) THEN PRINT AT(0,0); "FIRE!"
    110 WAITMS 33
    120 GOTO 40

### CHARAT(col, row) / GETCHAR(col, row) — Screen Character

    CHARAT(10, 5)   → character code at column 10, row 5

Reads the character code at the given screen position. `GETCHAR` is an alias.

### ATTRIBAT(col, row) — Cell Attribute

    ATTRIBAT(10, 5)  → 0 if normal, 1 if reversed

Reads the text attribute of the character cell at (col, row). Corresponds to
the value set by `SETATTRIB`.

### CHECKBLOCK(x, y, w, h) — Is a Rectangular Region Empty?

    CHECKBLOCK(x, y, w, h)
       → 0 if every cell in the w×h rectangle at (x, y) holds a space
         (32) or a NUL (0)
       → 1 if any other character is present

Companion to `FILLBLOCK` for collision/free-space checks: scan a
rectangular region of the text screen and report whether anything is
already drawn there. Useful for placing sprites or menus into known-
empty space without overdrawing existing content.

The rectangle is clipped to the visible screen — cells outside
0..39 horizontally or 0..24 vertically are ignored (treated as
empty). Negative starting coordinates are clamped to 0; a zero or
negative width or height yields 0 (an empty rectangle is trivially
empty).

    10 IF CHECKBLOCK(10, 5, 8, 3) = 0 THEN PLOTCHAR 10, 5, 65

### MILLIS() — Milliseconds Since Boot

    T = MILLIS()

Returns elapsed milliseconds as a floating-point number. Useful for timing.

### TIME(sel) — Read the Real-Time Clock (Time)

    TIME(0)    → current hour (0–23)
    TIME(1)    → current minute (0–59)
    TIME(2)    → current second (0–59)

Reads the hardware real-time clock. The `sel` argument selects which field to
return.

    10 PRINT "IT IS "; TIME(0); ":"; TIME(1)

### DATE(sel) — Read the Real-Time Clock (Date)

    DATE(0)    → current month (1–12)
    DATE(1)    → current day (1–31)
    DATE(2)    → current year (e.g. 2026)

Reads the hardware real-time clock. The `sel` argument selects which field to
return.

    10 PRINT DATE(0); "/"; DATE(1); "/"; DATE(2)

### POINT(x, y) — Pixel State

    POINT(40, 25)   → 0 if off, 1 if solid on, 2 if dithered on

Reads the pixel-graphics layer at the given pixel coordinates (0–79 x, 0–49 y).
Returns the same values accepted by `PPLOT`.

### CURX() / CURY() — Cursor Position

    COL = CURX()    ← current cursor column (0–39)
    ROW = CURY()    ← current cursor row (0–24)

### RESULT(n) — Subroutine Return Value

    val = RESULT(1)     ← first value from most recent RETURN
    val = RESULT(2)     ← second value

Returns the nth numeric value passed by the most recently executed `RETURN`
statement. `n` must be 1 or 2. Gives `?RESULT WITHOUT RETURN ERROR` if no
`RETURN` with values has been executed, or if `n` exceeds the number of
values returned.

### SIZEARR(name, dim) — Array Dimension Size

    n = SIZEARR(A%, 1)    ← number of elements in dimension 1
    n = SIZEARR(M%, 2)    ← number of elements in dimension 2 (2-D arrays)

Returns the declared size of the specified dimension of array `name`. `dim`
must be 1 or 2. Gives `?ARRAY NOT DIM'D ERROR` if the array has not been
dimensioned or if `dim` is out of range.

### NETCONNECTED() — Network Connection Status

    IF NETCONNECTED() THEN PRINT "ONLINE"

Returns 1 if a TCP session is currently active (opened with `NETCONNECT`),
0 otherwise.

---

## STRING FUNCTIONS

String functions return strings and are used in string expressions or with `PRINT`.

### MID$(s$, start[, len]) — Substring

    MID$("HELLO", 2, 3)   → "ELL"

`start` is 1-based. If `len` is omitted, returns from `start` to the end.

### LEFT$(s$, n) — Left n Characters

    LEFT$("HELLO", 3)    → "HEL"

### RIGHT$(s$, n) — Right n Characters

    RIGHT$("HELLO", 3)   → "LLO"

### CHR$(n) — Character from Code

    CHR$(65)   → "A"
    CHR$(13)   → carriage return

Use to produce special or graphic characters by number.

### STR$(n) — Number to String

    STR$(42)    → "42"
    STR$(3.14)  → "3.14"

### HEX$(n) — Number to Hexadecimal String

    HEX$(255)   → "FF"
    HEX$(4096)  → "1000"

Result is 2 digits for values 0–255, 4 digits for larger values.
Range: 0–65535.

### BIN$(n) — Number to Binary String

    BIN$(10)    → "1010"

No leading zeros (except `BIN$(0)` → `"0"`). Range: 0–65535.

### TOUPPER$(s$) — Convert to Uppercase

    TOUPPER$("hello")   → "HELLO"

### TOLOWER$(s$) — Convert to Lowercase

    TOLOWER$("HELLO")   → "hello"

### CHOMP$(s$) — Trim Whitespace

    CHOMP$("  HELLO  ")  → "HELLO"

Removes leading and trailing spaces, tabs, and newline characters.

### DATE$(sel) — Formatted Date String

    DATE$(0)   → "MM/DD/YY"   (US format)
    DATE$(1)   → "DD/MM/YY"   (European format)

Returns the current date from the real-time clock as a formatted string.

    10 PRINT "TODAY IS "; DATE$(0)

### TIME$(sel) — Formatted Time String

    TIME$(0)   → "HH:MM:SS AM"   (12-hour format)
    TIME$(1)   → "HH:MM:SS"      (24-hour format)

Returns the current time from the real-time clock as a formatted string.

    10 PRINT "THE TIME IS "; TIME$(1)

### ERR$(n) — Error Information (inside a TRAP handler)

    S$ = ERR$(0)    ← error message text (e.g. "?FOPEN ERROR")
    S$ = ERR$(1)    ← line number where the error occurred
    S$ = ERR$(2)    ← numeric error code as a string (use VAL() to compare)

Returns information about the most recently trapped error. Only meaningful
when called from within a `TRAP` handler.

`ERR$(0)` returns the message text **exactly** as it would have been printed,
including the trailing word `ERROR` where present. Compare against the full
string — `"?TYPE MISMATCH ERROR"`, not `"?TYPE MISMATCH"`. See the
[ERROR MESSAGES](#error-messages) table for the exact wording of each message.
Comparing against a numeric code with `VAL(ERR$(2))` avoids the issue entirely.

    9000 PRINT "Caught: "; ERR$(0); " at line "; ERR$(1)
    9010 RESUME NEXT

### WIFI$(n) — WiFi Network Info

    S$ = WIFI$(0)    ← SSID of connected network
    S$ = WIFI$(1)    ← IP address
    S$ = WIFI$(2)    ← "OK" if 8.8.8.8 is reachable, "" otherwise

Queries the ESP-01 WiFi module for network information. Returns `""` if not
connected. Only meaningful when **not** in a server TCP session.

### String Concatenation

Use `+` to join strings:

    "HELLO" + " " + "WORLD"   → "HELLO WORLD"
    A$ + CHR$(13)

---

## TIMING AND DELAYS

### SLEEP ms — Wait

    SLEEP 1000      ← pause 1000 milliseconds (1 second)

Waits for the specified number of milliseconds. BREAK (Shift+AC) interrupts
the wait. `TIMER` subroutines fire during `SLEEP`. `WAITMS` is an alias for
`SLEEP`.

### WAITMS ms — Wait (Alias)

    WAITMS 500

Same as `SLEEP`.

### TIMER — Periodic Subroutine

    TIMER interval_ms, linenum     ← arm the timer
    TIMER OFF                       ← disarm

Arms a periodic interrupt that calls `GOSUB linenum` approximately every
`interval_ms` milliseconds. The GOSUB fires between statements (not mid-
expression) and during `SLEEP`. Program mode only. Cleared by `RUN`/`NEW`/`CLR`.

    10 TIMER 500, 9000
    20 WHILE 1 : SLEEP 100 : WEND
    9000 PRINT "TICK" : RETURN

### TRAP — Arm an Error Handler

    TRAP linenum     ← redirect any runtime error to linenum
    TRAP OFF         ← disarm; errors break the program as normal

When an error occurs during a running program and a trap is armed, execution
jumps to `linenum` instead of stopping. The trap handler can inspect the error
with `ERR$()`, then use `RESUME` to continue. Cleared by `RUN`/`NEW`/`CLR`.

**Re-entry protection**: while the handler is executing, the trap is
suspended — any further error breaks the program normally. Trapping is
re-armed automatically when `RESUME` is executed.

    10 TRAP 9000
    20 FOPEN 0, "data.txt", "R"
    30 FINPUT 0, LINE$ : PRINT LINE$ : GOTO 30
    40 END
    9000 IF ERR$(0) = "?FOPEN ERROR" THEN PRINT "FILE NOT FOUND"
    9010 TRAP OFF : END

### RESUME — Return from an Error Handler

    RESUME            ← retry the line that caused the error
    RESUME NEXT       ← skip to the line after the offending line
    RESUME linenum    ← continue at linenum (breaks if line not found)

`RESUME` re-arms the trap so future errors are caught again.

`RESUME linenum` breaks the program with `?UNDEF'D STATEMENT ERROR` if
`linenum` does not exist — a hardcoded bad resume target is a programming
error.

    9000 PRINT "ERROR: "; ERR$(0); " IN LINE "; ERR$(1)
    9010 RESUME NEXT

---

## DATE AND TIME

The Daisy has a built-in real-time clock (RTC) backed by the SAM3X crystal.
Use the numeric functions `TIME(sel)` and `DATE(sel)` to read individual fields,
or the string functions `TIME$(sel)` and `DATE$(sel)` for formatted output.

### SETTIME — Set the Clock

    SETTIME hours, minutes, seconds

Sets the real-time clock's time. Hours use 24-hour format (0–23).

    SETTIME 14, 30, 0          ← set clock to 2:30:00 PM

### SETDATE — Set the Date

    SETDATE day, month, year

Sets the real-time clock's date. Year must be 2000–2099.

    SETDATE 25, 3, 2026        ← March 25, 2026

---

## DISPLAY AND GRAPHICS

### REVERSE / NORMAL — Text Attributes

    REVERSE         ← subsequent text is printed in reverse video
    NORMAL          ← return to normal video
    REVERSE SCREEN  ← invert the entire screen display
    NORMAL SCREEN   ← restore normal screen display

### CHARMODE — Switch Character ROM per Row

    CHARMODE CHAR, firstrow [, lastrow]
    CHARMODE GFX,  firstrow [, lastrow]

Selects which character set (standard CHAR or user-defined GFX) is active for
the specified screen row range.

### DEFCHAR — Redefine a Character Bitmap

    DEFCHAR n, b0, b1, b2, b3, b4, b5, b6, b7

Redefines character `n` (0–255) with eight 8-bit bitmap rows. Each byte gives
one row of 8 pixels; bit 7 is the leftmost pixel.

    100 DEFCHAR 65, 24,60,102,126,102,102,102,0

Rows may be omitted (unchanged) or passed as computed expressions.

### RESETCHAR — Restore a Character to Default

    RESETCHAR n     ← restore character n
    RESETCHAR       ← restore all 256 characters

### DEFGFX / RESETGFX — Redefine GFX Layer Characters

Same syntax as `DEFCHAR`/`RESETCHAR`, but modifies the GFX character set used
when `CHARMODE GFX` is active.

### COPYCHAR — Copy Character Bitmaps

    COPYCHAR src, dst, start            ← copy char 'start' in src to 'start' in dst
    COPYCHAR src, dst, start, end       ← copy chars start..end from src to dst

`src` and `dst` are the keywords `CHAR` or `GFX`.

- Same RAM: copy one character from index `start` to index `end`.
- Different RAMs: copy character range `start`..`end` at the same indices.

    COPYCHAR CHAR, GFX, 64, 127       ← copy chars 64–127 to GFX RAM
    COPYCHAR GFX, GFX, 200, 210       ← copy char 200 to char 210 in GFX

### LOADC / SAVEC — Load/Save Character Sets via Network

    LOADC filename [, CHAR|GFX [, startIndex]]
    SAVEC filename [, CHAR|GFX [, startIndex]]

Transfer raw 8-byte-per-character bitmap data over the network connection.
Default RAM is `CHAR`; default start index is 0.

### LINE — Draw a Character Line

    LINE x1, y1, x2, y2, charcode

Draws a straight line from (x1,y1) to (x2,y2) using the Bresenham algorithm,
filling each cell with the given character code. Coordinates are character
cells (col 0–39, row 0–24).

    10 LINE 0, 0, 39, 24, 42     ← diagonal of asterisks

### PLOTCHAR — Plot a Single Character

    PLOTCHAR col, row, charcode

Places `charcode` at the given character cell. Faster than `PRINT AT` because
it does not move the cursor. **The attribute (normal/reverse) of the cell is
left unchanged** — only the character is replaced.

### FILLCELLS — Fill a Run of Cells

    FILLCELLS col, row, charcode, count

Fills `count` consecutive character cells starting at (col, row), wrapping to
subsequent rows as needed.

### HLINE — Horizontal Character Line

    HLINE x1, x2, y, charcode

Fills all cells from column `x1` to column `x2` on row `y` with `charcode`.
Automatically swaps x1 and x2 if needed; clips to screen edges.

### VLINE — Vertical Character Line

    VLINE x, y1, y2, charcode

Fills all cells in column `x` from row `y1` to row `y2` with `charcode`.

### BOX — Draw a Text Box

    BOX x1, y1, x2, y2, p

Draws a rectangular border of box-drawing characters with corners at (x1, y1)
and (x2, y2). `p=1` draws the box; `p=0` erases it (fills with spaces).
Coordinates are character cells (0–39 x, 0–24 y); corners are automatically
swapped and clipped if needed.

The box uses these built-in characters:

| Position   | Code |
|------------|------|
| Top-left   | 22   |
| Top-right  | 28   |
| Bottom-left| 19   |
| Bottom-right| 25  |
| Horizontal | 26   |
| Vertical   | 21   |

    BOX 2, 2, 37, 22, 1         ← draw a box almost full-screen
    BOX 2, 2, 37, 22, 0         ← erase it

### SETATTRIB — Set Cell Attribute

    SETATTRIB col, row, z

Sets the text attribute of the character cell at (col, row). `z=0` sets the
cell to normal video; `z=1` sets it to reverse video. Does not change the
character code in the cell.

    SETATTRIB 10, 5, 1      ← highlight cell (10,5) in reverse video
    SETATTRIB 10, 5, 0      ← restore it to normal

### SCROLL — Enable/Disable Auto-Scroll

    SCROLL ON          ← disable auto-scroll (screen stops scrolling when full)
    SCROLL OFF         ← enable auto-scroll (default: new lines scroll old ones up)
    SCROLL 1           ← numeric: non-zero = on, 0 = off

`SCROLL` does not scroll the screen — it controls whether automatic scrolling
occurs. With `SCROLL ON` the cursor stays on the last row when output reaches
the bottom; no lines scroll off the top. With `SCROLL OFF` (the default) text
scrolls up normally as each new line is added.

### SCROLLX — Rotate a Band of Rows Horizontally

    SCROLLX y, height, direction

Horizontally rotates `height` rows starting at row `y` by `direction` columns.
Positive = right; negative = left. Characters that scroll off one edge re-appear
on the other edge (wrap-around, not scroll).

### MOVEBLOCK — Copy a Rectangular Block

    MOVEBLOCK x1, y1, x2, y2, w, h [, fillchar]

Copies the `w`×`h` rectangle of character cells whose top-left corner is at
(x1, y1) to position (x2, y2). Handles overlapping regions correctly.
If `fillchar` is given, the source region is filled with that character after
the copy.

    MOVEBLOCK 0,0, 10,5, 20,10     ← copy 20×10 block

### FILLBLOCK — Fill a Rectangle of Cells

    FILLBLOCK x, y, w, h, charcode                    ← fill entirely
    FILLBLOCK x, y, w, h, startchar, endchar           ← cycle chars
    FILLBLOCK x, y, w, h, startchar,                   ← sequential fill

Three modes:
- **Single char**: fills `w×h` cells with `charcode`.
- **Range** (startchar, endchar): cycles through `startchar..endchar`
  repeatedly across the cells.
- **Sequential** (trailing comma, no endchar): fills cells with
  `startchar`, `startchar+1`, … for each of the `w×h` cells.

---

## PIXEL GRAPHICS

The pixel-graphics layer uses a **80×50 pixel** space (four pixels per character
cell, arranged in a 2×2 grid). Pixel coordinates are 0–79 (x) and 0–49 (y).

### Pixel Values

All pixel commands accept a `p` argument:

| Value | Meaning                     |
|-------|-----------------------------|
| `0`   | Off (erase)                 |
| `1`   | Solid on                    |
| `2`   | Dithered (checkerboard on)  |

Dithered pixels look 50% grey. A single character cell can contain any
combination of solid-on and dithered-on pixels independently.

### Block-Graphic Encoding

Pixel cells are stored as characters using a 2×2 bit-pattern scheme:

| Range    | Encoding                                     |
|----------|----------------------------------------------|
| 0–15     | All pixels solid; bits 0–3 = TL,TR,BL,BR    |
| 160–175  | All pixels dithered; same bit layout         |
| 176–225  | Mixed: some pixels solid, some dithered      |

Bit 0 = top-left, bit 1 = top-right, bit 2 = bottom-left, bit 3 = bottom-right.
Characters 128+ are safe to use for `DEFCHAR` without conflicting with pixel cells.

### PPLOT — Set a Single Pixel

    PPLOT x, y, p

Sets pixel at (x, y) to value `p` (0=off, 1=solid, 2=dithered). The other
pixels in the same character cell are unaffected.

### PLINE — Draw a Pixel Line or Rectangle

    PLINE x0, y0, x1, y1, p
    PLINE x0, y0, x1, y1, p, B       ← unfilled rectangle
    PLINE x0, y0, x1, y1, p, BF      ← filled rectangle

Without suffix: draws a Bresenham line from (x0,y0) to (x1,y1).
`,B`: draws an unfilled rectangle with corners at the two points.
`,BF`: draws a filled rectangle.
`p` may be 0, 1, or 2.

### PCIRCLE — Draw an Ellipse or Arc

    PCIRCLE x, y, xr, yr, p [, startdeg [, enddeg]] [, F]

Draws an ellipse centered at pixel (x, y) with horizontal radius `xr` and
vertical radius `yr`. `p` may be 0, 1, or 2.

Optional `startdeg` and `enddeg` (0–360) draw an arc instead of a full
ellipse. 0° points right; angles increase clockwise.

Append `,F` at any point after the first five arguments to flood-fill the
interior of the ellipse after drawing it.

    PCIRCLE 40, 25, 20, 15, 1            ← full ellipse
    PCIRCLE 40, 25, 15, 15, 1, 0, 180   ← top half of circle
    PCIRCLE 40, 25, 20, 15, 2            ← dithered ellipse
    PCIRCLE 40, 25, 20, 15, 1, F        ← filled ellipse
    PCIRCLE 40, 25, 15, 15, 1, 0, 180, F ← filled half-circle

### PFILL — Flood Fill

    PFILL x, y, p

4-connectivity flood fill starting at pixel (x, y). Fills the connected region
that does not match `p` with value `p`. `p` may be 0, 1, or 2.

    PLINE 10,10, 30,10, 1      ← draw a closed shape first
    PLINE 10,10, 10,25, 1
    PLINE 10,25, 30,25, 1
    PLINE 30,10, 30,25, 1
    PFILL 20, 17, 1             ← fill the interior
    PFILL 20, 17, 2             ← fill with dithered pattern instead

### PPOLY — Draw Connected Pixel Polygons

    PPOLY draw%                ← draw polygon with color 1
    PPOLY erase%, draw%        ← erase first polygon, then draw second

Draws connected lines between all points in a 2-D integer array, closing the
shape (last point back to first). Each array must be dimensioned as
`DIM arr%(n, 2)` where column 0 is the pixel X coordinate (0–79) and column 1
is the pixel Y coordinate (0–49).

With two arguments, the first polygon is erased (color 0) and the second is
drawn (color 1) in a single video message. This makes flicker-free animation
possible without double buffering — both the erase and draw happen before the
next screen refresh.

    10 DIM SH%(4, 2)
    20 SH%(0,0)=20 : SH%(0,1)=10
    30 SH%(1,0)=30 : SH%(1,1)=10
    40 SH%(2,0)=30 : SH%(2,1)=20
    50 SH%(3,0)=20 : SH%(3,1)=20
    60 PPOLY SH%                     ← draws a square

    ← Animation: erase old, draw new in one atomic operation
    70 DIM OLD%(4, 2) : DIM NEW%(4, 2)
    80 REM ... copy current to OLD, compute new positions into NEW ...
    90 PPOLY OLD%, NEW%

Maximum 125 total points across both polygons (limited by the 256-byte video
message buffer).

### POINT(x, y) — Read a Pixel

See Numeric Functions section.

---

## AUDIO

The Daisy has three sound voices. Voices 0 and 1 support waveform synthesis.
Voice 2 is a noise generator.

### BEEP — Quick Tone / Atari-style key click

    BEEP             ← single C6 beep on voice 0
    BEEP ON          ← enable per-keystroke click (Atari 400/800-style)
    BEEP OFF         ← disable per-keystroke click

`BEEP` with no argument plays a short C6 beep on voice 0 (the legacy
behaviour). `BEEP ON` enables a global key-click feature: every key
pressed produces a brief 999 Hz pulse, layered across voice 0 (pulse
wave) and voice 2 (noise) for an Atari-like quack. `BEEP OFF` disables
the feature. The setting persists across `RUN` and `NEW`; only
explicit `BEEP OFF` (or rebooting) turns it off.

### SOUND — Play a Tone

    SOUND voice, freq, ms [, 1]

Starts a tone on `voice` (0–2) at frequency `freq` Hz for `ms` milliseconds.
The optional fourth argument `1` makes the statement wait for the tone to
finish before continuing.

    SOUND 0, 440, 500          ← 440 Hz for 0.5 seconds (returns immediately)
    SOUND 0, 440, 500, 1       ← same, but waits

### SHUSH — Stop a Voice

    SHUSH voice     ← stop voice n and its sound program
    SHUSH           ← stop all voices, programs, and reset audio state

### SOUNDPGM — Sound Programs (Music Sequences)

    SOUNDPGM n                     ← start playing program n (0–2)
    SOUNDPGM n, array%, [R|S]      ← load notes array into program n
    SOUNDPGM n, R                  ← set program n to repeat
    SOUNDPGM n, S                  ← set program n to play once

A sound program is a sequence of notes in a 2-D integer array with 2 columns:
column 0 = frequency (Hz), column 1 = duration (ms). Terminate with a row of
(0, 0).

    10 DIM M%(5,2)
    20 M%(0,0)=262 : M%(0,1)=200
    30 M%(1,0)=330 : M%(1,1)=200
    40 M%(2,0)=0   : M%(2,1)=0
    50 SOUNDPGM 0, M%, R           ← load and repeat on voice 0
    60 SOUNDPGM 0                  ← start playing

### SOUNDPWM — Pulse-Width Modulation

    SOUNDPWM voice, pw, lfo_hz

Sets the pulse-width (`pw`, 0–255) and LFO rate (`lfo_hz`, 0.0 = off) for
voice 0 or 1. Affects the pulse waveform type.

### SOUNDPRT — Portamento (Pitch Glide)

    SOUNDPRT voice, ms

Enables pitch glide for voice 0 or 1. `ms` is the glide time in milliseconds
(0 = off). The voice glides smoothly from its previous pitch to each new note.

    SOUNDPRT 0, 200          ← 200 ms glide on voice 0

### PLAY — MML (Music Macro Language)

    PLAY string$

Plays music described by an MML string. The string is interpreted character
by character (case-insensitive):

| Token | Meaning                                      |
|-------|----------------------------------------------|
| `C`–`B` | Note names (C D E F G A B)                |
| `#`/`+` | Sharp (follows note)                      |
| `-`     | Flat (follows note)                       |
| `On`    | Set octave n (1–8; default 4)             |
| `>`     | Octave up                                 |
| `<`     | Octave down                               |
| `Ln`    | Set default note length (1=whole, 4=quarter, 8=eighth…) |
| `Tn`    | Set tempo in BPM (default 120)            |
| `Vn`    | Select voice/program (0–2)                |
| `Rn`/`Pn` | Rest of length n                        |
| `.`     | Dotted note (×1.5 duration, follows note or rest) |

Append an optional length to a note: `C4` = quarter-note C.

    PLAY "T180 L8 CDEFGAB>C"          ← scale in eighth notes at 180 BPM
    PLAY "O4 C4 E4 G4 >C2"            ← C major arpeggio

---

## NETWORK I/O

The network serial port (Serial1, via ESP-01 WiFi module) allows communication
with a host computer running the daisyfile.py file server or a custom companion app.

### WIFI — Connect to a WiFi Network

    WIFI ssid$, password$

Connects to the specified WiFi network via the ESP-01 module (Zimodem firmware).
Call only when **not** already in a server session. Prints `?WIFI CONNECT FAILED`
on error.

    WIFI "MyNetwork", "secret"

### NETCONNECT — Open a TCP Connection

    NETCONNECT host$, port

Dials a TCP server at `host$`:`port` using the Zimodem `ATDT` command and
enters transparent (pass-through) mode. Prints `?NETCONNECT FAILED` on error.
Use `NETPRINT`/`NETGET`/`NETINPUT` to communicate once connected.

    NETCONNECT "192.168.1.10", 8000

### NETDISCONNECT — Close the TCP Connection

    NETDISCONNECT

Exits transparent mode (`+++` guard sequence followed by `ATH`) and hangs up
the TCP connection. Blocks approximately 2 seconds for the guard-time delay.

### NETPRINT — Send Data over Network

    NETPRINT expr [; expr] [, expr] ...

Sends formatted text to the network stream. Separators `;` and `,` both
concatenate without any column spacing. **No newline is appended automatically**;
add `CHR$(13)` or `CHR$(10)` manually if the receiver needs a line terminator.

    NETPRINT "SCORE="; SCORE; CHR$(13)

### NETGET — Receive Raw Bytes

    NETGET var [, var ...] [;]

Reads one byte per variable from the network stream. String variables receive
the character; numeric variables receive the ASCII code.

- Default (no `;`): non-blocking — assigns 0 / "" if no byte is available.
- With trailing `;`: blocking — waits for each byte; BREAK interruptible.

### NETINPUT — Receive a Line (Comma-Delimited)

    NETINPUT var [, var ...] [;]

Reads one newline-terminated line from the network and parses comma-separated
fields into up to 4 variables (numeric or string).

- Default (no `;`): non-blocking — assigns 0 / "" if no complete line.
- With trailing `;`: blocking — waits for a complete line; BREAK interruptible.

    NETINPUT X, Y;                     ← block until "10,20\n" arrives

### LOAD — Load a Program from Network

    LOAD [filename]

Receives a BASIC program over the network from the file server.
If a filename is provided, the server uses it to select which file to send.
May be used inside a running program.

    LOAD "mygame.bas"

### SAVE — Save a Program via Network

    SAVE filename
    SAVE -filename        ← overwrite without confirmation

Sends the current program to the network file server as plain BASIC text.
May be used inside a running program. Prefix `-` to overwrite an existing file.

    SAVE "mygame.bas"

### CAT / CATALOG — Display File Catalog

    CAT
    CATALOG

Requests a directory listing from the network file server and displays it on
screen. `CATALOG` is an alias for `CAT`.

### DEL / DELETE — Delete a File

    DEL filename
    DELETE filename

Asks the file server to delete `filename`. The filename may be quoted or
bare-word. Prints `?FILE NOT FOUND ERROR` if the file does not exist.

    DEL "oldprog.bas"
    DEL oldprog.bas

### REN / RENAME — Rename a File

    REN oldname, newname
    RENAME oldname, newname

Renames `oldname` to `newname` on the file server. Filenames may be quoted
or bare-word.

    REN "prog.bas", "backup.bas"

### COPY — Copy a File

    COPY source, destination

Copies `source` to a new file named `destination` on the file server.
Prints `?FILE NOT FOUND ERROR` if the source does not exist.

    COPY "prog.bas", "prog_bak.bas"

### CHDIR — Change Directory

    CHDIR dirname

Changes the server's current working directory. Subsequent file operations
(LOAD, SAVE, CAT, DEL, REN, COPY, FOPEN, etc.) work relative to the new
directory. Prints `?DIR NOT FOUND ERROR` if the directory does not exist or
would escape the file root.

    CHDIR "games"       ← enter the games/ subdirectory
    CHDIR ".."          ← go up one level
    CHDIR "/"           ← return to the root directory

**Note:** The current directory is per-connection and resets to `/` when a
new connection is established.

### MKDIR — Create a Directory

    MKDIR dirname

Creates a new directory relative to the current directory on the server.
Intermediate directories are created automatically. Prints `?DIR EXISTS ERROR`
if the directory already exists.

    MKDIR "games"
    MKDIR "games/saves"    ← creates both games/ and games/saves/ if needed

---

## SEQUENTIAL FILE I/O

DaisyBASIC can read and write files on the host computer through the
`daisyfile.py` file server. Files are accessed via numbered channels (0–3).
All operations are acknowledged; a failed operation prints an error message.

### FOPEN — Open a File

    FOPEN channel, filename, mode

Opens `filename` on the host and assigns it to `channel`. `mode` is a
string: `"R"` (read), `"W"` (write, truncates), or `"A"` (append).

    FOPEN 0, "data.txt", "W"

### FCLOSE — Close a File

    FCLOSE channel

Flushes and closes the file on the given channel.

**Note:** any channels still open are auto-closed by `RUN`, `NEW`, `CLR`,
and `BREAK` so a program that errors out mid-stream doesn't leak file
handles on the server. A program that ends cleanly with `END` (or by
running off the end) leaves its channels open so you can continue to
`FINPUT`/`FGET` from the prompt; the next `RUN` then closes them.

### FPRINT — Write a Line to a File

    FPRINT channel, expr [; expr ...] [, expr ...]

Formats and writes data to the file. Separators `;` and `,` both
concatenate with no spacing. No newline is appended automatically; add
`CHR$(10)` manually when needed.

    FPRINT 0, "NAME="; NAME$; CHR$(10)

### FINPUT — Read a Line from a File

    FINPUT channel, var [, var ...]

Reads one complete newline-terminated line per variable. Commas inside the
file are treated as literal data, not field separators. Assigns 0 / "" on
EOF.

    FINPUT 0, LINE$

### FGET — Read a Single Byte

    FGET channel, var

Reads one byte from the file. String variables receive the character;
numeric variables receive its ASCII code. Assigns 0 / "" on EOF.

### FPUT — Write a Single Byte

    FPUT channel, expr

Writes one byte (the integer value of `expr`) to the file.

### FSEEK — Move the File Cursor

    FSEEK channel, delta

Shifts the file cursor by `delta` bytes relative to its current position.
Use a positive value to move forward, negative to move backward.

    FSEEK 0, -1        ← back up one byte
    FSEEK 0, 10        ← skip forward 10 bytes

### FREWIND — Seek to Start of File

    FREWIND channel

Repositions the file cursor of `channel` (0–3) back to byte 0. Raises
`?FSEEK ERROR` if the server rejects the seek or the channel is not open.

    10 FOPEN 0, "log.txt", "R"
    20 FINPUT 0, A$           ← read first line
    30 FREWIND 0              ← rewind to start
    40 FINPUT 0, A$           ← read first line again
    50 FCLOSE 0

### FBYTES — Bytes Remaining

    n = FBYTES(channel)

Returns the number of bytes remaining from the current cursor position to
the end of the file.

**Example — write then read back a file:**

    10 FOPEN 0, "test.txt", "W"
    20 FPRINT 0, "HELLO"; CHR$(10)
    30 FCLOSE 0
    40 FOPEN 0, "test.txt", "R"
    50 FINPUT 0, A$
    60 FCLOSE 0
    70 PRINT A$

---

## SYSTEM COMMANDS

The following commands work only in immediate mode (not inside a running program).

### LIST — Display the Program

    LIST            ← list entire program
    LIST 10         ← list line 10 only
    LIST 10-50      ← list lines 10 through 50
    LIST 10-        ← list from line 10 to the end

Press BREAK (Shift+AC) to abort a long listing. Press Scroll Lock to pause.

### RUN — Execute the Program

    RUN             ← run from the first line
    RUN 100         ← run from line 100

Clears all variables before starting.

### CONT — Continue After Break

    CONT

Resumes execution from where it stopped after a BREAK or `END`.

### NEW — Erase Program and Variables

    NEW

Deletes the entire program and all variables and arrays.

### CLR — Clear Variables

    CLR

Frees all variables, arrays, FOR/GOSUB/WHILE stacks, and user functions.
The program text is kept.

### VERSION — Display Build Information

    VERSION

Prints the firmware build date and time.

### CHUNK — Split a String into an Array

    CHUNK arr$, string$ [, delimiter$]

Splits `string$` at each occurrence of `delimiter$` (default: a single space)
and places the pieces into successive elements of `arr$`, which must be a
pre-dimensioned string array.

    DIM W$(5)
    CHUNK W$, "ONE TWO THREE"           ← W$(0)="ONE", W$(1)="TWO", W$(2)="THREE"
    CHUNK W$, "A,B,C", ","             ← split on comma

Useful with `CHOMP$` when fields may have surrounding whitespace.

### RENUMBER — Renumber Program Lines

    RENUMBER start [, step]

Renumbers all program lines starting from `start` with an increment of `step`
(default 5). All `GOTO`, `GOSUB`, `THEN`, `ON`, `RESTORE`, and `TIMER` line
references are updated automatically. Immediate mode only.

Prints `?RENUMBERING OVERFLOW` if any new line number would exceed 65535.
`start` must be in the range 1–65535 or you get `?ILLEGAL LINE NUMBER`, and
`step` must be at least 1 or you get `?ILLEGAL QUANTITY ERROR`.

    RENUMBER 100        ← renumber from 100, step 5
    RENUMBER 10, 10     ← renumber from 10, step 10

### MORE — Page Through a File

    MORE filename

Opens `filename` on the file server and displays it one screenful at a time.
The filename may be quoted or given as a bare word. Unlike the other system
commands, `MORE` also works inside a running program.

While paging:

| Key             | Action                                    |
|-----------------|-------------------------------------------|
| Space           | Next page                                 |
| Backspace       | Previous page                             |
| `:`             | Search — opens a `/` prompt on the status bar |
| BREAK / Ctrl-C  | Quit                                      |

At the `/` prompt, type text and press Enter to search forward from the
current page; the search wraps to the start of the file if nothing is found
ahead. Pressing Enter on an empty prompt repeats the previous search. A
status bar shows the current page, the page count, and search results.

    MORE "readme.txt"
    MORE readme.txt

### DEMO — Run the Built-in Demo

    DEMO

Clears the current program with `NEW`, loads the built-in demonstration
program, and runs it. **This replaces whatever program is currently in
memory** — save your work first.

### SHAPEDEMO — Run the Built-in Shape Demo

    SHAPEDEMO

Clears the current program with `NEW`, loads a built-in pixel-graphics
demonstration, and runs it. As with `DEMO`, **this replaces whatever program
is currently in memory** — save your work first.

### REBOOT — Restart the Computer

    REBOOT

Performs a hardware reset. All program and variable data is lost.

### TERM — VT52 Terminal Emulator

    TERM

Launches the built-in VT52 terminal emulator. The terminal communicates over
the serial/Wifi connection and supports:

- VT52 escape sequences (cursor movement, clear screen, graphics mode)
- ANSI arrow key sequences
- Configurable local echo, return key mode, bell mode, and screen polarity
- Status bar with date/time display

Press **Ctrl-Z** to open the configuration menu. Select **EXIT TO BASIC** from
the menu to return to the BASIC prompt.

### INVADERS — Built-in Game

    INVADERS

Starts the built-in Space Invaders arcade game. Press BREAK (Shift+AC) to exit.

---

## SPECIAL NOTES AND TIPS

### Line Editing

- **Backspace** deletes the last character typed.
- **Enter** submits the line.
- **Ctrl-C** cancels the current input line.
- **BREAK (Shift+AC)** interrupts a running program; use `CONT` to resume.
- **Scroll Lock** pauses a running program (press again to resume).

### Immediate Mode Evaluation

Any valid statement typed without a line number executes immediately:

    PRINT 2 + 2              ← prints 4
    X = 100 : PRINT X        ← assigns then prints

### Multi-Line Programs

Type each line with a line number to build your program. When done, type `RUN`.

    10 FOR I=1 TO 5
    20 PRINT I*I
    30 NEXT
    RUN

### Deleting a Line

Type just the line number and press Enter:

    20              ← deletes line 20

### Colon-Separated Statements

Multiple statements fit on one line, separated by colons. `REM` must be the
last statement on the line (it consumes everything after it).

### Variables Are Case-Insensitive

`SCORE`, `Score`, and `score` all refer to the same variable.

### GOTO Out of FOR Loops

Jumping out of a `FOR` loop with `GOTO` is safe (Commodore BASIC behaviour).
The stack entry is replaced when the loop variable re-enters a `FOR`.

### RESTORE with a Line Number

    RESTORE 1000

Moves the `READ` pointer to the first `DATA` statement at or after line 1000,
useful for targeting specific data blocks.

---

## SPECIAL CHARACTERS

### Block-Graphic Pixel Characters

The pixel-graphics commands use character codes 0–225 to represent 2×2 pixel
cells. **Do not use these ranges for text or `DEFCHAR` unless you know what
you are doing**, as they are maintained by the pixel commands.

| Range   | Contents                                          |
|---------|---------------------------------------------------|
| 0–15    | Solid pixels; bit n = pixel state (TL/TR/BL/BR)  |
| 160–175 | Dithered pixels; same bit layout                 |
| 176–225 | Mixed cells: some pixels solid, some dithered     |

### Box-Drawing Characters

Used by the `BOX` command:

| Code | Glyph        |
|------|--------------|
| 19   | Bottom-left  |
| 21   | Vertical bar |
| 22   | Top-left     |
| 25   | Bottom-right |
| 26   | Horizontal   |
| 28   | Top-right    |

### User-Definable Range

Characters **128–159** and **226–255** are safe for `DEFCHAR`/`DEFGFX` without
conflicting with pixel cells or box-drawing characters.

### Other Notable Characters

| Code | Description       |
|------|-------------------|
| 32   | Space             |
| 42   | Asterisk `*`      |
| 65–90| A–Z               |
| 97–122| a–z              |
| 244  | Die face — 1 pip  |
| 245  | Die face — 2 pips |
| 246  | Die face — 3 pips |
| 247  | Die face — 4 pips |
| 248  | Die face — 5 pips |
| 249  | Die face — 6 pips |

---

## ERROR MESSAGES

Messages are listed exactly as they are printed. Note that most end in the word
`ERROR` — this matters when comparing against `ERR$(0)` inside a `TRAP` handler,
which returns the full text.

### Interpreter Errors

| Error                                | Meaning                                          |
|--------------------------------------|--------------------------------------------------|
| `?SYNTAX ERROR`                      | Unrecognised statement or bad syntax             |
| `?TYPE MISMATCH ERROR`               | String/numeric type error                        |
| `?ILLEGAL QUANTITY ERROR`            | Numeric value out of the legal range             |
| `?ILLEGAL DIRECT ERROR`              | Program-only command used in immediate mode      |
| `?ILLEGAL LINE NUMBER`               | Line number not in range 1–65535                 |
| `?UNDEF'D STATEMENT ERROR`           | GOTO/GOSUB to a non-existent line number         |
| `?LINE TOO LONG ERROR`               | Entered line exceeds the input buffer            |
| `?FORMULA TOO COMPLEX ERROR`         | Condition too long for the evaluator's buffers   |
| `?ARRAY NOT DIM'D ERROR`             | Array used before DIM                            |
| `?BAD SUBSCRIPT ERROR`               | Array index out of range                         |
| `?WRONG NUMBER OF DIMENSIONS ERROR`  | 1-D access on 2-D array or vice-versa            |
| `?DIVISION BY ZERO ERROR`            | Division or modulo by zero                       |
| `?OUT OF DATA ERROR`                 | READ with no more DATA values                    |
| `?NEXT WITHOUT FOR ERROR`            | NEXT with no matching FOR                        |
| `?RETURN WITHOUT GOSUB ERROR`        | RETURN with no GOSUB on the stack                |
| `?WEND WITHOUT WHILE ERROR`          | WEND with no matching WHILE                      |
| `?WHILE WITHOUT WEND ERROR`          | WHILE with no matching WEND found                |
| `?UNTIL WITHOUT DO ERROR`            | UNTIL with no matching DO                        |
| `?DO WITHOUT UNTIL ERROR`            | DO with no matching UNTIL found                  |
| `?EXIT NOT IN LOOP ERROR`            | EXIT used outside a WHILE or DO loop             |
| `?OUT OF MEMORY ERROR`               | Heap exhausted                                   |
| `?RESULT WITHOUT RETURN ERROR`       | RESULT(n) called with no RETURN values available |
| `?RESUME WITHOUT TRAP ERROR`         | RESUME/RESUME NEXT with no trapped error state   |
| `?RENUMBERING OVERFLOW`              | RENUMBER would push a line number past 65535     |
| `?UNKNOWN ERROR`                     | Internal fallback for an unrecognised error code |
| `BREAK IN LINE n`                    | BREAK pressed; use CONT to resume                |

### Network and File Errors Raised by the Daisy

| Error                          | Meaning                                          |
|--------------------------------|--------------------------------------------------|
| `?FOPEN ERROR`                 | File server rejected or could not open the file  |
| `?FCLOSE ERROR`                | File server failed to close the channel          |
| `?FPRINT ERROR`                | File server rejected the write                   |
| `?FPUT ERROR`                  | File server rejected the byte write              |
| `?FSEEK ERROR`                 | File server rejected the seek                    |
| `?WIFI CONNECT FAILED`         | Could not connect to the specified WiFi network  |
| `?NETCONNECT FAILED`           | TCP connection to host/port could not be opened  |
| `?LOAD ERROR`                  | Problem receiving a program via network          |
| `?MISSING FILE NAME ERROR`     | SAVE/LOAD requires a filename                    |
| `?CATALOG ERROR`               | Problem retrieving network file listing (CAT)    |
| `?EMPTY FILE`                  | MORE: the file opened but contains no data       |
| `?OUT OF MEMORY`               | MORE: could not allocate its page-offset table. Note this one has **no** `ERROR` suffix, unlike the interpreter's `?OUT OF MEMORY ERROR` |

### Errors Reported by the File Server

`CHDIR`, `MKDIR`, `DEL`, `REN`, `COPY`, `SAVE`, and `SAVEC` forward the request
to `daisyfile.py` and print whatever the server replies. These messages come
from the **server**, not the Daisy, so their exact wording depends on the
version of `daisyfile.py` you are running:

| Error                     | Meaning                                        |
|---------------------------|------------------------------------------------|
| `?FILE NOT FOUND ERROR`   | The named file does not exist on the server    |
| `?FILE EXISTS ERROR`      | SAVE/SAVEC/REN/COPY: target exists already      |
| `?DIR NOT FOUND ERROR`    | CHDIR target missing, or escapes the file root |
| `?DIR EXISTS ERROR`       | MKDIR target already exists                    |
| `?MKDIR ERROR`            | Directory could not be created                 |
| `?DELETE ERROR`           | File could not be deleted                      |
| `?RENAME ERROR`           | File could not be renamed                      |
| `?COPY ERROR`             | File could not be copied                       |
| `?SAVE ERROR`             | Program could not be written                   |
| `?SAVECHAR ERROR`         | Character set could not be written             |
| `?IO COMMAND ERROR`       | Server did not understand the request          |

---

## QUICK REFERENCE

### Immediate-Mode Commands

    LIST [start[-end]]     DEMO          INVADERS
    RUN [linenum]          NEW           REBOOT
    CONT                   CLR           TERM
    LOAD [file]            SAVE file     CAT / CATALOG
    DEL file               REN old,new   COPY src,dst
    CHDIR dir              MKDIR dir
    LOADC file[,...]       SAVEC file[,...]
    VERSION                CHUNK arr$, str$[, delim$]
    RENUMBER start[,step]  SHAPEDEMO

### Program Statements

    REM ...                  LET var = expr
    PRINT [expr[;|,]...]     INPUT var[,var...][;]
    GET var                  LOCATE col,row        CLS
    END                      GOTO linenum
    IF cond THEN stmt        ON x GOTO/GOSUB lines
    FOR v=s TO e [STEP n]    NEXT [v]
    GOSUB linenum            RETURN [expr[,expr]]
    WHILE cond               WEND
    DO                       UNTIL cond
    EXIT                     DATA ...
    READ var[,var...]        READMAT arr
    RESTORE [linenum]        DIM name(n)[,...]
    CLEARARR name[(idx)][,...] SWAPARR a,b[,c]
    DEF FN name(x) = expr    TIMER ms,line | TIMER OFF
    TRAP linenum | TRAP OFF  RESUME [NEXT | linenum]
    SLEEP ms                 WAITMS ms
    DEG                      RAD
    BEEP [ON|OFF]            SOUND v,freq,ms[,1]
    SHUSH [v]                SOUNDPGM n[,arr%[,R|S]]
    SOUNDPWM v,pw,lfo        SOUNDPRT v,ms
    PLAY mml$
    LINE x1,y1,x2,y2,ch      PLOTCHAR c,r,ch
    FILLCELLS c,r,ch,n        HLINE x1,x2,y,ch
    VLINE x,y1,y2,ch          SCROLL ON|OFF
    SCROLLX y,h,dir           MOVEBLOCK x1,y1,x2,y2,w,h[,ch]
    FILLBLOCK x,y,w,h,start[,end]
    BOX x1,y1,x2,y2,p         SETATTRIB c,r,z
    PPLOT x,y,p               PLINE x0,y0,x1,y1,p[,B|BF]
    PCIRCLE x,y,xr,yr,p[,s[,e]][,F]
    PFILL x,y,p               PPOLY [erase%,] draw%
    DEFCHAR n,b0..b7          RESETCHAR [n]
    DEFGFX n,b0..b7           RESETGFX [n]
    CHARMODE CHAR|GFX,r[,r2]  COPYCHAR src,dst,start[,end]
    REVERSE [SCREEN]          NORMAL [SCREEN]
    WIFI ssid$,pass$          NETCONNECT host$,port
    NETDISCONNECT
    NETPRINT expr[;...]       NETGET var[,...][;]
    NETINPUT var[,...][;]
    LOAD [file]               SAVE [-]file
    CAT                       DEL file
    REN old,new               COPY src,dst
    CHDIR dir                 MKDIR dir
    FOPEN ch,name$,mode$      FCLOSE ch
    FPRINT ch,expr[;...]      FINPUT ch,var[,var...]
    FGET ch,var               FPUT ch,byte
    FSEEK ch,delta            FREWIND ch
    MORE file
    VERSION                   CHUNK arr$,str$[,delim$]
    SETTIME h,m,s             SETDATE d,m,y

### Numeric Functions

    ABS(n)        INT(n)        SQR(n)
    SIN(n)        COS(n)        TAN(n)
    LOG(n)        LN(n)         RND(min,max)
    LEN(s$)       ASC(s$)       VAL(s$)
    DEC("$hh")    INSTR(s1$,s2$[,pos])
    FRE(0)        PRESSED(c,r)  KEYDOWN(k)
    CHARAT(c,r)   GETCHAR(c,r)  ATTRIBAT(c,r)
    MILLIS()      TIME(sel)     DATE(sel)
    POINT(x,y)    CURX()        CURY()
    RESULT(n)     SIZEARR(name,dim)   NETCONNECTED()
    FBYTES(ch)    JOY(n)
    CHECKBLOCK(x,y,w,h)
    FN name(x)

### String Functions

    MID$(s$,start[,len])     LEFT$(s$,n)    RIGHT$(s$,n)
    CHR$(n)    STR$(n)        HEX$(n)        BIN$(n)
    TOUPPER$(s$)              TOLOWER$(s$)   CHOMP$(s$)
    DATE$(sel)                TIME$(sel)
    WIFI$(n)                  ERR$(n)
    Concatenation: s1$ + s2$
