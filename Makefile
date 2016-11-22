all:
	(cd boot && $(MAKE) kernel all)

clean:
	(cd boot && $(MAKE) clean)
	(cd kernel && $(MAKE) clean)

