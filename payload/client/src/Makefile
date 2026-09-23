# MaxRegnerOS Cloud Avatar — client build
CC      ?= cc
TARGET  ?= mrca-client
BUILD   ?= build
PREFIX  ?= /data/adb/maxregner

SRCS := log.c util.c ring.c net.c ws.c proto.c input.c display.c drm.c \
        codec.c audio.c metrics.c watchdog.c config.c mrca_status.c main.c
OBJS := $(addprefix $(BUILD)/,$(SRCS:.c=.o))

CFLAGS  := -std=c11 -O2 -g0 -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -Iinclude -D_GNU_SOURCE
LDFLAGS ?=
LDLIBS  += -lpthread

ifeq ($(JPEG),1)
  CFLAGS  += -DMRCA_HAVE_JPEG
  LDLIBS  += -ljpeg
endif

ifeq ($(STATIC),1)
  LDFLAGS += -static
endif

.PHONY: all clean install test
all: $(BUILD)/$(TARGET)

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

install: $(BUILD)/$(TARGET)
	mkdir -p $(PREFIX)/bin
	cp -f $(BUILD)/$(TARGET) $(PREFIX)/bin/$(TARGET)
	chmod 0755 $(PREFIX)/bin/$(TARGET)

test: $(BUILD)/mrca_tests $(BUILD)/mrca_ws_test
	$(BUILD)/mrca_tests
	$(BUILD)/mrca_ws_test

$(BUILD)/mrca_ws_test: tests/test_ws_integration.c net.c ws.c proto.c util.c log.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

$(BUILD)/mrca_tests: tests/test_main.c util.c ring.c proto.c config.c mrca_status.c log.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(BUILD)

$(BUILD)/gen_vectors: tests/gen_vectors.c proto.c util.c log.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -Itests -o $@ $^ $(LDLIBS)

vecgen: $(BUILD)/gen_vectors
