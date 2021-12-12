default: build

build:
	@mkdir -p bin
	gcc -o bin/lisp main.c -Wall -Wpedantic -Wextra

run: build
	./bin/lisp

.PHONY: format
format:
	clang-format -i -style=Microsoft main.c
