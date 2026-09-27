CXX ?= g++
CXXFLAGS ?= -O3 -march=native -fopenmp -std=c++17 -Wall -Wextra
TARGET = aco

all: $(TARGET)

$(TARGET): aco.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) aco.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
