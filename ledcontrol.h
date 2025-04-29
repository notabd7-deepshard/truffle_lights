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
#include <algorithm>
#include <random>
#include <exception>
#include <stdexcept>
#include <array>
#include <cstring>
#include <unistd.h>
#include "spi.h"


#define LED_COUNT 61

struct LEDPxF {                      // linear-float 0‥1  (premultiplied)
    float r, g, b;                   // colour energy
    float v;                         // brightness scalar
};
using LEDFrame = std::array<LEDPxF, LED_COUNT>;

template <typename T>
inline T lerp(T a, T b, float t) {
    return a + (b - a) * t;
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

#define M_PI_F		((float)(M_PI))	
#define RAD2DEG( x )  ( (float)(x) * (float)(180.f / M_PI_F) )
#define DEG2RAD( x )  ( (float)(x) * (float)(M_PI_F / 180.f) )
#define RAD2DEGNUM 57.295779513082f

//https://jetsonhacks.com/nvidia-jetson-agx-orin-gpio-header-pinout/
//https://controllerstech.com/ws2812-leds-using-spi/

#define WS2812B_SPI_SPEED 2500000
#define WS2812B_HIGH 0b11100000  //  WS2812 "1"
#define WS2812B_LOW  0b10000000  //  WS2812 "0"

//1, 8, 12, 16, 24
const int ring_sizes[] = {1, 8, 12, 16, 24};
const float ring_incs[] = {0.f, 45.f, 30.f, 22.5f, 15.f};

inline float ringunit(int ring, float mul){
    return ((ring_incs[ring] - 0.6f) * mul);
}

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

uint8_t generate_random_uint8();
led_color_t generate_random_color();

using LEDArray = std::array<led_color_t, LED_COUNT>;

//helper for ang diff
static float angularDifference(float a, float b) {
    float d = fabs(a - b);
    d = fmod(d, 2.0f * M_PI_F);
    return (d > M_PI_F) ? (2.0f * M_PI_F - d) : d;
}

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
    PROMPT = 1 << 3,      // Value 8 when prompt state should run
    ERROR = 1 << 4       // Value 16 when error state should run
};


class LEDRingBase{
public:
    LEDRingBase(int index) : index(index) {
    }

    virtual void Update(LEDArray& leds) = 0;

    virtual const int Count() const = 0;
    const auto Index() const {
        return index;
    }
    virtual void clear(led_color_t clr = {0,0,0}) = 0;
    virtual void set_led(int idx, led_color_t color) = 0;
protected:
    const int index;
};

template <std::size_t N>
class LEDRing : public LEDRingBase {
public:
    LEDRing(int index) : LEDRingBase(index), led_count(leds.size()) {
        start_idx = 0;
        for(int i = 0; i < index; ++i)
            start_idx += ring_sizes[i];
        end_idx = start_idx + N;
        assert(end_idx <= LED_COUNT);

        //flip indices 
        int temp = start_idx;
        start_idx = LED_COUNT - end_idx;
        end_idx = LED_COUNT - temp;
        printf("Ring[%d] %d: %d - %d\n", index, led_count, start_idx, end_idx);
        
        for(auto& led : leds)
            led = {125,125,125};
    }

    const int Count() const override { return led_count; }


    void Update(LEDArray& all) override {
        for(int i = start_idx; i < end_idx; ++i){
            if(index == 4) all[i] = leds[i - start_idx] * 0.37f;
            else if(index == 3) all[i] = leds[i - start_idx] * 1.66f;
            else all[i] = leds[i - start_idx];
        }

    }


    void set_led(int idx, led_color_t color) override {
        if(idx < 0 || idx >= N) throw std::out_of_range("LED index out of range");
        if(color)
            leds[idx] = leds[idx] + color;
        else 
            leds[idx] = color;
    }

    void clear(led_color_t clr = {0,0,0}) override {
        for(auto& led : leds)
            led = clr;
    }
protected:
    const int led_count;
    int start_idx;
    int end_idx;
    std::array<led_color_t, N> leds; 

    
};


class LEDMatrix {
public:
    LEDMatrix() {
        this->rings = {
            std::make_unique<LEDRing<1>>(0),
            std::make_unique<LEDRing<8>>(1),
            std::make_unique<LEDRing<12>>(2),
            std::make_unique<LEDRing<16>>(3),
            std::make_unique<LEDRing<24>>(4),
        };
        set_all({0,0,0});
    }

    inline void sampleToFrame(LEDFrame& out) const
    {
        for (int i = 0; i < LED_COUNT; ++i) {
            const led_color_t c = outLedCache_[i];   // see § B below
            out[i].r = c.r / 255.f;
            out[i].g = c.g / 255.f;
            out[i].b = c.b / 255.f;
            out[i].v = 1.f;
        }
    }

    void Clear(LEDArray& leds, led_color_t clr = {0,0,0}){
        for(auto& ring : rings){
            ring->clear(clr);
        }
        this->Update(leds);
    }
    //returns ring index, led index within ring 
    std::pair<int, int> polar_to_ring(float angle_deg, int radius){
        if(std::abs(radius) > 4) return {0xffff, 0xffff};
        if(radius == 0) return {0, 0};
        if(angle_deg >= 360.f){
            while(angle_deg >= 360.f) angle_deg -= 360.f;
        }
        if(angle_deg < 0.f){
            while(angle_deg < 0.f) angle_deg += 360.f;
        }

        int ring = abs(radius); 
        float angle = DEG2RAD(angle_deg);
        float led_idx_f = (angle / (2.f * M_PI_F) ) * (ring_sizes[ring] - 1);
       
        int led = static_cast<int>(std::round(led_idx_f));

      //  printf("angle: %f, radius: %d, ring: %d, led: %d\n", angle_deg, radius, ring, led); 
        return {ring, led};
    }
    std::pair<int, int> polar_to_ring(polar_t coords){
        if(std::abs(coords.r) > 4.0f) return {0xffff, 0xffff};
        if(coords.r < 0.5f) return {0, 0};  // Consider values less than 0.5 as center
        coords.normalize();

        int ring = static_cast<int>(std::round(coords.r)); 
        ring = std::min(4, std::max(0, ring)); // Clamp to valid range
       
        float led_idx_f = (coords.theta / (2.f * M_PI_F) ) * ( static_cast<float>(ring_sizes[ring]));
       
        int led = static_cast<int>(std::round(led_idx_f) );
        if(led != 0 && led == ring_sizes[ring]) led = 0;

        //printf("angle: %f, radius: %f, ring: %d, led: %d\n", RAD2DEG(coords.theta), coords.r, ring, led); 
        return {ring, led};
    }
    std::pair<int, int> grid_to_ring(int x, int y) {
        if(x == 0 && y == 0) return {0, 0};
        if(std::abs(x) > 4 || std::abs(y) > 4) return {0xffff, 0xffff};

        float theta = atan2(y, x);

        int radius = static_cast<int>(std::round(std::sqrt(x * x + y * y)));

      
        return polar_to_ring(RAD2DEG(theta), radius);
    }

    void Update(LEDArray& leds) {

        for(auto& ring : rings){
            ring->Update(leds);
        }

        outLedCache_ = leds;        // ← keeps last colours for sampling
    }


    void set_led(float angle_deg, int radius, led_color_t color){
        std::pair<int, int> result = polar_to_ring(angle_deg, radius);
        int ring = result.first;
        int led = result.second;
        if(ring == 0xffff || led == 0xffff) {
            throw std::out_of_range("Invalid coords: " + std::to_string(angle_deg) + ", " + std::to_string(radius));
        }
        rings[ring]->set_led(led, color);
    }

    void set_led(float angle_deg, float radius, led_color_t color){
        polar_t coords = {DEG2RAD(angle_deg), radius};
        set_led(coords, color);
    }

    void set_led(polar_t coords, led_color_t color){
        std::pair<int, int> result = polar_to_ring(coords);
        int ring = result.first;
        int led = result.second;
        if(ring == 0xffff || led == 0xffff){
            printf("Invalid coords: %f, %f\n", RAD2DEG(coords.theta), coords.r);
            return;
        }
        rings[ring]->set_led(led, color);
    }

    void set_all(led_color_t color){
        for(auto& ring : rings){
            for(int i = 0; i < ring->Count(); ++i){
                ring->set_led(i, color);
            }
        }
    }

    
protected:
    std::array<std::unique_ptr<LEDRingBase>, 5> rings;
    std::array<led_color_t, LED_COUNT> outLedCache_;
};

struct animLED{
    polar_t origin;
    led_color_t color;

};

class Animatable{
public:
    Animatable() {
        start = std::chrono::high_resolution_clock::now();
    }
    virtual void Update() = 0;
    virtual void Draw(LEDMatrix* matrix) = 0;
    virtual ~Animatable() = default;

    void SetOrigin(float angle_deg, int radius){
       this->origin = polar_t::Degrees(angle_deg, static_cast<float>(radius));
    }
    
    // new overload to set a full polar directly
    void SetOrigin(const polar_t &p){
        origin = p;
    }
    
    polar_t GetOrigin() const {
        return origin;
    }
    
    // Make start time accessible for fusion calculations
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    
protected:
    std::vector<animLED> leds;
    polar_t origin;
};

class Orb : public Animatable{
    public:
    Orb(int size = 3, led_color_t base_color = {245,245,245}, polar_t origin = {0.f, 3.0f}) {
        this->origin = origin;
        this->base_color = base_color;
        this->min_color = {0, 0, 0};
        this->size = std::min(size, 5);
        
        // Store original layout of LEDs
        float mul = 1.f;
        const float mulmul = 0.85f;
        
        // ORIGIN
        led_layouts.push_back(polar_t{DEG2RAD(0.f), 0.0f});
        
        // LINE OF 3 DOWN CENTER
        led_layouts.push_back(polar_t{DEG2RAD(0.f), -1.0f});
        led_layouts.push_back(polar_t{DEG2RAD(0.f), 1.0f});
        if(origin.r <= 2.0f)
            led_layouts.push_back(polar_t{DEG2RAD(0.f), 2.0f});
        if(size == 1) {
            init_display_leds();
            return;
        }

        // LEFT AND RIGHT OF CENTER
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r), -1.f)), 0.0f});
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r), 1.f)), 0.0f});   

        // LEFT AND RIGHT TOP
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, -1.f)), 1.0f});
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, 1.f)), 1.0f});   
        if(origin.r <= 2.0f){
            led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) + 2, -1.f)), 2.0f});
            led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) + 2, 1.f)), 2.0f});   
        }
        
        // LEFT AND RIGHT BOTTOM
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) - 1, -1.f)), -1.0f});
        led_layouts.push_back(polar_t{DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, 1.f)), -1.0f});   
        if(size == 2) {
            init_display_leds();
            return;
        }
        
        for(int i = 3; i <= size; ++i){
            int ii = i - 2;
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(static_cast<int>(origin.r) + j, ii * -1.f);
                led_layouts.push_back(polar_t{DEG2RAD(angle), static_cast<float>(j)});
            }
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(static_cast<int>(origin.r) + j, ii * 1.f);
                led_layouts.push_back(polar_t{DEG2RAD(angle), static_cast<float>(j)});
            }
            mul *= mulmul;
        }
        
        init_display_leds();
        last_update = std::chrono::high_resolution_clock::now();
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void init_display_leds() {
        // Initialize display LEDs with same positions but zero color
        leds.clear();
        for(auto& layout : led_layouts) {
            // Create animLED with the layout position and zero color
            animLED led;
            led.origin = layout;
            led.color = {0, 0, 0};
            leds.push_back(led);
        }
    }

    void Update() override {
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_update).count();
        if(us < 2000) return; // Rate limiting
        last_update = now;

        // Handle rotation
        uint64_t delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        
        float scale = exp((rot_speed * 0.0001f) / max_speed);
        const float mul = 0.99f * scale;
        const float imul = (1.f / mul) * scale;
        const float base_speed = 1.001f;
        const uint64_t hold_time = 2500;
        
        if(!speed_up && delta_ms > (hold_time + last_speedchange) && std::abs(rot_speed) > base_speed){
            rot_speed *= (imul);
        }
        else if(speed_up && delta_ms > (hold_time + last_speedchange) && std::abs(rot_speed) < max_speed){
            rot_speed *= (mul);
        }
        
        if((rot_speed <= base_speed || std::abs(rot_speed) > max_speed) && delta_ms > (hold_time + last_speedchange)){
            rot_speed = (speed_up) ? (base_speed + 0.01f) : (max_speed - 0.01f);
            last_speedchange = delta_ms;
            speed_up = !speed_up;
            if(speed_up) m *= -1.f;
        }
        
        this->origin.rotate_deg(rot_speed * m);
        
        // Handle pulsing expansion/contraction
        // Use time-based animation for smooth expanding/contracting (4 second cycle)
        constexpr float pulse_cycle_ms = 4000.0f;
        constexpr float hold_ms = 600.0f;
        const float cycle_ms = pulse_cycle_ms + 2 * hold_ms;
        
        float t_ms = std::fmod(std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - start_time).count(), cycle_ms);
        
        // Calculate the current expansion factor
        if (t_ms < hold_ms) {
            // Hold at minimum
            current_expansion = 0.0f;
            is_expanding = true;
        }
        else if (t_ms < hold_ms + pulse_cycle_ms/2) {
            // Expanding phase
            current_expansion = (t_ms - hold_ms) / (pulse_cycle_ms/2);
            is_expanding = true;
        }
        else if (t_ms < hold_ms + pulse_cycle_ms/2 + hold_ms) {
            // Hold at maximum
            current_expansion = 1.0f;
            is_expanding = false;
        }
        else {
            // Contracting phase
            float x = t_ms - (hold_ms + pulse_cycle_ms/2 + hold_ms);
            current_expansion = 1.0f - (x / (pulse_cycle_ms/2));
            is_expanding = false;
        }
        
        // Apply easing function for smoother animation
        current_expansion = 0.5f - 0.5f * std::cos(current_expansion * M_PI);
        
        // Define edge width for smooth transitions
        const float edge_width = 0.3f;
        
        // Apply expansion/contraction to LED colors
        // Calculate the effective size based on expansion
        float effective_size = 1.0f + current_expansion * (size - 1);
        
        // Update LED colors based on their distance from center
        for (size_t i = 0; i < leds.size(); ++i) {
            polar_t pos = led_layouts[i];
            float distance = std::sqrt(pos.r * pos.r + 0.2f); // Distance from center
            
            float brightness;
            if (is_expanding) {
                // Expanding phase
                if (distance < effective_size - edge_width) {
                    // Inside the expanding wave
                    brightness = 1.0f;
                }
                else if (distance < effective_size + edge_width) {
                    // At the edge of the expanding wave - smooth falloff
                    float t = (effective_size + edge_width - distance) / (edge_width * 2);
                    t = std::max(0.0f, std::min(1.0f, t));
                    brightness = t;
                }
                else {
                    // Outside the expanding wave
                    brightness = 0.0f;
                }
            }
            else {
                // Contracting phase
                if (distance < effective_size - edge_width) {
                    // Inside the contracting wave
                    brightness = 1.0f;
                }
                else if (distance < effective_size + edge_width) {
                    // At the edge of the contracting wave - smooth falloff
                    float t = (effective_size + edge_width - distance) / (edge_width * 2);
                    t = std::max(0.0f, std::min(1.0f, t));
                    brightness = t;
                }
                else {
                    // Outside the contracting wave
                    brightness = 0.0f;
                }
            }
            
            // Apply distance falloff for more natural looking light
            float falloff = 1.0f - (distance * 0.1f);
            falloff = std::max(0.0f, falloff);
            
            // Apply color with brightness
            leds[i].color = base_color * (brightness * falloff);
        }
    }
    
    void Draw(LEDMatrix* matrix) override {
        for(auto& led : leds){
            auto real_pos = origin + led.origin;
            matrix->set_led(real_pos, led.color);
        }
    }
    
    float max_speed = 270.f;
    float rot_speed = max_speed;
    bool speed_up = false;
    uint64_t last_speedchange = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    float m = 1.f;
    
    // Parameters for expansion/contraction
    float current_expansion = 0.0f;
    bool is_expanding = true;
    int size;
    led_color_t base_color;
    led_color_t min_color;
    std::vector<polar_t> led_layouts; // Store the original layouts
};



// ────────────────── Smoother centre-out / centre-in "breathing" ────────────────
class Glow : public Animatable {
public:
    /*  size        – number of animated rings   (1‥4)          (not centre)
        base_color  – colour of the wave front
        min_color   – background "off" tint                                      */
    Glow(int size = 4,
         led_color_t base_color = { 40,120,255 },   // bright blue
         led_color_t min_color  = { 1, 2, 3 })      // nearly off (very subtle tint)
        : base_color(base_color), min_color(min_color), max_size(std::max(1, std::min(size, 4)))
    {
        // build LED lookup ------------------------------------------------------
        SetOrigin(0.f, 0);
        leds.reserve(1 + LED_COUNT);

        // Store all LEDs with their exact positions
        leds.push_back({ {0.f,0.f}, base_color });           // centre pixel always lit

        for (int r = 1; r <= max_size; ++r) {
            for (int i = 0; i < ring_sizes[r]; ++i) {
                float angle = DEG2RAD(ringunit(r,i));
                polar_t pos = { angle, static_cast<float>(r) };
                animLED led;
                led.origin = pos;
                led.color = min_color;
                leds.push_back(led);
            }
        }

        start_time = std::chrono::high_resolution_clock::now();
        last_frame = start_time;
    }

    void Update() override {
        auto now = std::chrono::high_resolution_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame).count() < 10)
            return;
        last_frame = now;

        constexpr float full_cycle_ms = 6000.0f;      // total time for one complete cycle
        
        float t_norm = fmodf(float(std::chrono::duration_cast<std::chrono::milliseconds>
                                 (now - start_time).count()), full_cycle_ms) / full_cycle_ms;
        
        float sin_t = (std::sin(t_norm * 2.0f * M_PI_F - M_PI_F/2.0f) + 1.0f) * 0.5f;
        
        // Calculate exact radius using smoothed sine wave
        float r_wave = sin_t * max_size;                // current wave radius [0,max]
        
        // Determine direction (expanding/contracting) from derivative of sine
        bool expanding = std::cos(t_norm * 2.0f * M_PI_F - M_PI_F/2.0f) > 0;

        // Set center always lit with base blue color (first LED)
        leds[0].color = base_color;
        
        // Calculate gaussian sigma based on wave speed
        // Smaller sigma = sharper edge, larger sigma = more spread
        const float base_sigma = 0.80f; // More moderate spread for smooth transitions
        
        // Transition width (distance from wave front where brightness changes)
        float transition_width = base_sigma;
        
        // Update each LED individually based on distance from center
        for (size_t i = 1; i < leds.size(); ++i) {
            // Get LED's distance from center
            float led_radius = leds[i].origin.r;
            
            // Calculate distance from wave front
            float dist_from_wave = led_radius - r_wave;
            
            // Simple, clean gaussian falloff for smooth transition
            float brightness;
            
            if (expanding) {
                if (dist_from_wave > 0) {
                    // LED is ahead of wave front - smooth falloff
                    brightness = expf(-dist_from_wave * dist_from_wave / (2.0f * transition_width * transition_width));
                } else {
                    // LED is inside the wave - fully lit
                    brightness = 1.0f;
                }
            } else {
                if (dist_from_wave < 0) {
                    // Inside contracting wave - fully lit
                    brightness = 1.0f;
                } else {
                    // Outside the wave - smooth falloff
                    brightness = expf(-dist_from_wave * dist_from_wave / (2.0f * transition_width * transition_width));
                }
            }
            
            // Apply basic smoothstep for natural transition
            if (brightness < 1.0f && brightness > 0.01f) {
                brightness = brightness * brightness * (3.0f - 2.0f * brightness);
            }
            
            // Ensure brightness is in [0,1]
            brightness = std::max(0.0f, std::min(1.0f, brightness));
            
            // Blend between min_color and base_color based on brightness
            leds[i].color = min_color + ((base_color - min_color) * brightness);
        }
    }

    void Draw(LEDMatrix* m) override {
        for (auto& p : leds) m->set_led(origin + p.origin, p.color);
    }

private:
    led_color_t base_color, min_color;
    int max_size;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time, last_frame;
};


class TransitionEngine {
public:
    void begin(const LEDFrame& from, const LEDFrame& to,
               float duration_ms = 1200.f)
    {
        A = from;  B = to;  T = duration_ms;
        t0 = std::chrono::high_resolution_clock::now();
        active = true;
    }

    // blends into out[], returns true while active
    bool blend(LEDFrame& out)
    {
        if (!active) return false;

        float t = std::chrono::duration<float, std::milli>(
                      std::chrono::high_resolution_clock::now() - t0).count() / T;
        if (t >= 1.f) { out = B; active = false; return false; }

        // cubic ease-in-out
        float e = (t < .5f) ? 4.f*t*t*t
                            : 1.f - powf(-2.f*t + 2.f, 3.f) / 2.f;

        for (int i = 0; i < LED_COUNT; ++i) {
            out[i].v = 1.0f;                       // keep full brightness
            out[i].r = lerpf(A[i].r, B[i].r, e);
            out[i].g = lerpf(A[i].g, B[i].g, e);
            out[i].b = lerpf(A[i].b, B[i].b, e);
        }
        return true;
    }

private:
    LEDFrame A, B;
    std::chrono::high_resolution_clock::time_point t0;
    float  T       = 0.f;
    bool   active  = false;
};


class LEDController
{
public:
    LEDController();
    ~LEDController(){
        shutdown();
    }

    //void SetState(LEDState s) { state.store(s, std::memory_order_relaxed); }

    void SetState(LEDState s)
    {
        pendingState_ = s;           // don't flip immediately
    }

    void renderSnapshot(LEDState s, LEDFrame& out);

    void run_dormant(bool commit = true);
    void run_respond_to_user(bool commit = true);
    void run_prompt(bool commit = true);
    void run_active(bool commit = true);
    void run_error(bool commit = true);

private:
    spi_t spi;
    std::array<polar_t, LED_COUNT> led_lut;
    std::thread control_thread;
    std::atomic_bool should_run{true};

    std::array<led_color_t, LED_COUNT> leds;
    static_assert(LED_COUNT * 24 < SPI_BUFFER_SIZE );
    
    std::atomic<LEDState> state{LEDState::DORMANT};

    TransitionEngine transition_;
    LEDFrame         curF_{}, nextF_{}, blendedF_{};
    LEDState         pendingState_ = LEDState::DORMANT;
    bool             inTransition_ = false;

    std::unique_ptr<LEDMatrix> liveMatrix_ = std::make_unique<LEDMatrix>();

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
};






#endif
