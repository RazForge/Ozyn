# Ozayn Core Engine — Makefile
# Builds libozayn_core.so shared library

CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -I./include
LDFLAGS = -shared -lm -lssl -lcrypto

SRC_DIR = ./src
BUILD_DIR = ./build
INCLUDE_DIR = ./include

SRCS = $(SRC_DIR)/ml_engine.c \
       $(SRC_DIR)/vision.c \
       $(SRC_DIR)/system_control.c \
       $(SRC_DIR)/crypto.c \
       $(SRC_DIR)/utility.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TARGET = $(BUILD_DIR)/libozayn_core.so

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.so

install: $(TARGET)
	cp $(TARGET) /usr/local/lib/
	ldconfig
	@echo "Installed: /usr/local/lib/libozayn_core.so"

test: $(TARGET)
	@echo "Testing core library..."
	@LD_LIBRARY_PATH=$(BUILD_DIR) python3 -c " \
		import ctypes; \
		lib = ctypes.CDLL('$(BUILD_DIR)/libozayn_core.so'); \
		lib.ozayn_version.restype = ctypes.c_char_p; \
		v = lib.ozayn_version(); \
		print(f'Core version: {v.decode()}'); \
		cpu = lib.ozayn_sys_get_cpu_usage(); \
		print(f'CPU usage: {cpu}%'); \
		print('All tests passed!') \
	"
