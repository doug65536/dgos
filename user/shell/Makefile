
all: grammar.o

clean:


%.o: %.c
	$(CC) -o $@ -c $<

%.o: %.cc
	$(CXX) -o $@ -c $<

%.c %.h: %.y
	lemon $<

.PHONY: all clean
