CFLAGS ?= -O3 -Wall -Wextra -pedantic
HIREDIS = `pkg-config --cflags --libs hiredis`

pir-watch: main.c Makefile
	$(CC) $(CFLAGS) $(HIREDIS) $< -o $@

clean:
	rm -rf $^ pir-watch

.PHONY: clean
