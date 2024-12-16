#pragma once

#ifdef __aarch64__
#include <thread>
#include <queue>
#include <mutex>
#include <cassert>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <condition_variable>

#include "spi.h"


//https://jetsonhacks.com/nvidia-jetson-agx-orin-gpio-header-pinout/
//https://controllerstech.com/ws2812-leds-using-spi/

#define LED_COUNT 61
#define WS2812B_SPI_SPEED 2500000
#define WS2812B_HIGH 0b11100000  //  WS2812 "1"
#define WS2812B_LOW  0b10000000  //  WS2812 "0"

struct led_color_t {
    uint8_t r,g,b;

    led_color_t operator+(const led_color_t& other) const {
        return { static_cast<uint8_t>(std::min(255, r + other.r)), static_cast<uint8_t>(std::min(255, g + other.g)), static_cast<uint8_t>(std::min(255, b + other.b)) };
    }
    led_color_t operator-(const led_color_t& other) const {
        return { static_cast<uint8_t>(std::max(0, r - other.r)), static_cast<uint8_t>(std::max(0, g - other.g)), static_cast<uint8_t>(std::max(0, b - other.b)) };
    }
    led_color_t operator*(float scalar) const {

        return {
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r * scalar))),
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g * scalar))),
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b * scalar)))
        };
    }
    led_color_t operator/(float scalar) const {
        return {
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r / scalar))),
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g / scalar))),
            static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b / scalar)))
        };
    }

};

inline void encode_color(uint8_t r, uint8_t g, uint8_t b, char* buffer) {
    for (int i = 0; i < 8; i++) {
        buffer[i] = (g & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
        buffer[8 + i] = (r & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
        buffer[16 + i] = (b & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
    }
}
inline void encode_color(const led_color_t& c, char* buffer) {
    for (int i = 0; i < 8; i++) {
        buffer[i] = (c.g & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
        buffer[8 + i] = (c.r & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
        buffer[16 + i] = (c.b & (1 << (7 - i))) ? WS2812B_HIGH : WS2812B_LOW;
    }
}



struct led_action_t{
    enum action_type{
        ACTION_DEFAULT = 0,
        ACTION_NOTIFY,
        ACTION_HOLD,
        ACTION_SET
    };
    led_action_t() : type(ACTION_DEFAULT) {}
    led_action_t(action_type type) : type(type) {}
    led_action_t(action_type type, float duration, led_color_t feature_color =  { 20, 245, 255 }) : type(type), duration(duration), feature_color(feature_color) {}
    action_type type; 
    float duration;
    led_color_t feature_color;

    std::chrono::_V2::system_clock::time_point start_time;
    //would be cool if this had a std::function that let you pass custom effects something
    //actually this is a good use for abstract class/OO for effects bc they need state....



};

/*
needs effect things (see dylans notebook for what this means lol)

needs global brightness modifier 


*/

#define DEFAULT_COLOR {128, 128, 128}

class LEDController
{
public:
    LEDController() : spi(WS2812B_SPI_SPEED) {
        if(spi.state == SPI_OPEN){
            off();
            control_thread = std::thread(&LEDController::run, this);
        }
        else puts("ledcontrol failed to init SPI");
    }
    ~LEDController(){
        shutdown();
    }



private:
    spi_t spi;
    std::thread control_thread;
    std::atomic_bool should_run{true};

    std::array<led_color_t, LED_COUNT> leds;
    static_assert(LED_COUNT * 24 < SPI_BUFFER_SIZE );
    



    inline void update_leds(){
        char tx[LED_COUNT * 24] = {0};
        for (int j = 0; j < LED_COUNT; j++) {
                encode_color(this->leds[j], &tx[j * 24]);
        }
        if(!spi.transfer(tx, sizeof(tx))) {
            //damn that sucks
            puts("SPI transfer failed");
        }
        usleep(50);
    };

    inline void set_all(const led_color_t& color, bool no_update = false){
        for(int i = 0; i < LED_COUNT; ++i)
            leds[i] = color;
        if(!no_update) update_leds();
    }
    

    inline void off(){
        memset(leds.data(), 0, sizeof(led_color_t) * LED_COUNT);
        update_leds();
    }
    void run();
    void shutdown(){
        if(spi.state == SPI_OPEN) off();
        should_run.store(false);
        if(control_thread.joinable())
            control_thread.join();
        puts("LEDController cleanly shutdown");
    }
  
};






#endif
