# Compile libkernel (processor independent code)

LIBNAME := kernel
CSRCS := mm.c \
	tls.c \
	thread.c \
	printk.c \
	dev_text.c \
	dev_storage.c \
	conio.c \
	mouse.c \
	keyboard.c \
	string.c \
	irq.c \
	time.c

# TLS has memory references at address 0
tls.o: CFLAGS += -mcmodel=large

include $(BUILDROOT)/library.mk
