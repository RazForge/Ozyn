# Ozayn Control Room — Build System
# C/C++ controller: voice + gesture + keyboard

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -pthread -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
INCLUDE = -Iinclude
LDFLAGS = -lpthread

# Optional: uncomment if you have these libs
# LDFLAGS += -lX11 -lXtst    # X11 key simulation
# LDFLAGS += -lasound         # ALSA audio
# LDFLAGS += -lvosk           # Speech recognition

SRC     = src/main.c src/ipc.c src/voice.c src/gesture.c src/keyboard.c
OBJ     = $(SRC:.c=.o)
BUILD   = build
TARGET  = $(BUILD)/ozayn_control

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $(SRC) $(LDFLAGS)
	@echo "Built: $(TARGET)"

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: clean all
