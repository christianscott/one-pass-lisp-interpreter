default: build

build:
	@mkdir -p bin
	gcc -o bin/lisp main.c -Wall -Wpedantic -Wextra

run: build
	./bin/lisp
