CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS = -ldl -rdynamic
BUILD   = build
TARGET  = ozayn

SRCS    = $(wildcard src/*.c) $(wildcard src/core/*.c)
OBJS    = $(patsubst src/%.c, $(BUILD)/%.o, $(SRCS))

PLUGIN_DIR  = plugins
PLUGIN_SRCS = $(wildcard $(PLUGIN_DIR)/*.c)
PLUGIN_SO   = $(patsubst $(PLUGIN_DIR)/%.c, $(PLUGIN_DIR)/%.so, $(PLUGIN_SRCS))

all: $(BUILD)/$(TARGET) plugins

$(BUILD)/%.o: src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo ""
	@echo "  Built: $(BUILD)/$(TARGET)"
	@echo ""

$(BUILD):
	mkdir -p $(BUILD)

plugins: $(PLUGIN_SO)

$(PLUGIN_DIR)/%.so: $(PLUGIN_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@
	@echo "  Built plugin: $@"

run: all
	./$(BUILD)/$(TARGET)

test: all
	@echo "  No tests yet."
	@echo ""

clean:
	rm -rf $(BUILD)
	rm -f $(PLUGIN_DIR)/*.so

.PHONY: all run test clean plugins
