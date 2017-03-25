# Compile libkernel (processor independent code)

LIBNAME := kernel
CSRCS := mm.cc \
	thread.cc \
	priorityqueue.cc \
	threadsync.cc \
	callout.cc \
	printk.cc \
	math.cc \
	dev_text.cc \
	dev_storage.cc \
	dev_eth.cc \
	conio.cc \
	mouse.cc \
	keyboard.cc \
	string.cc \
	irq.cc \
	time.cc \
	stdlib.cc \
	assert.cc \
	bitsearch.cc \
	bsearch.cc \
	rbtree.cc \
	debug.cc \
	rand.cc \
	bswap.cc \
	pool.cc \
	heap.cc \
	hash.cc \
	hash_table.cc \
	fileio.cc \
	process.cc \
	png.cc \
	zlib_helper.cc \
	framebuffer.cc \
	unique_ptr.cc \
	array.cc

framebuffer.o: CXXFLAGS += -ffast-math

include $(BUILDROOT)/library.mk
