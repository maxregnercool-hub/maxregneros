# MaxRegnerOS Client Makefile

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE

ARCH ?= host
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/mrca-client

# -----------------------------------------------------------------------------
# OS & Toolchain Detection
# Automatically detects Android NDK compilers (e.g. aarch64-linux-android29-clang)
# -----------------------------------------------------------------------------
IS_ANDROID := 0

ifneq ($(findstring android,$(CC)),)
    IS_ANDROID := 1
else ifeq ($(NDK_BUILD),1)
    IS_ANDROID := 1
endif

# -----------------------------------------------------------------------------
# Libraries
# Android's Bionic libc embeds pthread. Linux glibc requires -lpthread.
# -----------------------------------------------------------------------------
ifeq ($(IS_ANDROID),1)
    LDLIBS :=
else
    LDLIBS := -lpthread
endif

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

.PHONY: all clean

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── LINE 29 PERMANENT FIX ────────────────────────────────────────────────
# Uses $(LDLIBS) so Android gets no -lpthread while Linux host still gets it.
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

clean:
	rm -rf build
