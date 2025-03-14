CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 -Wno-deprecated-declarations
LDFLAGS = -lrtlsdr -lliquid -lmp3lame -lshout

TARGET = rtl_icecast
SRCS = rtl_icecast.cpp config.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) 