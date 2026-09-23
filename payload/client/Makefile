# MaxRegnerOS Client Makefile

CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE
BUILD_DIR ?= build/$(ARCH)
TARGET ?= $(BUILD_DIR)/mrca-client

# Detect if we are building with Android NDK
IS_ANDROID := $(if $(findstring android,$(CC)),1,0)

# Bionic (Android libc) includes pthread functions directly in libc.
# Standard Linux GLIBC requires explicit -lpthread.
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

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

clean:
	rm -rf build
