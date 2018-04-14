src=mine.c
bin=mine-git-hash

test=test
test_src=test.c

CFLAGS=-std=c11
LDFLAGS=-lssl -lcrypto

$(bin): $(src)
	gcc $(LDFLAGS) $(CFLAGS) -O3 -o $(bin) $(src)

install: $(bin)
	install $(bin) ${HOME}/.local/bin

format: $(wildcard *.c)
	clang-format -i -style=WebKit *.c

.PHONY: debug
debug:
	gcc $(LDFLAGS) $(CFLAGS) -g -O0 -o $(bin) $(src)

.PHONY: clean
clean:
	rm -f $(bin) $(test)

.PHONY: check
check: $(test)
	./$(test)

$(test): $(test_src)
	gcc $(LDFLAGS) -ltap $(CFLAGS) -o $(test) $(test_src)
