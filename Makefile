CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Iinclude

SRCS := Server.cpp
TARGET := server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)