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

.PATH:		${.CURDIR}/http-parser

PROG=		obsd-prom-exporter
MAN=

BINDIR=		/usr/local/bin

SRCS=		main.c log.c metrics.c
SRCS+=		http_parser.c

SRCS+=		collect_pf.c
SRCS+=		collect_cpu.c
SRCS+=		collect_if.c
SRCS+=		collect_uvm.c
SRCS+=		collect_pools.c
SRCS+=		collect_procs.c
SRCS+=		collect_disk.c

CFLAGS+=	-fno-strict-aliasing -fstack-protector-all -Werror \
		    -fwrapv -fPIC -Wall

installids:
	groupadd _promexp
	useradd -g _promexp _promexp

afterinstall:
	${INSTALL} ${INSTALL_COPY} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${.CURDIR}/rc.d/promexporter /etc/rc.d/

.include <bsd.prog.mk>
