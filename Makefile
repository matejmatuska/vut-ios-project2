CFLAGS= -std=gnu99 -Wall -Wextra -Werror -pedantic -g
LDLIBS= -lrt -lpthread

BIN=proj2

proj2: proj2.o out_writer.o

out_writer.o: out_writer.c out_writer.h

.PHONY: run
run: $(BIN)
	./$(BIN) 5 4 100 100

.PHONY: debug
debug: $(BIN)
	gdb -tui $(BIN)

.PHONY: pack
pack:
	zip proj2.zip *.c *.h Makefile

.PHONY: clean
clean:
	$(RM) *.o $(BIN)
