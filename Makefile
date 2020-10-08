#
# Copyright 2020 The University of Queensland
# Author: Alex Wilson <alex@uq.edu.au>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

OBJS	= \
	http-parser/http_parser.o \
	log.o \
	metrics.o \
	collect_kstat.o \
	main.o

CFLAGS 	+= -fno-strict-aliasing -fstack-protector-all -Werror \
	   -fwrapv -fPIC -Wall -m64 -msave-args -gdwarf-2
LDFLAGS += -m64 -lnsl -lsocket -lkstat -lssp

.PHONY: all
all: prom-exporter

.PHONY: clean
clean:
	rm -f prom-exporter $(OBJS)

.SUFFIXES: .c .o
.c.o:
	$(CC) -c -o $@ $(CFLAGS) $<

prom-exporter: $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $(OBJS)
	ctfconvert prom-exporter

.PHONY: install
install: all
	install -o root -g bin -m 0755 prom-exporter /opt/local/bin/
	install -o root -g bin -m 0644 prom-exporter.xml /opt/local/lib/svc/manifest/
	svccfg import /opt/local/lib/svc/manifest/prom-exporter.xml
