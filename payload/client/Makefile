# MaxRegnerOS Client Makefile

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE

ARCH ?= host
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/mrca-client

# -----------------------------------------------------------------------------
# OS & Toolchain Detection
# -----------------------------------------------------------------------------
# Detect if building via Android NDK compiler or Termux environment
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
# Libraries (LDLIBS)
# -----------------------------------------------------------------------------
# Android's Bionic C library includes POSIX threads directly inside libc.a/libc.so.
# Linking -lpthread on Android causes ld.lld to fail with "unable to find library".
ifeq ($(IS_ANDROID),1)
    LDLIBS ?=
else
    LDLIBS ?= -lpthread
endif

# -----------------------------------------------------------------------------
# Sources & Objects
# -----------------------------------------------------------------------------
SRCS := log.c \
        util.c \
        ring.c \
        net.c \
        ws.c \
        proto.c \
        input.c \
        display.c \
        drm.c \
        codec.c \
        audio.c \
        metrics.c \
        watchdog.c \
        config.c \
        mrca_status.c \
        main.c

OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

# -----------------------------------------------------------------------------
# Build Rules
# -----------------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test:
	@echo "No tests defined for client binary."

clean:
	rm -rf build
