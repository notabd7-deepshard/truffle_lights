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

// Helper function to print transition information
void printTransition(const std::string& from, const std::string& to) {
    std::cout << "\n===================================\n";
    std::cout << "TRANSITIONING FROM " << from << " TO " << to;
    std::cout << "\n===================================\n";
}

int main() {
    std::cout << "Starting LED state test (Press Ctrl+C to exit)...\n";
    
    // Set up signal handler for clean termination
    signal(SIGINT, signal_handler);
    
    // Create instance of the controller
    LEDController ledController;
    
    // Define HSV color profiles for each state
    const std::array<HSV, 3> dormantHSV = {
        HSV{220.0f, 0.7f, 0.8f},   // Blue
        HSV{220.0f, 0.8f, 0.6f},   // Blue variant
        HSV{200.0f, 0.7f, 0.8f}    // Blue-cyan
    };
    
    // const std::array<HSV, 3> activeHSV = {
    //     HSV{245.0f, 0.1f, 1.0f},   // White-ish
    //     HSV{40.0f, 0.8f, 1.0f},    // Blue
    //     HSV{290.0f, 0.7f, 0.9f}    // Purple
    // };
    
    const std::array<HSV, 3> respondHSV = {
        HSV{30.0f, 0.9f, 1.0f},    // Orange
        HSV{40.0f, 0.9f, 1.0f},    // Orange-yellow
        HSV{15.0f, 0.8f, 0.9f}     // Orange-red
    };
    
    const std::array<HSV, 3> promptHSV = {
        HSV{0.0f, 0.0f, 1.0f},     // White
        HSV{0.0f, 0.0f, 0.9f},     // White variant
        HSV{0.0f, 0.0f, 0.8f}      // White variant
    };
    
    // Start with dormant state (using RequestState instead of SetState)
    printTransition("INIT", "DORMANT");
    ledController.RequestState(LEDState::DORMANT, dormantHSV);
    
    // Run continuously until interrupted
    while (running) {
        // Stay in dormant state for a while
        std::cout << "Running DORMANT state for 15 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Transition to active state
        // printTransition("DORMANT", "ACTIVE");
        // ledController.RequestState(LEDState::ACTIVE, activeHSV);
        // std::cout << "Running ACTIVE state for 15 seconds...\n";
        // std::this_thread::sleep_for(std::chrono::seconds(15));
        // if (!running) break;
        
        // Transition to respond-to-user state
        printTransition("DORMANT", "RESPOND_TO_USER");
        ledController.RequestState(LEDState::RESPOND_TO_USER, respondHSV);
        std::cout << "Running RESPOND_TO_USER state for 15 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Transition to the new PROMPT state
        printTransition("RESPOND_TO_USER", "PROMPT");
        ledController.RequestState(LEDState::PROMPT, promptHSV);
        std::cout << "Running PROMPT state for 15 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        // Transition back to DORMANT state to complete the cycle
        printTransition("PROMPT", "DORMANT");
        ledController.RequestState(LEDState::DORMANT, dormantHSV);
        std::cout << "Running DORMANT state for 15 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running) break;
        
        std::cout << "Completed one cycle, starting again...\n";
    }
    
    // End cleanly - no need to SetState directly, it will respect current animation
    std::cout << "Test finished, exiting.\n";
    return 0;
}

#else
#include <iostream>

int main() {
    std::cout << "LED control is only supported on aarch64 platform.\n";
    return 0;
}
#endif 