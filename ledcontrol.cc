#ifdef __aarch64__

#include "ledcontrol.h"


#include <random>
#include <exception>
#include <stdexcept>
#include <cmath>

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

struct polar_t{
    float theta;
    int r;
    // polar_t() : theta(0.f), r(0) {}
    // polar_t(float angle_rad, int radius) : theta(angle_rad), r(radius) {}
    static polar_t Degrees(float angle_deg, int radius){
        return {DEG2RAD(angle_deg), radius};
    }
    void rotate_deg(float deg){
        theta += DEG2RAD(deg);
        normalize(); //SUS!
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
        r = std::abs(r);
        if(r > 4) r = 4;
        return *this;
    }
    operator std::pair<float, int>() const {
        return {angle_deg(), r};
    }
    polar_t operator+ (const polar_t& other) const {
        return {theta + other.theta, r + other.r};
    }
    polar_t operator- (const polar_t& other) const {
        return {theta - other.theta, r - other.r};
    }
    polar_t operator* (float scalar) const {
        return {theta * scalar, static_cast<int>((float)r * scalar)};
    }
    polar_t operator/ (float scalar) const {
        return {theta / scalar, static_cast<int>((float)r / scalar)};
    }

    bool operator==(const polar_t& other) const {
        //use epsilon!
        const float eps = 0.01f;
        return std::abs(theta - other.theta) < eps && r == other.r;
    }

};


void LEDController::buildLUT() {
    int idx = 0;
    for(int ring = 0; ring < 5; ++ring) {
        int count = ring_sizes[ring];
        for(int i = 0; i < count; ++i) {
            float theta = (ring == 0)
                        ? 0.0f
                        : DEG2RAD((360.0f / count) * i);
            led_lut[idx++] = polar_t{ theta, ring };
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
        if(std::abs(coords.r) > 4) return {0xffff, 0xffff};
        if(coords.r == 0) return {0, 0};
        coords.normalize();

        int ring = std::abs(coords.r); 
       
        float led_idx_f = (coords.theta / (2.f * M_PI_F) ) * ( static_cast<float>(ring_sizes[ring]));
       
        int led = static_cast<int>(std::round(led_idx_f) );
        if(led != 0 && led == ring_sizes[ring]) led = 0;

        //printf("angle: %f, radius: %d, ring: %d, led: %d\n", RAD2DEG(coords.theta), coords.r, ring, led); 
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

    void set_led(polar_t coords, led_color_t color){
        auto [ring, led] = polar_to_ring(coords);
        if(ring == 0xffff || led == 0xffff){
            printf("Invalid coords: %f, %d\n", coords.theta, coords.r);
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
       this->origin = polar_t::Degrees(angle_deg, radius);
    }
    polar_t GetOrigin() const {
        return origin;
    }
protected:
    std::vector<animLED> leds;
    polar_t origin;

    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

class Orb : public Animatable{
    public:



    Orb(int size = 3,  led_color_t base_color = {245,245,245}, polar_t origin = {0.f,3}) {
       
        this->origin = origin;
        float mul = 1.f;
        const float mulmul = 0.85f;
        //ORIGIN
        leds.push_back({{DEG2RAD(0.f), 0}, base_color});
        //LINE OF 3 DOWN CENTER
        leds.push_back({{DEG2RAD(0.f), -1}, base_color});
        leds.push_back({{DEG2RAD(0.f), 1}, base_color});
        if(origin.r <= 2)
            leds.push_back({{DEG2RAD(0.f), 2}, base_color});
        if(size == 1) return;


        //LEFT AND RIGHT OF CENTER
        leds.push_back({{DEG2RAD(ringunit(origin.r, -1.f)), 0}, base_color});
        leds.push_back({{DEG2RAD(ringunit(origin.r, 1.f)), 0}, base_color});   


        //LEFT AND RIGHT TOP
        leds.push_back({{DEG2RAD(ringunit(origin.r + 1, -1.f)), 1}, base_color});
        leds.push_back({{DEG2RAD(ringunit(origin.r + 1, 1.f)), 1}, base_color});   
        if(origin.r <= 2){
            leds.push_back({{DEG2RAD(ringunit(origin.r + 2, -1.f)), 2}, base_color});
            leds.push_back({{DEG2RAD(ringunit(origin.r + 2, 1.f)), 2}, base_color});   
        }
        //LEFT AND RIGHT BOTTOM
        leds.push_back({{DEG2RAD(ringunit(origin.r - 1,  -1.f)), -1}, base_color});
        leds.push_back({{DEG2RAD(ringunit(origin.r + 1, 1.f)), -1}, base_color});   
        if(size == 2) return;

        
        for(int i = 3; i <= size; ++i){
            int ii = i - 2;
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(origin.r + j,  ii * -1.f); // -11.5f * ii;
               // if(j == 1) angle = -4.f * ii;
                leds.push_back({{DEG2RAD(angle), j}, base_color * mul});
               // printf("Orb[%d]: %f, %d\n", i, -20.f * ii, j);
            }
            for(int j = -1; j <= 1; ++j){
                float angle = ringunit(origin.r + j, ii * 1.f); // -11.5f * ii;
              //  float angle = 11.f * ii;
                //if(j == 1) angle = 8.f * ii;
                leds.push_back({{DEG2RAD(angle), j}, base_color * mul});
            //     printf("Orb[%d]: %f, %d\n", i, 25.f * ii, j);
            }
            mul *= mulmul;
        }
        last_update = std::chrono::high_resolution_clock::now();
    }

    void Update() override {
        uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - last_update).count();
        if(us < 2000) return; //smoothing this would be nice
        last_update = std::chrono::high_resolution_clock::now();

       uint64_t delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        
        
        float scale = exp((rot_speed * 0.0001f) / max_speed);
        const float mul = 0.99f * scale;
        const float imul = (1.f / mul) * scale;

        const float base_speed = 1.001f;
 
        const uint64_t hold_time = 2500;
        if(!speed_up && delta_ms > (hold_time + last_speedchange) && std::abs(rot_speed) > base_speed){
              
            rot_speed *= (imul);
            //printf("not speed up: %f \n", rot_speed);
        }
        else if(speed_up && delta_ms > (hold_time + last_speedchange) && std::abs(rot_speed) < max_speed){
          
            rot_speed *= (mul);
            //printf("speed up %f \n", rot_speed);
        }
        if((rot_speed <= base_speed || std::abs(rot_speed) > max_speed) && delta_ms > (hold_time + last_speedchange)){
            rot_speed = (speed_up) ? (base_speed + 0.01f) : (max_speed - 0.01f);
            last_speedchange = delta_ms;
            speed_up = !speed_up;
            if(speed_up) m *= -1.f;
           // printf("speed change: %f, %d, %lu\n", rot_speed, speed_up, last_speedchange);
        }

       this->origin.rotate_deg(rot_speed * m);
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
    std::chrono::time_point<std::chrono::high_resolution_clock> last_update ;
    float m = 1.f;
};



class Glow : public Animatable{
    public:
    Glow(int size = 3,  led_color_t base_color = {255,255,255}, led_color_t min_color = {0,0,0}) : base_color(base_color), min_color(min_color), max_size(size) {
        if(size < 3) throw std::invalid_argument("Glow size must be at least 3");
        this->SetOrigin(0.f, 0);
        leds.push_back({{DEG2RAD(0.f), 0}, {0,0,0}});

        for(int i = 0; i < size; ++i){
            for(int j = 0; j < ring_sizes[i]; ++j){
                leds.push_back({{DEG2RAD(ringunit(i, j)), i},{0,0,0}});
            }
        }
        current_size = 0.f;
        inc = 0.015f;
        last_update = std::chrono::high_resolution_clock::now();
    }
    void set_ring(int ring, led_color_t color){
        int idx = 1;
         for(int i = 0; i < ring; ++i){
            idx += ring_sizes[i];
        }
        for(int i = 0; i < ring_sizes[ring]; ++i){
            leds[idx + i].color = color;
        }
    }
    void Update() override {
        for(auto& led : leds){
            led.color = min_color;
        }
     //   leds[0].color = base_color; 
        uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - last_update).count();
        if(us < 800) return; //smoothing this would be nice
        last_update = std::chrono::high_resolution_clock::now();
        led_color_t dif = base_color - min_color;
        for(int i = 0; i < std::round(current_size + 0.5f); ++i){
            float mul = ((current_size - i) / (float)max_size) + (min_color.r / 255.f);
            //if(i < 2) mul = std::max(1.f, mul * (3 - i));
            set_ring(i , base_color * mul); 
        }
        current_size += inc;
        if(current_size >= (float)max_size) { inc = inc * -1.f; current_size = ((float)(max_size) - 0.01); pulses++; }
        if(current_size <= 0.f) { inc = inc * -1.f; current_size = 0.001f; pulses++; }
      //  printf("Glow: %f - inc: %f\n", current_size, inc);

      // this->origin.rotate_deg(1.f);
    }
    void Draw(LEDMatrix* matrix) override {
        for(auto& led : leds){
            auto real_pos = origin + led.origin;
            matrix->set_led(real_pos, led.color);
        }
    }

    led_color_t base_color;
    led_color_t min_color;
    int max_size;
    float current_size;
    float inc;
    int pulses = 0;
     std::chrono::time_point<std::chrono::high_resolution_clock> last_update ;
};


//helper for ang diff
static float angularDifference(float a, float b) {
    float d = fabs(a - b);
    d = fmod(d, 2.0f * M_PI_F);
    return (d > M_PI_F) ? (2.0f * M_PI_F - d) : d;
}

void LEDController::run(){
    
    std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    update_leds();



   // set_all({255,0,0});
    puts("enter loop");

    auto set_line = [&](float angle_deg, led_color_t color){
        for(int i = 0; i < 5; ++i){
            matrix->set_led(angle_deg, i, color);
        }
    };

    std::vector<std::unique_ptr<Animatable>> scene;
   // scene.push_back(std::make_unique<Glow>(4, led_color_t{255, 255, 255}, led_color_t{25,25,25}));  //pulse


    scene.push_back(std::make_unique<Orb>(4, led_color_t{245,245,245}, polar_t{0.f, 3})); //white orb
   // scene.back()->SetOrigin(0.f, 2);
    scene.push_back(std::make_unique<Orb>(4, led_color_t{40, 120, 255}, polar_t::Degrees(120.f, 3))); //blue orb
   // scene.back()->SetOrigin(120.f, 2);

    scene.push_back(std::make_unique<Orb>(4, led_color_t{240, 50, 105}, polar_t::Degrees(240.f, 3))); //red orb
  //  scene.back()->SetOrigin(240.f, 2);

    
    // set_line(0.f, {255,0,0});
    // set_line(90.f, {0,255,0});
    // set_line(180.f, {0,0,255});
    // set_line(270.f, {255,255,0});
    float t = 270.f;
    int i = 0;
    // while(should_run.load(std::memory_order_relaxed)){       
    //    matrix->Clear(leds);
    
    //    for(auto& anim : scene){
    //        anim->Update();
    //        anim->Draw(matrix.get());
    //    }


    polar_t center = dynamic_cast<Orb*>(scene[0].get())->GetOrigin();

        HSV orbHSV     = { 240.0f, 0.8f, 1.0f };  // blue-ish
        float sigma    = 1.0f;                    // blur radius
        float intensity= 0.7f;                    // global brightness

        for(int i = 0; i < LED_COUNT; ++i) {
            auto p = led_lut[i];
            float dθ = angularDifference(p.theta, center.theta);
            float r̄ = (p.r + center.r) * 0.5f;
            float Δr = p.r - center.r;
            float d2 = (dθ * r̄)*(dθ * r̄) + (Δr * Δr);
            float F  = std::exp(-d2 / (2 * sigma * sigma));

            // compute RGB and write into your matrix
            led_color_t c = hsv2rgb(orbHSV) * (intensity * F);
            matrix->set_led(p, c);
        }

        matrix->Update(leds);
        update_leds();
       // set_line(t, {200,25,205});
    //     t -= 10.f;
    //     if(t < 0.f) t = 359.9f;
    //     if(t > 360.f) t = 0.1f;
    //     matrix->Update(leds); 
    //    // if(i >= LED_COUNT) i = 0;

    //     update_leds();
       // std::this_thread::sleep_for(std::chrono::microseconds(750));
    }
    
    puts("exit loop");
}

#endif