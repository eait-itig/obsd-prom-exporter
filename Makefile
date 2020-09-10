OBJS	= \
	http-parser/http_parser.o \
	main.o

.PHONY: all
all: obsd-prom-exporter

.PHONY: clean
clean:
	rm -f obsd-prom-exporter $(OBJS)

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<

obsd-prom-exporter: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $(OBJS)

