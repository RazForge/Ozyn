# Ozayn Control Room — Build System
# C/C++ controller: voice + gesture + keyboard

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -pthread -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
INCLUDE = -Iinclude
LDFLAGS = -lpthread -lX11 -lXtst -lm

# Optional: uncomment if you have these libs
# LDFLAGS += -lasound         # ALSA audio
# LDFLAGS += -lvosk           # Speech recognition

SRC     = src/main.c src/ipc.c src/voice.c src/gesture.c src/keyboard.c src/hand_data.c src/gesture_classifier.c src/cursor_engine.c
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
