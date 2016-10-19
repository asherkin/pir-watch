CFLAGS ?= -O3 -Wall -Wextra -pedantic

pir-watch: main.c Makefile
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $^ pir-watch

.PHONY: clean
