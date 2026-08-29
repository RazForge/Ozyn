CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS =
BUILD   = build
TARGET  = ozayn

SRCS    = $(wildcard src/*.c) $(wildcard src/core/*.c)
OBJS    = $(patsubst src/%.c, $(BUILD)/%.o, $(SRCS))

all: $(BUILD)/$(TARGET)

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

run: all
	./$(BUILD)/$(TARGET)

test: all
	@echo "  No tests yet."
	@echo ""

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
