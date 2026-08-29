CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -I include -D_POSIX_C_SOURCE=200809L
LDFLAGS =
SRC     = src
BUILD   = build
TARGET  = ozayn

SRCS    = $(wildcard $(SRC)/*.c)
OBJS    = $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(SRCS))

all: $(BUILD)/$(TARGET)

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo ""
	@echo "  Built: $(BUILD)/$(TARGET)"
	@echo ""

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
