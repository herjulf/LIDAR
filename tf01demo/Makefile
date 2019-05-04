CFLAGS?=-Wall -g
LDFLAGS?=-static
tf01demo:	tf01demo.o devtag-allinone.o
install:	tf01demo
	strip tf01demo
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p tf01demo $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o tf01demo

