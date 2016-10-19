pir-watch: main.c
	clang $^ -Wall -Wextra -pedantic -o $@

clean:
	rm -rf $^ pir-watch

.PHONY: clean
