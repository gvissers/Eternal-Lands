-include ../make.conf

CFLAGS = -DLINUX $(foreach FEATURE, $(FEATURES), -D$(FEATURE)) \
	-g -Wall -W $(shell sdl-config --cflags) $(shell xml2-config --cflags)
CXXFLAGS = -DLINUX $(foreach FEATURE, $(FEATURES), -D$(FEATURE)) \
	-g -Wall -W $(shell sdl-config --cflags) $(shell xml2-config --cflags)
LDFLAGS = $(shell xml2-config --libs)

all: build_actor_defs

build_actor_defs: build_actor_defs.o xml.o
	$(CXX) -o$@ $^ $(LDFLAGS)

xml.o: ../xml.c
	$(CC) $(CFLAGS) -c -o$@ $<
