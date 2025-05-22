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

class WiFiDemo {
private:
    std::unique_ptr<LEDMatrix> matrix;
    WiFiSymbol wifi_symbol;
    spi_t spi;
    std::array<led_color_t, LED_COUNT> leds;
    
    void update_leds() {
        char tx[LED_COUNT * 24] = {0};
        for (int j = 0; j < LED_COUNT; j++) {
            encode_color(this->leds[j], &tx[j * 24]);
        }
        if(!spi.transfer(tx, sizeof(tx))) {
            puts("SPI transfer failed");
        }
        usleep(5);
    }

public:
    WiFiDemo() : spi(WS2812B_SPI_SPEED), wifi_symbol(270.0f) {
        matrix = std::make_unique<LEDMatrix>();
        memset(leds.data(), 0, sizeof(led_color_t) * LED_COUNT);
        update_leds();
    }
    
    void demo_directions() {
        std::cout << "=== WiFi Symbol Direction Demo ===\n";
        
        float directions[] = {270.0f, 0.0f, 90.0f, 180.0f};  // Up, Right, Down, Left
        const char* names[] = {"UP (transmitting upward)", "RIGHT", "DOWN", "LEFT"};
        const led_color_t colors[] = {
            {40, 120, 255},   // Blue
            {255, 100, 0},    // Orange  
            {0, 255, 100},    // Green
            {255, 0, 150}     // Magenta
        };
        
        for (int i = 0; i < 4 && running; i++) {
            std::cout << "Direction: " << names[i] << " (" << directions[i] << " degrees)\n";
            
            wifi_symbol.SetDirection(directions[i]);
            
            // Animate building the symbol
            for (int element = 0; element <= wifi_symbol.GetElementCount() && running; element++) {
                matrix->Clear(leds);
                
                if (element < wifi_symbol.GetElementCount()) {
                    wifi_symbol.Draw(matrix.get(), colors[i], element + 1);
                }
                
                matrix->Update(leds);
                update_leds();
                std::this_thread::sleep_for(std::chrono::milliseconds(600));
            }
            
            // Hold complete symbol for a moment
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    }
    
    void demo_rotation() {
        std::cout << "\n=== WiFi Symbol Rotation Demo ===\n";
        std::cout << "Continuously rotating WiFi symbol...\n";
        
        const led_color_t rotation_color = {100, 255, 200};  // Cyan
        float angle = 0.0f;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            if (elapsed >= 15) break;  // Run for 15 seconds
            
            matrix->Clear(leds);
            wifi_symbol.SetDirection(angle);
            wifi_symbol.Draw(matrix.get(), rotation_color);
            matrix->Update(leds);
            update_leds();
            
            angle += 3.0f;  // Rotate 3 degrees per frame
            if (angle >= 360.0f) angle -= 360.0f;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
};

int main(){
    std::cout << "WiFi Symbol Demonstration Program\n";
    std::cout << "This demonstrates the new WiFi symbol that uses the full circular LED matrix\n";
    std::cout << "Features:\n";
    std::cout << "- Can point in any direction (0-360 degrees)\n";
    std::cout << "- Uses relative coordinates for easy positioning\n";
    std::cout << "- Progressive animation (center dot + 3 arcs)\n";
    std::cout << "- Uses proper LED ring spacing for each arc\n\n";
    std::cout << "Press Ctrl+C to exit at any time...\n\n";
    
    signal(SIGINT, signal_handler);

    try {
        WiFiDemo demo;
        
        // Demo 1: Different directions
        demo.demo_directions();
        
        if (running) {
            // Demo 2: Rotation
            demo.demo_rotation();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nWiFi symbol demo finished.\n";
    return 0;
}

#else
#include <iostream>
int main(){
    std::cout << "LED control only supported on aarch64 platform.\n";
    return 0;
}
#endif 