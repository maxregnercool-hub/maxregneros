# MaxRegnerOS Cloud Avatar — client build
CC      ?= cc
TARGET  ?= mrca-client
BUILD   ?= build
PREFIX  ?= /data/adb/maxregner

# -----------------------------------------------------------------------------
# OS & Toolchain Detection
# -----------------------------------------------------------------------------
IS_ANDROID := 0

ifneq ($(findstring android,$(CC)),)
    IS_ANDROID := 1
else ifneq ($(wildcard /system/bin/app_process*),)
    # Termux / on-device Android environment
    IS_ANDROID := 1
else ifeq ($(NDK_BUILD),1)
    IS_ANDROID := 1
endif

# -----------------------------------------------------------------------------
# Flags & Libraries
# -----------------------------------------------------------------------------
SRCS := log.c util.c ring.c net.c ws.c proto.c input.c display.c drm.c \
        codec.c audio.c metrics.c watchdog.c config.c mrca_status.c main.c
OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o))

CFLAGS  := -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -Iinclude -D_GNU_SOURCE
LDFLAGS ?=

# Android Bionic libc includes pthread directly in libc.
# Linux glibc requires -lpthread.
ifeq ($(IS_ANDROID),1)
    LDLIBS ?=
else
    LDLIBS ?= -lpthread
endif

ifeq ($(JPEG),1)
    CFLAGS += -DMRCA_HAVE_JPEG
    LDLIBS += -ljpeg
endif

ifeq ($(STATIC),1)
    LDFLAGS += -static
endif

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------
.PHONY: all clean install test vecgen

all: $(BUILD)/$(TARGET)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

install: $(BUILD)/$(TARGET)
	mkdir -p $(PREFIX)/bin
	cp -f $(BUILD)/$(TARGET) $(PREFIX)/bin/$(TARGET)
	chmod 0755 $(PREFIX)/bin/$(TARGET)

test: $(BUILD)/mrca_tests $(BUILD)/mrca_ws_test
	$(BUILD)/mrca_tests
	$(BUILD)/mrca_ws_test

$(BUILD)/mrca_ws_test: tests/test_ws_integration.c net.c ws.c proto.c util.c log.c | $(BUILD)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

$(BUILD)/mrca_tests: tests/test_main.c util.c ring.c proto.c config.c mrca_status.c log.c | $(BUILD)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

$(BUILD)/gen_vectors: tests/gen_vectors.c proto.c util.c log.c | $(BUILD)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

vecgen: $(BUILD)/gen_vectors

clean:
	rm -rf $(BUILD)
