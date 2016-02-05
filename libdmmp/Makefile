# Makefile
#
# Copyright (C) 2015 - 2016 Red Hat, Inc.
# Gris Ge <fge@redhat.com>
#
include ../Makefile.inc

LIBDMMP_VERSION=0.1.0
SONAME=$(LIBDMMP_VERSION)
DEVLIB = libdmmp.so
LIBS = $(DEVLIB).$(SONAME)
LIBDEPS = -pthread
PKGFILE = libdmmp.pc
EXTRA_MAN_FILES = libdmmp.h.3
HEADERS = libdmmp/libdmmp.h
OBJS = libdmmp.o libdmmp_mp.o libdmmp_pg.o libdmmp_path.o libdmmp_misc.o

CFLAGS += -fvisibility=hidden -I$(libdmmpdir)

all: $(LIBS) doc

$(LIBS): $(OBJS)
	$(CC) $(LDFLAGS) $(SHARED_FLAGS) \
	-Wl,-soname=$@ $(CFLAGS) -o $@ $(OBJS) $(LIBDEPS)
	ln -sf $@ $(DEVLIB)

install:
	$(INSTALL_PROGRAM) -m 755 $(LIBS) $(DESTDIR)$(syslibdir)/$(LIBS)
	$(INSTALL_PROGRAM) -m 644 -D \
		$(HEADERS) $(DESTDIR)/$(includedir)/$(HEADERS)
	ln -sf $(LIBS) $(DESTDIR)/$(syslibdir)/$(DEVLIB)
	$(INSTALL_PROGRAM) -m 644 -D \
		$(PKGFILE).in $(DESTDIR)/$(pkgconfdir)/$(PKGFILE)
	perl -i -pe 's|__VERSION__|$(LIBDMMP_VERSION)|g' \
		$(DESTDIR)/$(pkgconfdir)/$(PKGFILE)
	perl -i -pe 's|__LIBDIR__|$(syslibdir)|g' \
		$(DESTDIR)/$(pkgconfdir)/$(PKGFILE)
	perl -i -pe 's|__INCLUDEDIR__|$(includedir)|g' \
		$(DESTDIR)/$(pkgconfdir)/$(PKGFILE)
	@for file in docs/man/*.3.gz; do \
		$(INSTALL_PROGRAM) -m 644 -D \
			$$file \
			$(DESTDIR)/$(man3dir)/ || exit $?; \
	done

uninstall:
	rm -f $(DESTDIR)$(syslibdir)/$(LIBS)
	rm -f $(DESTDIR)$(includedir)/$(HEADERS)
	rm -f $(DESTDIR)/$(syslibdir)/$(DEVLIB)
	@for file in $(DESTDIR)/$(man3dir)/dmmp_*; do \
		rm $$file; \
	done
	rm -f $(DESTDIR)$(man3dir)/libdmmp.h*

clean:
	rm -f core *.a *.o *.gz *.so *.so.*
	rm -f docs/man/*.3.gz
	$(MAKE) -C test clean

check: all
	$(MAKE) -C test check

speed_test: all
	$(MAKE) -C test speed_test

doc:
	@for file in $(EXTRA_MAN_FILES); do \
		$(INSTALL_PROGRAM) -m 644 -D docs/$$file docs/man/$$file; \
	done
	perl docs/kernel-doc -man $(HEADERS) | perl docs/split-man.pl docs/man
	@for file in docs/man/*.3; do \
		gzip -f $$file; \
	done
	find docs/man -type f -name \*[0-9].gz
