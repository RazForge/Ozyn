CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -ldl -rdynamic
BUILD   = build
TARGET  = ozayn

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    PLATFORM_SRC = src/platform/linux/platform_linux.c
    PLATFORM_NAME = linux
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM_SRC = src/platform/macos/platform_macos.c
    PLATFORM_NAME = macos
    LDFLAGS += -framework CoreFoundation
endif
ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    PLATFORM_SRC = src/platform/windows/platform_windows.c
    PLATFORM_NAME = windows
    LDFLAGS += -lws2_32 -liphlpapi
    TARGET = ozayn.exe
endif
ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    PLATFORM_SRC = src/platform/windows/platform_windows.c
    PLATFORM_NAME = windows
    LDFLAGS += -lws2_32 -liphlpapi
    TARGET = ozayn.exe
endif

# Default to Linux if nothing matched
ifndef PLATFORM_SRC
    PLATFORM_SRC = src/platform/linux/platform_linux.c
    PLATFORM_NAME = linux
endif

SRCS    = $(wildcard src/*.c) $(wildcard src/core/*.c) $(PLATFORM_SRC)
OBJS    = $(patsubst src/%.c, $(BUILD)/%.o, $(SRCS))

PLUGIN_DIR  = plugins
PLUGIN_SRCS = $(wildcard $(PLUGIN_DIR)/*.c)
PLUGIN_SO   = $(patsubst $(PLUGIN_DIR)/%.c, $(PLUGIN_DIR)/%.so, $(PLUGIN_SRCS))

TOOLS_DIR   = tools
TOOLS_SRCS  = $(wildcard $(TOOLS_DIR)/*.c)
TOOLS_BIN   = $(patsubst $(TOOLS_DIR)/*.c, $(BUILD)/%, $(TOOLS_SRCS))

all: $(BUILD)/$(TARGET) plugins tools

$(BUILD)/%.o: src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo ""
	@echo "  Built: $(BUILD)/$(TARGET) ($(PLATFORM_NAME))"
	@echo ""

$(BUILD):
	mkdir -p $(BUILD)

plugins: $(PLUGIN_SO)

$(PLUGIN_DIR)/%.so: $(PLUGIN_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@
	@echo "  Built plugin: $@"

tools: $(TOOLS_BIN)

$(BUILD)/%: $(TOOLS_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
	@echo "  Built tool: $@"

run: all
	./$(BUILD)/$(TARGET)

test: all
	@echo "  No tests yet."
	@echo ""

clean:
	rm -rf $(BUILD)
	rm -f $(PLUGIN_DIR)/*.so

.PHONY: all run test clean plugins tools
