CXX ?= g++
CXXFLAGS ?= -O3 -march=native -fopenmp -std=c++17 -Wall -Wextra
BIN_DIR = ../bin
TARGET = $(BIN_DIR)/aco

all: $(TARGET)

$(TARGET): aco.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) aco.cpp

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run: $(TARGET)
	$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
