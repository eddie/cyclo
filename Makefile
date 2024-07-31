# vim: set noexpandtab tabstop=8 shiftwidth=8:
#
CC = c99
CFLAGS = -Wall
LDFLAGS = -lc


all: assembler emulator

assembler: assembler.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

emulator: emulator.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f bin/

format: $(SOURCES)
	clang-format -i -style=file hello.c


.PHONY: all clean
