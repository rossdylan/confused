PREFIX ?= /usr/local

CFLAGS += -std=c99 -Wall -Wextra -fPIC -pedantic
CFLAGS += $(shell pkg-config --cflags-only-other fuse libmemcached)
CPPFLAGS += -I./include
CPPFLAGS += $(shell pkg-config --cflags-only-I fuse libmemcached)
ifeq ($(CC),gcc)
    CFLAGS += -ggdb3
endif
ifeq ($(CC),clang)
    CFLAGS += -Qunused-arguments -ggdb -Weverything
endif

LDLIBS += $(shell pkg-config --libs fuse libmemcached)
VPATH := src
OBJS := confused.o
EXECUTABLE := confused

$(OBJS): %.o: src/%.c

$(EXECUTABLE): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)
.phony: clean
clean:
	rm -rf $(EXECUTABLE) $(OBJS)
