#ifdef __aarch64__

#include "ledcontrol.h"


#include <random>

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

// 24
// 16
// 12
// 8
//1

using LEDArray = std::array<led_color_t, LED_COUNT>;


const int ring_sizes[] = {1, 8, 12, 16, 24};

class LEDRingBase{
public:
    LEDRingBase(int index) : index(index) {
    }

    virtual void Update(LEDArray& leds) = 0;

    virtual const int Count() const = 0;
    const auto Index() const {
        return index;
    }
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
            led = generate_random_color();
    }

    const int Count() const override { return led_count; }


    void Update(LEDArray& all) override {
        for(int i = start_idx; i < end_idx; ++i)
            all[i] = leds[i - start_idx];
        
        for(auto& led : leds){
            led = generate_random_color();
            if(index == 4) led = led * 0.2f;
        }
    }

protected:
    const int led_count;
    int start_idx;
    int end_idx;
    std::array<led_color_t, N> leds; 

    
};



void LEDController::run(){
    
    std::array<std::unique_ptr<LEDRingBase>, 5> rings = {
        std::make_unique<LEDRing<1>>(0),
        std::make_unique<LEDRing<8>>(1),
        std::make_unique<LEDRing<12>>(2),
        std::make_unique<LEDRing<16>>(3),
        std::make_unique<LEDRing<24>>(4),
    };

   // set_all({255,0,0});
  //  update_leds();
    puts("enter loop");
    int i = 0;
    while(should_run.load(std::memory_order_relaxed)){       
        set_all({0,0,0}, true);

        for(auto& ring : rings){
            ring->Update(leds);
        }
       // leds[i++] = {0,255,0};
       // if(i >= LED_COUNT) i = 0;

        update_leds();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    puts("exit loop");
}


// void LEDController::run(){
//     bool even_up = false;
//     bool odd_up = true;
//     puts("LED");
//     set_all({0,0,0});   
//   //      for(int i = (LED_COUNT - 4); i < LED_COUNT; ++i)
//     //            leds[i] = {240,240,245};
//    update_leds();
//    std::this_thread::sleep_for(std::chrono::milliseconds(10));
// puts("running");
// int base = 0; //(24 + 16); //+ 8);

// int special = base;
// int special2 = LED_COUNT - ((LED_COUNT - base)  / 2);
// // 24
// 16
// 12
// 8
//1


// while(should_run.load()) {
//         for(int l = (base); l < LED_COUNT; ++l) {
//             if(l > ( base + 15) && l < (LED_COUNT - 5 - 9)) continue;
//              // if(l  % 2 ==  0 && 0){
//             //     if(leds[l].r <= 5) even_up = true;
//             //     if(leds[l].r >= 125) even_up = false; 

//             //     if(even_up) leds[l] = {++leds[l].r, 0, ++leds[l].b};
//             //     else leds[l] = {--leds[l].r, 0, --leds[l].b};
//             // } 
//             // else{
//                 if(leds[l].r <= 15) odd_up = true;
//                 if(leds[l].r >= 90) odd_up = false; 

//                 if(odd_up) leds[l] = {++leds[l].r,  ++leds[l].g, ++leds[l].b};
//                 else leds[l] = {--leds[l].r,  --leds[l].g, --leds[l].b};

                
//             //} 
//         }

//         leds[special++] = {100,100,105};
//                 if(special >= LED_COUNT) special = base;
//         leds[special2++] = {100,100,105};
//        if(special2 >= LED_COUNT) special2 = base;


// //	for(int i = (LED_COUNT - 5); i < LED_COUNT; ++i)
// //		leds[i] = {240,240,245};

//         update_leds();

//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }


// }



/*
        for(int i = 0; i < LED_COUNT; ++i)
            leds[i] = {0,0,0};
        for(int i = 0; i < 48; ++i)
            leds[i] = {255,0,0};

        for(int i = 48; i < (48 + 24); ++i)
            leds[i] = {0,255,0};

        for(int i = (48 + 24); i < (48 + 24 + 16); ++i)
            leds[i] = {0,0,255};

        for(int i = (48 + 24 + 16); i < (48 + 24 + 16 + 12); ++i)
            leds[i] = {0,255,255};
        for(int i = (48 + 24 + 16 + 12); i < (108); ++i)
            leds[i] = {255,0,255};
        leds[LED_COUNT - 1] = {255,255,255};

*/
//0-48
// 24
// 16
// 12
// 8
//1








#endif 
