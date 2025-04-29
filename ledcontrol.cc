#ifdef __aarch64__

#include "ledcontrol.h"


#include <random>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#define M_PI_F		((float)(M_PI))	
#define RAD2DEG( x )  ( (float)(x) * (float)(180.f / M_PI_F) )
#define DEG2RAD( x )  ( (float)(x) * (float)(M_PI_F / 180.f) )
#define RAD2DEGNUM 57.295779513082f

uint8_t generate_random_uint8() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);
    return dis(gen);
}

led_color_t generate_random_color() {
    uint8_t r = generate_random_uint8();
    uint8_t g = generate_random_uint8();
    uint8_t b = generate_random_uint8();

    int avg = (r + g + b) / 3;
    if(avg < 50) {
        return generate_random_color();
    }
    return {r,g,b};
}


using LEDArray = std::array<led_color_t, LED_COUNT>;

//1, 8, 12, 16, 24
const int ring_sizes[] = {1, 8, 12, 16, 24};
const float ring_incs[] = {0.f, 45.f, 30.f, 22.5f, 15.f};


inline float ringunit(int ring, float mul){
    return ((ring_incs[ring] - 0.6) * mul);
}

void LEDController::buildLUT() {
    int idx = 0;
    for(int ring = 0; ring < 5; ++ring) {
        int count = ring_sizes[ring];
        for(int i = 0; i < count; ++i) {
            float theta = (ring == 0)
                        ? 0.0f
                        : DEG2RAD((360.0f / count) * i);
            led_lut[idx++] = polar_t{ theta, static_cast<float>(ring) };
        }
    }
}

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
            led = {125,125,125};// generate_random_color();
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


    }


    void set_led(float angle_deg, int radius, led_color_t color){
        auto [ring, led] = polar_to_ring(angle_deg, radius);
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
        auto [ring, led] = polar_to_ring(coords);
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
};

struct animLED{
    polar_t origin;
    led_color_t color;

   // animLED(polar_t origin, led_color_t color) : origin(origin), color(color) {}
    //animLED() : color({0,0,0}) {}
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
        led_layouts.push_back({DEG2RAD(0.f), 0.0f});
        
        // LINE OF 3 DOWN CENTER
        led_layouts.push_back({DEG2RAD(0.f), -1.0f});
        led_layouts.push_back({DEG2RAD(0.f), 1.0f});
        if(origin.r <= 2.0f)
            led_layouts.push_back({DEG2RAD(0.f), 2.0f});
        if(size == 1) {
            init_display_leds();
            return;
        }

        // LEFT AND RIGHT OF CENTER
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r), -1.f)), 0.0f});
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r), 1.f)), 0.0f});   

        // LEFT AND RIGHT TOP
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, -1.f)), 1.0f});
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, 1.f)), 1.0f});   
        if(origin.r <= 2.0f){
            led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) + 2, -1.f)), 2.0f});
            led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) + 2, 1.f)), 2.0f});   
        }
        
        // LEFT AND RIGHT BOTTOM
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) - 1, -1.f)), -1.0f});
        led_layouts.push_back({DEG2RAD(ringunit(static_cast<int>(origin.r) + 1, 1.f)), -1.0f});   
        if(size == 2) {
            init_display_leds();
            return;
        }
        
        for(int i = 3; i <= size; ++i){
            int ii = i - 2;
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(static_cast<int>(origin.r) + j, ii * -1.f);
                led_layouts.push_back({DEG2RAD(angle), static_cast<float>(j)});
            }
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(static_cast<int>(origin.r) + j, ii * 1.f);
                led_layouts.push_back({DEG2RAD(angle), static_cast<float>(j)});
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
        : base_color(base_color), min_color(min_color), max_size(std::clamp(size,1,4))
    {
        // build LED lookup ------------------------------------------------------
        SetOrigin(0.f, 0);
        leds.reserve(1 + LED_COUNT);

        // Store all LEDs with their exact positions
        leds.push_back({ {0.f,0.f}, base_color });           // centre pixel always lit

        for (int r = 1; r <= max_size; ++r) {
            for (int i = 0; i < ring_sizes[r]; ++i) {
                float angle = DEG2RAD(ringunit(r,i));
                leds.push_back({ { angle, float(r) }, min_color });
            }
        }

        start_time = std::chrono::high_resolution_clock::now();
        last_frame = start_time;
    }

    // --------------------------------------------------------------------------
    void Update() override {
        auto now = std::chrono::high_resolution_clock::now();

        // ~100 fps cap ----------------------------------------------------------
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame).count() < 10)
            return;
        last_frame = now;

        // -------- fluid continuous motion timebase ------------------------------
        constexpr float full_cycle_ms = 6000.0f;      // total time for one complete cycle
        
        // Calculate normalized time position in cycle [0.0 - 1.0]
        float t_norm = fmodf(float(std::chrono::duration_cast<std::chrono::milliseconds>
                                 (now - start_time).count()), full_cycle_ms) / full_cycle_ms;
        
        // Use sine wave for smooth continuous oscillation (values between 0 and 1)
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

//helper for ang diff
static float angularDifference(float a, float b) {
    float d = fabs(a - b);
    d = fmod(d, 2.0f * M_PI_F);
    return (d > M_PI_F) ? (2.0f * M_PI_F - d) : d;
}


void LEDController::run_dormant() {
    static std::unique_ptr<LEDMatrix> matrix   = std::make_unique<LEDMatrix>();
    static Glow dormGlow(4, { 40,120,255 }, { 1,2,3 });   // size, base, min


    // clear back-buffer
    matrix->Clear(leds);

    // animate & draw
    dormGlow.Update();
    dormGlow.Draw(matrix.get());

    // push to framebuffer & hardware
    matrix->Update(leds);
    update_leds();

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // ~100 fps
}

void LEDController::run_active() {
    static std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    static bool initialized = false;
    static std::vector<std::unique_ptr<Animatable>> scene;
    static std::vector<HSV> orbHSV;
    static std::vector<float> sigma;
    static std::vector<float> I;
    static float t = 270.f;
    
    // One-time initialization
    if (!initialized) {
        initialized = true;
        update_leds();
        puts("initializing active state");
        
        // Initialize the scene with three orbs
        scene.push_back(std::make_unique<Orb>(4, led_color_t{245,245,245}, polar_t{0.f, 3.0f})); //white orb
        scene.push_back(std::make_unique<Orb>(4, led_color_t{40, 120, 255}, polar_t::Degrees(120.f, 3))); //blue orb
        scene.push_back(std::make_unique<Orb>(4, led_color_t{240, 50, 105}, polar_t::Degrees(240.f, 3))); //red orb
        
        // Parameters for all orbs (must match scene.push_back order)
        orbHSV = {
            {245.0f, 0.8f, 1.0f},   // white-ish
            { 40.0f, 0.8f, 1.0f},   // blue
            {240.0f, 0.8f, 1.0f}    // magenta
        };
        sigma = { 1.0f, 1.0f, 1.0f };
        I = { 0.7f, 0.7f, 0.7f };
    }
    
    // Clear the matrix for this frame
    matrix->Clear(leds);

    // Update all animations
    for(auto& anim : scene){
        anim->Update();
    }
    
    // Spiral animation logic
    static bool spiralTriggered = false;
    static uint64_t spiralStartUs = 0;
    const uint64_t spiralDurationUs = 2000000; // 2 seconds
    
    // Get orb positions
    auto c0 = dynamic_cast<Orb*>(scene[0].get())->GetOrigin();
    auto c1 = dynamic_cast<Orb*>(scene[1].get())->GetOrigin();
    auto c2 = dynamic_cast<Orb*>(scene[2].get())->GetOrigin();
    
    auto sep = [&](const polar_t &a, const polar_t &b){
        // Euclid dist in LED units
        float dθ = angularDifference(a.theta, b.theta);
        float r̄  = (a.r + b.r) * 0.5f;
        float Δr  = a.r - b.r;
        return sqrtf((dθ*r̄)*(dθ*r̄) + Δr*Δr);
    };
    
    float sep01 = sep(c0, c1), sep12 = sep(c1, c2);
    
    uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::high_resolution_clock::now() - scene[0]->start
                   ).count();
    
    // Check if orbs are close enough to trigger spiral
    if (!spiralTriggered && sep01 < 0.25f && sep12 < 0.25f) {
        spiralTriggered = true;
        spiralStartUs = nowUs;
        printf("Fusion! starting spiral\n");
    }
    
    // Apply spiral animation if triggered
    if (spiralTriggered) {
        uint64_t dt = nowUs - spiralStartUs;
        float tNorm = std::min(1.0f, dt / float(spiralDurationUs));
        float e = easeInOut(tNorm);
        
        // Apply r(t) = mix(r_start, 0, e)
        for (auto &anim : scene) {
            Orb *orb = dynamic_cast<Orb*>(anim.get());
            auto C = orb->GetOrigin();
            float startR = 3.0f;              // initial ring
            float newR = mixf(startR, 0.0f, e);
            
            C.r = newR;                       // float radius
            orb->SetOrigin(C);
        }
    }
    
    // Debug output (first frame only)
    static bool once=true;
    if(once){
        once=false;
        for (size_t i = 0; i < scene.size(); i++) {
            auto orbPtr = dynamic_cast<Orb*>(scene[i].get());
            polar_t C = orbPtr->GetOrigin();
            printf("Orb %zu at r=%.1f,θ=%.1f°\n", 
                   i, C.r, RAD2DEG(C.theta));
            led_color_t test = hsv2rgb(orbHSV[i])*(I[i]*1.0f);
            printf("  RGB=(%u,%u,%u)\n", test.r, test.g, test.b);
        }
    }

    // Zero-out LED framebuffer
    for(int i = 0; i < LED_COUNT; ++i)
        leds[i] = {0,0,0};

    // Calculate contribution of each orb to each LED
    for(size_t o = 0; o < scene.size(); ++o) {
        auto orbPtr = dynamic_cast<Orb*>(scene[o].get());
        polar_t C = orbPtr->GetOrigin();

        // For each LED
        for(int i = 0; i < LED_COUNT; ++i) {
            polar_t P = led_lut[i];

            float dθ = angularDifference(P.theta, C.theta);
            float r̄ = (P.r + C.r) * 0.5f;
            float Δr = P.r - C.r;
            float d2 = (dθ * r̄)*(dθ * r̄) + (Δr * Δr);
            float F = std::exp(-d2 / (2 * sigma[o] * sigma[o]));

            // Orb-specific base color
            led_color_t base = hsv2rgb(orbHSV[o]);

            // Scaled contribution
            led_color_t contrib = base * (I[o] * F);

            // Additive blend (operator+ clamps at 255)
            leds[i] = leds[i] + contrib;
        }
    }

    // Push to hardware
    update_leds();
    
    // Update angle for next frame
    t -= 10.f;
    if(t < 0.f) t = 359.9f;
    if(t > 360.f) t = 0.1f;
}

void LEDController::run_respond_to_user() {
    static std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    // Orange glow - vibrant orange color with very subtle red undertone
    static Glow respondGlow(5, led_color_t{255, 140, 0}, led_color_t{10,5,0});  // Orange color

    // Clear the matrix
    matrix->Clear(leds);

    // Update the animation
    respondGlow.Update();
    respondGlow.Draw(matrix.get());

    // Push to framebuffer & hardware
    matrix->Update(leds);
    update_leds();

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // ~100 fps
}

void LEDController::run_error() {
    static std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    // Orange glow - vibrant orange color with very subtle red undertone
    static Glow respondGlow(5, led_color_t{255, 0, 0}, led_color_t{10,0,0});  // Red color

    // Clear the matrix
    matrix->Clear(leds);

    // Update the animation
    respondGlow.Update();
    respondGlow.Draw(matrix.get());

    // Push to framebuffer & hardware
    matrix->Update(leds);
    update_leds();

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // ~100 fps
}

void LEDController::run_prompt() {
    static std::unique_ptr<LEDMatrix> prompt_matrix = std::make_unique<LEDMatrix>();
    
    // Static variables for animation state
    static float angle = 0.0f;
    static const float ROTATION_SPEED = 300.0f; // degrees per second - increased by 5x
    static auto last_update = std::chrono::high_resolution_clock::now();
    
    // Create our single large orb with constant parameters
    static polar_t orb_position = polar_t::Degrees(angle, 3); // Radius 3

    // Clear matrix
    prompt_matrix->Clear(leds); 

    // Update rotation angle based on elapsed time (for smooth constant movement)
    auto now = std::chrono::high_resolution_clock::now();
    float elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
    last_update = now;
    
    // Apply rotation at constant speed (degrees per frame)
    angle += (ROTATION_SPEED / 1000.0f) * elapsed_ms;
    if (angle >= 360.0f) angle -= 360.0f;
    
    // Update orb position
    orb_position = polar_t::Degrees(angle, 3);
    
    // Define HSV color and lighting parameters
    HSV orbHSV = {0.0f, 0.0f, 1.0f}; 
    float sigma = 3.5f;                  // blur radius (larger = bigger orb)
    float intensity = 1.2f;              // global brightness (increased for better contrast)
    
    // First set all LEDs to 50% white (background)
    for (int i = 0; i < LED_COUNT; ++i) {
        leds[i] = led_color_t{0, 0, 0}; // 50% white (128/255 = 0.5 or 50%)
    }
    
    // Calculate orb influence for each LED
    std::vector<float> orb_influence(LED_COUNT, 0.0f);
    float max_influence = 0.0f;
    
    for (int i = 0; i < LED_COUNT; ++i) {
        polar_t p = led_lut[i];
        
        // Calculate distance metrics
        float dθ = angularDifference(p.theta, orb_position.theta);
        float r̄ = (p.r + orb_position.r) * 0.5f;
        float Δr = p.r - orb_position.r;
        
        // Squared distance (for Gaussian)
        float d2 = (dθ * r̄)*(dθ * r̄) + (Δr * Δr);
        
        // Gaussian falloff function
        float F = std::exp(-d2 / (2 * sigma * sigma));
        orb_influence[i] = F;
        
        if (F > max_influence) max_influence = F;
    }
    
    // Apply colors based on influence
    for (int i = 0; i < LED_COUNT; ++i) {
        float F = orb_influence[i];
        
        // Calculate orb color with intensity falloff
        led_color_t orb_color = hsv2rgb(orbHSV) * (intensity * F);
        
        // Blend with background based on influence
        // Higher influence = more orb color, less background
        float blend_factor = std::min(1.0f, F * 2.0f); // Scale influence for stronger effect
        
        // Weighted blend between background white and orb color
        leds[i].r = static_cast<uint8_t>((1.0f - blend_factor) * 128 + blend_factor * orb_color.r);
        leds[i].g = static_cast<uint8_t>((1.0f - blend_factor) * 128 + blend_factor * orb_color.g);
        leds[i].b = static_cast<uint8_t>((1.0f - blend_factor) * 128 + blend_factor * orb_color.b);
    }
    
    // Push to hardware
    update_leds();
    
    // Smooth timing
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void LEDController::run() {
    update_leds();
    puts("enter loop");
    
    while(should_run.load(std::memory_order_relaxed)) {
        LEDState current_state = state.load(std::memory_order_relaxed);
        
        switch(current_state) {
            case LEDState::DORMANT:
                run_dormant();
                break;
            case LEDState::RESPOND_TO_USER:
                run_respond_to_user();
                break;
            case LEDState::PROMPT:
                run_prompt();
                break;
            default:
                run_active();
                break;
        }
    }
    
    puts("exit loop");
}


#endif
