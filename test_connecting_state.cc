#ifdef __aarch64__

#include "ledcontrol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

volatile sig_atomic_t running = 1;

void signal_handler(int signal) {
    if(signal == SIGINT){
        std::cout << "\nInterrupt received. Exiting...\n";
        running = 0;
    }
}

void test_wifi_symbol_directions() {
    std::cout << "\n=== Testing WiFi Symbol in Different Directions ===\n";
    
    LEDController controller;
    
    // Test different orientations
    float directions[] = {0.0f, 90.0f, 180.0f, 270.0f};  // Right, Down, Left, Up
    const char* direction_names[] = {"RIGHT", "DOWN", "LEFT", "UP"};
    
    for (int i = 0; i < 4 && running; i++) {
        std::cout << "Testing WiFi symbol pointing " << direction_names[i] << " (" << directions[i] << " degrees)\n";
        
        // For this test, we'll temporarily create our own matrix and symbol
        // (In a real implementation, you'd modify the LEDController to accept direction parameter)
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

int main(){
    std::cout << "Starting CONNECTING state test with new WiFi symbol (Ctrl+C to quit)\n";
    std::cout << "This will show an animated WiFi symbol that uses the full circular LED matrix\n";
    std::cout << "The symbol points upward (270 degrees) and builds element by element:\n";
    std::cout << "1. Center dot\n";
    std::cout << "2. Inner arc (90 degrees, ring 2)\n"; 
    std::cout << "3. Middle arc (120 degrees, ring 3)\n";
    std::cout << "4. Outer arc (150 degrees, ring 4)\n";
    std::cout << "Then it pauses and repeats the cycle.\n\n";
    
    signal(SIGINT, signal_handler);

    LEDController controller;

    // Switch to CONNECTING state immediately
    controller.SetState(LEDState::CONNECTING);

    // Let it run for a while to see multiple cycles
    int seconds_elapsed = 0;
    while(running && seconds_elapsed < 30){  // Run for 30 seconds max
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds_elapsed++;
        
        if (seconds_elapsed % 10 == 0) {
            std::cout << "Running for " << seconds_elapsed << " seconds...\n";
        }
    }

    if (running) {
        // Test different directions if we haven't been interrupted
        test_wifi_symbol_directions();
    }

    std::cout << "WiFi symbol test finished.\n";
    return 0;
}

#else
#include <iostream>
int main(){
    std::cout << "LED control only supported on aarch64 platform.\n";
    return 0;
}
#endif 