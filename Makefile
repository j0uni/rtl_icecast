CC = g++
CFLAGS = -Wall -std=c++11 -O2 -Werror -Wunused-variable

# Detect operating system
UNAME_S := $(shell uname -s)

# macOS specific settings
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/include -Wno-deprecated-declarations -Wno-return-type-c-linkage
    LDFLAGS = -L/opt/homebrew/lib -lrtlsdr -lliquid -lmp3lame -lshout -lm -lpthread
else
    # Linux and other systems
    CFLAGS += -Wno-deprecated-declarations -Wno-return-type-c-linkage
    LDFLAGS = -lrtlsdr -lliquid -lmp3lame -lshout -lm -lpthread
endif

# Source directories
SRC_DIR = src
SRC_SUBDIRS = $(SRC_DIR) $(SRC_DIR)/audio $(SRC_DIR)/radio $(SRC_DIR)/streaming $(SRC_DIR)/config $(SRC_DIR)/ui
VPATH = $(SRC_SUBDIRS)

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/audio/demodulator.cpp \
          $(SRC_DIR)/audio/mp3_encoder.cpp \
          $(SRC_DIR)/radio/rtl_device.cpp \
          $(SRC_DIR)/radio/scanner.cpp \
          $(SRC_DIR)/streaming/icecast_client.cpp \
          $(SRC_DIR)/config/config.cpp \
          $(SRC_DIR)/ui/status_display.cpp

# Object files
BUILD_DIR = build
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# Include paths
INCLUDES = $(addprefix -I,$(SRC_SUBDIRS))

# Target executable
TARGET = $(BUILD_DIR)/rtl_icecast

.PHONY: all clean

all: $(BUILD_DIR) $(BUILD_DIR)/audio $(BUILD_DIR)/radio $(BUILD_DIR)/streaming $(BUILD_DIR)/config $(BUILD_DIR)/ui $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/audio:
	mkdir -p $(BUILD_DIR)/audio

$(BUILD_DIR)/radio:
	mkdir -p $(BUILD_DIR)/radio

$(BUILD_DIR)/streaming:
	mkdir -p $(BUILD_DIR)/streaming

$(BUILD_DIR)/config:
	mkdir -p $(BUILD_DIR)/config

$(BUILD_DIR)/ui:
	mkdir -p $(BUILD_DIR)/ui

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

# Install target
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Dependencies
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
$(BUILD_DIR)/audio/demodulator.o: $(SRC_DIR)/audio/demodulator.cpp
$(BUILD_DIR)/audio/mp3_encoder.o: $(SRC_DIR)/audio/mp3_encoder.cpp
$(BUILD_DIR)/radio/rtl_device.o: $(SRC_DIR)/radio/rtl_device.cpp
$(BUILD_DIR)/radio/scanner.o: $(SRC_DIR)/radio/scanner.cpp
$(BUILD_DIR)/streaming/icecast_client.o: $(SRC_DIR)/streaming/icecast_client.cpp
$(BUILD_DIR)/config/config.o: $(SRC_DIR)/config/config.cpp
$(BUILD_DIR)/ui/status_display.o: $(SRC_DIR)/ui/status_display.cpp