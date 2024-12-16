#include <csignal>
#include <thread>
#include <chrono>
#include "ledcontrol.h"

static volatile std::sig_atomic_t flagStop = 0 ;
extern "C" void ctrl_c_handler(int) { flagStop = 1 ; }

int main(void){
  puts("starting LEDController");
  auto led = new  LEDController();


   while(!flagStop){
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  puts("ctrl+c, stopping");
  delete led;
  return 0;
}
