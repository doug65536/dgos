
all: grammar.o

clean:


%.o: %.c
	$(CC) -o $@ -c $<

%.c %.h: %.y
	lemon $<

.PHONY: all clean
