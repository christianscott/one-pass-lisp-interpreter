# One Pass Lisp Interpreter

A single pass "lisp" interpreter. It does everthing (parsing & evaluating) in a single pass.

## "Features"

Arithmetic:

```lisp
(add 3 2 1)
(mult 1 2 3)
(div 10 0)
```

Boolean logic:

```lisp
(eq 1 1)
```

Variables via let:

```lisp
(let
  a 1
  b 2
  (add a b))
```

Printing:

```lisp
(print 10)
```
