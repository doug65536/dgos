all:
	(cd boot && $(MAKE) kernel all)

iso:
	(cd boot && $(MAKE) iso)

clean:
	(cd boot && $(MAKE) clean)
	(cd kernel && $(MAKE) clean)

