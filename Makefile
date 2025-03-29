CC = g++
CFLAGS = -Wall -std=c++11 -O2
LDFLAGS = -lrtlsdr -lliquid -lmp3lame -lshout -lm -lpthread

SOURCES = rtl_icecast.cpp config.cpp scanner.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = rtl_icecast

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) 