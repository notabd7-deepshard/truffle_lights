#ifdef __aarch64__

#include "ledcontrol.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Starting LED state test...\n";
    
    // Create instance of the controller
    LEDController ledController;
    
    // Start with dormant state
    std::cout << "Setting DORMANT state for 30 seconds...\n";
    ledController.SetState(LEDState::DORMANT);
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Switch to active state
    std::cout << "Setting ACTIVE state for 30 seconds...\n";
    ledController.SetState(LEDState::ACTIVE);
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Switch to respond-to-user state
    std::cout << "Setting RESPOND_TO_USER state for 30 seconds...\n";
    ledController.SetState(LEDState::RESPOND_TO_USER);
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    // Back to dormant
    std::cout << "Setting DORMANT state for final 30 seconds...\n";
    ledController.SetState(LEDState::DORMANT);
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
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