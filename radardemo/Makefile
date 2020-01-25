CFLAGS?=-Wall -g
LDFLAGS?=-static
radardemo:	radardemo.o devtag-allinone.o
install:	radardemo
	strip radardemo
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p radardemo $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o radardemo

