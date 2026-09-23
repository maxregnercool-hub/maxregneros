# ✅ CORRECT Makefile:

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE

ARCH ?= host
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/mrca-client

# Check if compiling with an Android target compiler
IS_ANDROID := $(if $(findstring android,$(CC)),1,0)

# Bionic (Android libc) includes pthread functions directly in libc.a/libc.so.
# Standard Linux glibc needs -lpthread.
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

# LINE 29: Use $(LDLIBS) here instead of literal -lpthread
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

clean:
	rm -rf build
