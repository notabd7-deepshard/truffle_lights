CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -I.
LDFLAGS = -pthread

# Source files (note: spi is header-only)
SOURCES = ledcontrol.cc

# Object files
OBJECTS = $(SOURCES:.cc=.o)

# Main targets
all: test_connecting_state wifi_symbol_demo

test_connecting_state: test_connecting_state.o $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

wifi_symbol_demo: wifi_symbol_demo.o $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Object file rules
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f *.o test_connecting_state wifi_symbol_demo

# Convenience targets
.PHONY: clean all run_connect run_demo

run_connect: test_connecting_state
	@echo "Running connecting state test..."
	sudo ./test_connecting_state

run_demo: wifi_symbol_demo
	@echo "Running WiFi symbol demo..."
	sudo ./wifi_symbol_demo

# Help target
help:
	@echo "Available targets:"
	@echo "  make all          - Build both test programs"
	@echo "  make wifi_symbol_demo - Build just the demo"
	@echo "  make test_connecting_state - Build just the test"
	@echo "  make run_demo     - Build and run the WiFi demo"
	@echo "  make run_connect  - Build and run connecting test"
	@echo "  make clean        - Remove built files"
	@echo ""
	@echo "Note: You need SPI enabled. Run setup/enable_spi.sh first if not done"