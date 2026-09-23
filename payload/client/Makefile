# MaxRegnerOS Client Makefile

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE

ARCH ?= host
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/mrca-client

# Detect if CC is targeting Android (e.g. aarch64-linux-android29-clang)
IS_ANDROID := $(if $(findstring android,$(CC)),1,0)

# Android's Bionic C library incorporates pthread directly in libc.a.
# Linking -lpthread on Android causes ld.lld to fail.
ifeq ($(IS_ANDROID),1)
    LDLIBS :=
else
    LDLIBS := -lpthread
endif

SRCS := log.c util.c ring.c net.c ws.c proto.c input.c display.c drm.c \
        codec.c audio.c metrics.c watchdog.c config.c mrca_status.c main.c
OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

.PHONY: all clean

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ── LINE 29 FIX ──────────────────────────────────────────────────────────
# OLD: $(CC) $(LDFLAGS) -static -o $@ $^ -lpthread
# NEW: Replace -lpthread with $(LDLIBS)
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

clean:
	rm -rf build
