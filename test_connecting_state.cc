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

int main(){
    std::cout << "Starting CONNECTING state test (Ctrl+C to quit)\n";
    signal(SIGINT, signal_handler);

    LEDController controller;

    // Switch to CONNECTING state immediately
    controller.SetState(LEDState::CONNECTING);

    while(running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Test finished.\n";
    return 0;
}

#else
#include <iostream>
int main(){
    std::cout << "LED control only supported on aarch64 platform.\n";
    return 0;
}
#endif 