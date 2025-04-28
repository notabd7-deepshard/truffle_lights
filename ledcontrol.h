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
#include <cmath>
#include "spi.h"

#define M_PI_F		((float)(M_PI))	
#define RAD2DEG( x )  ( (float)(x) * (float)(180.f / M_PI_F) )
#define DEG2RAD( x )  ( (float)(x) * (float)(M_PI_F / 180.f) )

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
    bool operator==(const led_color_t& other) const {
        return r == other.r && g == other.g && b == other.b;
    }

    bool operator!=(const led_color_t& other) const {
        return !(*this == other);
    }
    bool operator>(const led_color_t& other) const {
        return r > other.r && g > other.g && b > other.b;
    }
    bool operator<(const led_color_t& other) const {
        return r < other.r && g < other.g && b < other.b;
    }

    bool operator>=(const led_color_t& other) const {
        return r >= other.r && g >= other.g && b >= other.b;
    }
    bool operator<=(const led_color_t& other) const {
        return r <= other.r && g <= other.g && b <= other.b;
    }
    bool operator<(uint8_t val) const {
        return r < val && g < val && b < val;
    }
    bool operator>(uint8_t val) const {
        return r > val && g > val && b > val;
    }
    bool operator==(uint8_t val) const {
        return r == val && g == val && b == val;
    }
    
    operator bool() const {
        return r || g || b;
    }
};

struct HSV {
    float h; // [0,360)
    float s; // [0,1]
    float v; // [0,1]
};

inline led_color_t hsv2rgb(const HSV& hsv) {
    float H = hsv.h;
    float S = hsv.s;
    float V = hsv.v;
    float C = V * S;
    float X = C * (1 - std::fabs(fmod(H/60.0f, 2) - 1));
    float m = V - C;

    float r1, g1, b1;
    if      (H <  60) { r1 = C; g1 = X; b1 = 0; }
    else if (H < 120) { r1 = X; g1 = C; b1 = 0; }
    else if (H < 180) { r1 = 0; g1 = C; b1 = X; }
    else if (H < 240) { r1 = 0; g1 = X; b1 = C; }
    else if (H < 300) { r1 = X; g1 = 0; b1 = C; }
    else              { r1 = C; g1 = 0; b1 = X; }

    uint8_t R = static_cast<uint8_t>(std::round((r1 + m) * 255));
    uint8_t G = static_cast<uint8_t>(std::round((g1 + m) * 255));
    uint8_t B = static_cast<uint8_t>(std::round((b1 + m) * 255));
    return { R, G, B };
}

// —— Stage 6: time functions ——
// t ∈ [0,1] ease-in/out
inline float easeInOut(float t) {
    return 0.5f * (1.0f - cosf(M_PI_F * t));
}
// linear interpolate
inline float mixf(float a, float b, float t) {
    return a + (b - a) * t;
}

struct polar_t{
    float theta;
    float r;    // now float so we can interpolate smoothly
    
    static polar_t Degrees(float angle_deg, int radius){
        return {DEG2RAD(angle_deg), static_cast<float>(radius)};
    }
    void rotate_deg(float deg){
        theta += DEG2RAD(deg);
        normalize();
    }
    float angle_deg() const {
        return RAD2DEG(theta);
    }
    void set_angle_deg(float angle_deg){
        theta = DEG2RAD(angle_deg);
    }
    polar_t& normalize(){
        while(theta >= 2.f * M_PI_F) theta -= 2.f * M_PI_F;
        while(theta < 0.f) theta += 2.f * M_PI_F;
        r = fabsf(r);
        if(r > 4.0f) r = 4.0f;
        return *this;
    }
    operator std::pair<float, int>() const {
        return {angle_deg(), static_cast<int>(r)};
    }
    polar_t operator+ (const polar_t& other) const {
        return {theta + other.theta, r + other.r};
    }
    polar_t operator- (const polar_t& other) const {
        return {theta - other.theta, r - other.r};
    }
    polar_t operator* (float scalar) const {
        return {theta * scalar, r * scalar};
    }
    polar_t operator/ (float scalar) const {
        return {theta / scalar, r / scalar};
    }

    bool operator==(const polar_t& other) const {
        //use epsilon!
        const float eps = 0.01f;
        return std::abs(theta - other.theta) < eps && std::abs(r - other.r) < eps;
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

// ==== Added: LED system states ====
enum class LEDState : uint8_t {
    DORMANT = 1,         // Value 1 when dormant should run
    ACTIVE = 1 << 1,     // Value 2 when active should run
    RESPOND_TO_USER = 1 << 2,  // Value 4 when respond to user should run
    PROMPT = 1 << 3      // Value 8 when prompt state should run
};

class LEDController
{
public:
    LEDController() : spi(WS2812B_SPI_SPEED) {
        buildLUT();
        if(spi.state == SPI_OPEN){
            off();
            control_thread = std::thread(&LEDController::run, this);
        }
        else puts("ledcontrol failed to init SPI");
    }
    ~LEDController(){
        shutdown();
    }

    //void SetState(LEDState s) { state.store(s, std::memory_order_relaxed); }

    void SetState(LEDState state) { this->state.store(state, std::memory_order_relaxed); }

private:
    spi_t spi;
    std::array<polar_t, LED_COUNT> led_lut;
    std::thread control_thread;
    std::atomic_bool should_run{true};

    std::array<led_color_t, LED_COUNT> leds;
    static_assert(LED_COUNT * 24 < SPI_BUFFER_SIZE );
    
    std::atomic<LEDState> state{LEDState::DORMANT};

    void buildLUT();

    inline void update_leds(){
        char tx[LED_COUNT * 24] = {0};
        for (int j = 0; j < LED_COUNT; j++) {
                encode_color(this->leds[j], &tx[j * 24]);
        }
        if(!spi.transfer(tx, sizeof(tx))) {
            //damn that sucks
            puts("SPI transfer failed");
        }
        usleep(5);
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
  
    void run_dormant();
    void run_respond_to_user();
    void run_prompt();
};






#endif
