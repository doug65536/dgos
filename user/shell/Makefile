
all: grammar.o

%.o: %.c
	$(CC) -o $@ -c $<

%.c %.h: %.y
	lemon $<
