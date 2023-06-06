# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# Define directories to use
ROOTDIR=$(shell pwd)
OBJDIR=$(ROOTDIR)/build/
SRCDIR=$(ROOTDIR)/src/

# Object file shorthand
OBJS_C := $(addprefix $(OBJDIR), $(patsubst %.c, %.o, $(shell ls $(SRCDIR))))

# Binary name
APP = middlebox

# Executable location from project root directory
TARGET = $(OBJDIR)/middlebox

# Build using pkg-config variables if possible
PKGCONF ?= pkg-config

ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 -I$(ROOTDIR)/inc $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)

.PHONY: all
all: $(TARGET)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)%.o: $(SRCDIR)%.c | $(OBJDIR) # order-only build; triggered only if necessary
	cc $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS_C)
	cc -o $@ $^ $(LDFLAGS_SHARED)

.PHONY: clean
clean:
	rm -f build/$(APP) build/*.o 
