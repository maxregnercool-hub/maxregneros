# Target executable
TARGET ?= mrca-client

# Compiler & Flags
CC ?= gcc
CFLAGS ?= -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter -Iinclude -D_GNU_SOURCE

# Detect NDK cross-compiler or NDK_BUILD flag
ifneq ($(NDK_BUILD),1)
    ifneq ($(findstring android,$(CC)),)
        NDK_BUILD := 1
    endif
endif

# Assign libraries: -lpthread is excluded on Android NDK builds
ifeq ($(NDK_BUILD),1)
    LDLIBS ?=
else
    LDLIBS ?= -lpthread
endif

# Source files
SRCS := log.c util.c ring.c net.c ws.c proto.c input.c display.c drm.c codec.c audio.c metrics.c watchdog.c config.c mrca_status.c main.c
OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

BUILD_DIR ?= build

.PHONY: all clean test

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test:
	@echo "Running tests..."

clean:
	rm -rf build/
