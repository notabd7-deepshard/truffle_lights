#ifdef __aarch64__

#include "ledcontrol.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <vector>

volatile sig_atomic_t running = 1;
void signal_handler(int sig){
    if(sig == SIGINT){
        std::cout << "\nReceived Ctrl+C, exiting test...\n";
        running = 0;
    }
}

int main(){
    std::cout << "Starting Placeholder Transition test (Ctrl+C to quit)\n";

    signal(SIGINT, signal_handler);

    LEDController ctrl;

    // Define some test colors
    std::vector<led_color_t> test_colors = {
        {255, 255, 255},    // White
        {255, 0, 0},        // Red
        {0, 255, 0},        // Green
        {0, 0, 255},        // Blue
        {255, 165, 0}       // Orange
    };

    for(const auto& col : test_colors){
        if(!running) break;

        // Apply color and start transition
        ctrl.SetPlaceholderColor(col);
        ctrl.SetState(LEDState::PLACEHOLDER_TRANSITION);

        // Display for 5 seconds
        for(int i=0;i<50 && running;++i){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "Test finished, exiting.\n";
    return 0;
}

#else
#include <iostream>
int main(){
    std::cout << "LED control only available on aarch64 platform.\n";
    return 0;
}
#endif 