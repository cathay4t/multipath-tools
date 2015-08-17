# Makefile
#
# Copyright (C) 2015 Gris Ge <fge@redhat.com>
#
include ../Makefile.inc
TEST_EXEC = libmultipath_test
CFLAGS += -I$(multipathdir)
LDFLAGS += -L$(multipathdir) -lmultipath

all: $(TEST_EXEC)

check: $(TEST_EXEC)
	sudo env LD_LIBRARY_PATH=$(multipathdir) ./$(TEST_EXEC)

clean:
	rm -f $(TEST_EXEC)
