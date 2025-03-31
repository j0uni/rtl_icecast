CC = g++
CFLAGS = -Wall -std=c++11 -O2

# Detect operating system
UNAME_S := $(shell uname -s)

# macOS specific settings
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/include -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1
    LDFLAGS = -L/opt/homebrew/lib -lrtlsdr -lliquid -lmp3lame -lshout -lm -lpthread
else
    # Linux and other systems
    LDFLAGS = -lrtlsdr -lliquid -lmp3lame -lshout -lm -lpthread
endif

SOURCES = rtl_icecast.cpp config.cpp
BUILD_DIR = build
OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET = $(BUILD_DIR)/rtl_icecast

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)