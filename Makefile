CFLAGS ?= -std=c11 -O3 -Wall -Wextra -pedantic
HIREDIS = `pkg-config --cflags --libs hiredis`

pir-watch: pir-watch.c
	$(CC) $(CFLAGS) $(HIREDIS) $^ -o $@

clean:
	rm -rf pir-watch

.PHONY: clean
