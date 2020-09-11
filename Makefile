OBJS	= \
	http-parser/http_parser.o \
	log.o \
	metrics.o \
	collect_pf.o \
	collect_cpu.o \
	collect_if.o \
	main.o

CFLAGS 	+= -fno-strict-aliasing -fstack-protector-all -Werror \
	   -fwrapv -fPIC -Wall -g -gdwarf-2 -falign-functions -faligned-allocation

.PHONY: all
all: obsd-prom-exporter

.PHONY: clean
clean:
	rm -f obsd-prom-exporter $(OBJS)

.SUFFIXES: .c .o
.c.o:
	$(CC) -c -o $@ $(CFLAGS) $<

obsd-prom-exporter: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $(OBJS)

