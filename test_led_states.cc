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
    std::cout << "Starting LED state test ...\n";
    std::cout << "Cycling through all LED states: PROMPT -> DORMANT -> ACTIVE -> RESPOND_TO_USER -> PROCESSING -> ERROR\n";
    
    // Set up signal handler for clean termination
    signal(SIGINT, signal_handler);
    
    // Create instance of the controller
    LEDController ledController;
    
    // For state cycling
    const int state_duration_seconds = 5;
    
    // Run continuously until interrupted
    while (running) {
        // 1. PROMPT state
        std::cout << "Setting PROMPT state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::PROMPT);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;
        
        // 2. DORMANT state
        std::cout << "Setting DORMANT state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::DORMANT);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;
        
        // 3. ACTIVE state
        std::cout << "Setting ACTIVE state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::ACTIVE);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;
        
        // 4. RESPOND_TO_USER state
        std::cout << "Setting RESPOND_TO_USER state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::RESPOND_TO_USER);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;

        // 5. PROCESSING state
        std::cout << "Setting PROCESSING state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::PROCESSING);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;

        // 6. ERROR state
        std::cout << "Setting ERROR state for " << state_duration_seconds << " seconds...\n";
        ledController.SetState(LEDState::ERROR);
        std::this_thread::sleep_for(std::chrono::seconds(state_duration_seconds));
        if (!running) break;
        
        std::cout << "Completed one full cycle through all states, starting again...\n";
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