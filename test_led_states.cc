#ifdef __aarch64__

#include "ledcontrol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

// Flag to control the main loop
volatile sig_atomic_t running = 1;

// Signal handler for Ctrl+C
void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived interrupt signal. Stopping LED test...\n";
        running = 0;
    }
}

int main() {
    std::cout << "Starting LED state test (Press Ctrl+C to exit)...\n";
    
    // Set up signal handler for clean termination
    signal(SIGINT, signal_handler);
    
    // Create instance of the controller
    LEDController ledController;
    
    // Run continuously until interrupted
    while (running) {
        //Start with dormant state
        std::cout << "Setting DORMANT state for 15 seconds...\n";
        ledController.SetState(LEDState::DORMANT);
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Switch to active state
        std::cout << "Setting ACTIVE state for 15 seconds...\n";
        ledController.SetState(LEDState::ACTIVE);
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Switch to respond-to-user state
        std::cout << "Setting RESPOND_TO_USER state for 15 seconds...\n";
        ledController.SetState(LEDState::RESPOND_TO_USER);
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Switch to the new PROMPT state
        std::cout << "Setting PROMPT state for 30 seconds...\n";
        ledController.SetState(LEDState::PROMPT);
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (!running) break;
        
        std::cout << "Completed one cycle, starting again...\n";
    }
    
    // Ensure we end in dormant state when exiting
    std::cout << "Restoring DORMANT state before exit...\n";
    ledController.SetState(LEDState::DORMANT);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "LED state test finished.\n";
    return 0;
}

#else
#include <iostream>

int main() {
    std::cout << "This test program only runs on ARM64 architecture.\n";
    return 0;
}
#endif 