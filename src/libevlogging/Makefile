## make file for libevlogging.

all: libevlogging.so.1.0.1

ARGS=-Wall
OBJS=libevlogging.o

libevlogging.o: libevlogging.c evlogging.h
	gcc -c -fPIC libevlogging.c -o $@ $(ARGS)


libevlogging.a: $(OBJS)
	@>$@
	@rm $@
	ar -r $@
	ar -r $@ $^

libevlogging.so.1.0.1: $(OBJS)
	gcc -shared -Wl,-soname,libevlogging.so.1 -o libevlogging.so.1.0.1 $(OBJS)
	

install: libevlogging.so.1.0.1 evlogging.h
	cp evlogging.h /usr/include/
	cp libevlogging.so.1.0.1 /usr/lib/
	@-test -e /usr/lib/libevlogging.so && rm /usr/lib/libevlogging.so
	ln -s /usr/lib/libevlogging.so.1.0.1 /usr/lib/libevlogging.so
	ldconfig
	@echo "Install complete."

clean:
	@-[ -e libevlogging.o ] && rm libevlogging.o
	@-[ -e libevlogging.so* ] && rm libevlogging.so*
