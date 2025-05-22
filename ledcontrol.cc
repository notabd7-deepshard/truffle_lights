#ifdef __aarch64__

#include "ledcontrol.h"
#include <random>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <optional>

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

void LEDController::run_transition(LEDMatrix* matrix) {
    static std::optional<TransitionSpiral> transition;

    // Set initial HSV values for testing if not already set
    if (currentHSV[0].h == 0 && currentHSV[0].s == 0 && currentHSV[0].v == 0) {
        currentHSV = {
            HSV{180.0f, 0.9f, 1.0f},    // Cyan
            HSV{300.0f, 0.9f, 1.0f},    // Magenta
            HSV{60.0f, 0.9f, 1.0f}      // Yellow
        };
        printf("Initialized default HSV values\n");
    }

    // Check if a transition is pending
    if (pendingNextState && !transition) {
        printf("Starting transition to state %d\n", static_cast<int>(*pendingNextState));
        
        // Create the transition object with the current and next HSV values
        transition.emplace(currentHSV, nextHSV);
    }
    
    // If there's an active transition, update and draw it
    if (transition) {
        // Update transition animation
        transition->Update();
        
        // Clear LEDs and draw the transition effect using the improved Draw method
        // that takes leds and led_lut directly for Gaussian blending
        transition->DrawTransition(matrix, leds, led_lut);
        
        // Push to hardware
        update_leds();
        
        // If transition is complete, finalize state change
        if (transition->finished()) {
            // Commit state change
            currentHSV = nextHSV;
            LEDState newState = *pendingNextState;
            pendingNextState.reset();
            transition.reset();
            
            // Actually set the new state once transition is complete
            printf("Transition finished, setting state to %d\n", static_cast<int>(newState));
            state.store(newState, std::memory_order_relaxed);
        }
    } else if (pendingNextState) {
        printf("Warning: pendingNextState is set but no transition started!\n");
    }
}

void LEDController::run_transition_test() {
    // Set up test values
    std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    
    // Create scene with the same structure as in run() but with different colors
    std::vector<std::unique_ptr<Animatable>> scene;
    
    // Create three orbs with different colors (positioned at radius 3.0, not 5.0)
    scene.push_back(std::make_unique<Orb>(4, led_color_t{220, 50, 220}, polar_t{0.f, 3.0f})); // Purple orb
    scene.push_back(std::make_unique<Orb>(4, led_color_t{50, 220, 220}, polar_t::Degrees(120.f, 3))); // Cyan orb
    scene.push_back(std::make_unique<Orb>(4, led_color_t{220, 220, 50}, polar_t::Degrees(240.f, 3))); // Yellow orb
    
    // Set parameters for all orbs (matching the structure in run())
    // Use std::array instead of std::vector for HSV values to match RequestState parameter type
    std::array<HSV, 3> srcHSV = {
        HSV{300.0f, 0.9f, 1.0f},   // Purple-ish
        HSV{180.0f, 0.9f, 1.0f},   // Cyan
        HSV{60.0f, 0.9f, 1.0f}     // Yellow
    };
    
    // Initialize currentHSV array from srcHSV
    currentHSV = srcHSV;
    
    // Target HSV values for transition
    std::array<HSV, 3> targetHSV = {
        HSV{120.0f, 0.9f, 1.0f},   // Green
        HSV{0.0f, 1.0f, 1.0f},     // Red
        HSV{240.0f, 0.8f, 1.0f}    // Blue
    };
    
    std::vector<float> sigma = {1.0f, 1.0f, 1.0f};  // Gaussian blur radius
    std::vector<float> I = {0.7f, 0.7f, 0.7f};      // Intensity
    
    // Run test for a few seconds
    for (int frame = 0; frame < 1000 && should_run.load(std::memory_order_relaxed); frame++) {
        matrix->Clear(leds);
        
        // Manually update orbs
        for (auto& anim : scene) {
            anim->Update();
        }
        
        // Reset LED buffer for this frame
        for (int i = 0; i < LED_COUNT; ++i) {
            leds[i] = {0, 0, 0};
        }
        
        // Render each orb with Gaussian distribution
        for (size_t o = 0; o < scene.size(); ++o) {
            auto orbPtr = dynamic_cast<Orb*>(scene[o].get());
            polar_t C = orbPtr->GetOrigin();
            
            // Make sure the coordinate is valid (normalized)
            C.normalize();
            
            // For each LED
            for (int i = 0; i < LED_COUNT; ++i) {
                polar_t P = led_lut[i];
                
                float dθ = angularDifference(P.theta, C.theta);
                float r̄ = (P.r + C.r) * 0.5f;
                float Δr = P.r - C.r;
                float d2 = (dθ * r̄)*(dθ * r̄) + (Δr * Δr);
                float F = std::exp(-d2 / (2 * sigma[o] * sigma[o]));
                
                // Orb-specific base color
                led_color_t base = hsv2rgb(srcHSV[o]);
                
                // Scaled contribution
                led_color_t contrib = base * (I[o] * F);
                
                // Additive blend
                leds[i] = leds[i] + contrib;
            }
        }
        
        // Push to hardware
        update_leds();
        
        // Every 300 frames, swap colors to trigger transition
        if (frame > 0 && frame % 300 == 0) {
            // Use the new RequestState method to initiate transition
            // Alternating between target and original colors
            if (frame % 600 == 0) {
                RequestState(LEDState::ACTIVE, targetHSV);
            } else {
                RequestState(LEDState::ACTIVE, srcHSV);
            }
            
            // Run the transition (will continue until finished)
            while (pendingNextState && should_run.load(std::memory_order_relaxed)) {
                matrix->Clear(leds);
                run_transition(matrix.get());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LEDController::run(){
    
    std::unique_ptr<LEDMatrix> matrix = std::make_unique<LEDMatrix>();
    update_leds();

    // set_all({255,0,0});
    puts("LED controller running main loop");

    auto set_line = [&](float angle_deg, led_color_t color){
        for(int i = 0; i < 5; ++i){
            matrix->set_led(angle_deg, i, color);
        }
    };

    std::vector<std::unique_ptr<Animatable>> scene;
    // scene.push_back(std::make_unique<Glow>(4, led_color_t{255, 255, 255}, led_color_t{25,25,25}));  //pulse

    scene.push_back(std::make_unique<Orb>(4, led_color_t{245,245,245}, polar_t{0.f, 3.0f})); //white orb
    // scene.back()->SetOrigin(0.f, 2);
    scene.push_back(std::make_unique<Orb>(4, led_color_t{40, 120, 255}, polar_t::Degrees(120.f, 3))); //blue orb
    // scene.back()->SetOrigin(120.f, 2);

    scene.push_back(std::make_unique<Orb>(4, led_color_t{240, 50, 105}, polar_t::Degrees(240.f, 3))); //red orb
    // scene.back()->SetOrigin(240.f, 2);

    // —— Stage 4: parameters for all orbs ——  
    // (must match your scene.push_back order)
    std::vector<HSV> orbHSV = {
        {245.0f, 0.8f, 1.0f},   // white-ish
        { 40.0f, 0.8f, 1.0f},   // blue
        {240.0f, 0.8f, 1.0f}    // magenta
    };
    
    // Initialize the currentHSV array from orbHSV
    for (size_t i = 0; i < std::min(orbHSV.size(), currentHSV.size()); i++) {
        currentHSV[i] = orbHSV[i];
    }
    
    std::vector<float> sigma = { 1.0f, 1.0f, 1.0f };
    std::vector<float> I = { 0.7f, 0.7f, 0.7f };
    
    // set_line(0.f, {255,0,0});
    // set_line(90.f, {0,255,0});
    // set_line(180.f, {0,0,255});
    // set_line(270.f, {255,255,0});
    float t = 270.f;
    int i = 0;
    
    // Keep track of state changes
    static LEDState lastState = state.load(std::memory_order_relaxed);
    
    while(should_run.load(std::memory_order_relaxed)){
        // Get current state
        LEDState currentState = state.load(std::memory_order_relaxed);
        
        // Print state changes for debugging
        if (currentState != lastState) {
            printf("STATE CHANGE: %d -> %d\n", 
                  static_cast<int>(lastState), 
                  static_cast<int>(currentState));
            lastState = currentState;
        }
   
        // Check for pending transition first
        if (pendingNextState) {
            // Modified to ensure transition completes fully before continuing
            // Keep running transition updates until the transition is complete
            printf("DEBUG: Starting transition loop for state %d\n", static_cast<int>(*pendingNextState));
            auto transitionStartTime = std::chrono::high_resolution_clock::now();
            int loopCount = 0;
            
            while (pendingNextState && should_run.load(std::memory_order_relaxed)) {
                auto loopStartTime = std::chrono::high_resolution_clock::now();
                
                // REMOVED: matrix->Clear(leds) - This was causing the blank screen during transitions
                // The clearing is now handled inside run_transition
                
                run_transition(matrix.get());
                
                auto loopEndTime = std::chrono::high_resolution_clock::now();
                auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEndTime - loopStartTime).count();
                
                loopCount++;
                if (loopCount % 50 == 0) {  // Log every 50 iterations
                    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEndTime - transitionStartTime).count();
                    printf("DEBUG: Transition loop iteration %d, loop time: %ldms, total time: %ldms\n", 
                           loopCount, loopDuration, totalDuration);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            auto transitionEndTime = std::chrono::high_resolution_clock::now();
            auto totalTransitionTime = std::chrono::duration_cast<std::chrono::milliseconds>(transitionEndTime - transitionStartTime).count();
            printf("DEBUG: Transition complete after %d iterations, total time: %ldms\n", 
                   loopCount, totalTransitionTime);
            
            continue; // Skip normal rendering after transition is complete
        }
       
        // Handle each state
        if(currentState == LEDState::DORMANT){
            run_dormant();
            continue;
        }
        else if(currentState == LEDState::RESPOND_TO_USER){
            run_respond_to_user();
            continue;
        }
        else if(currentState == LEDState::BOOT){
            run_boot();
            continue;
        }
        else if(currentState == LEDState::PROMPT){
            run_prompt();
            continue;
        }
        else if(currentState == LEDState::CONNECTING){
            run_connecting();
            continue;
        }
        else if(currentState == LEDState::PLACEHOLDER_TRANSITION){
            run_placeholder_transition();
            continue;
        }
        
        // Must be in ACTIVE state if we get here
        // Clear the matrix for rendering
        matrix->Clear(leds);

        // 6.1 linear motion
        for(auto& anim : scene){
            anim->Update();
        }
       
        // once-only for triggering spiral
        static bool spiralTriggered = false;
        static uint64_t spiralStartUs = 0;
        const uint64_t spiralDurationUs = 2000000; // 2 seconds
       
        // grab each centre
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
       
        if (!spiralTriggered && sep01 < 0.25f && sep12 < 0.25f) {
            spiralTriggered = true;
            spiralStartUs  = nowUs;
            printf("Fusion! starting spiral\n");
        }
       
        if (spiralTriggered) {
            uint64_t dt = nowUs - spiralStartUs;
            float tNorm = std::min(1.0f, dt / float(spiralDurationUs));
            float e     = easeInOut(tNorm);
           
            // apply r(t) = mix(r_start, 0, e)
            for (auto &anim : scene) {
                Orb *orb = dynamic_cast<Orb*>(anim.get());
                auto  C   = orb->GetOrigin();
                float startR = 3.0f;              // your initial ring
                float newR   = mixf(startR, 0.0f, e);
               
                C.r = newR;                       // float radius
                orb->SetOrigin(C);
            }
        }
       
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
 
        // zero‐out LED framebuffer
        for(int i = 0; i < LED_COUNT; ++i)
            leds[i] = {0,0,0};

        // for each orb…
        for(size_t o = 0; o < scene.size(); ++o) {
            auto orbPtr = dynamic_cast<Orb*>(scene[o].get());
            polar_t C = orbPtr->GetOrigin();

            // for each LED
            for(int i = 0; i < LED_COUNT; ++i) {
                polar_t P = led_lut[i];

                float dθ = angularDifference(P.theta, C.theta);
                float r̄ = (P.r + C.r) * 0.5f;
                float Δr = P.r - C.r;
                float d2 = (dθ * r̄)*(dθ * r̄) + (Δr * Δr);
                float F  = std::exp(-d2 / (2 * sigma[o] * sigma[o]));

                // orb‐specific base color
                led_color_t base = hsv2rgb(orbHSV[o]);

                // scaled contribution
                led_color_t contrib = base * (I[o] * F);

                // additive blend (operator+ clamps at 255)
                leds[i] = leds[i] + contrib;
            }
        }

        // push to hardware
        update_leds();
       
        // set_line(t, {200,25,205});
        t -= 10.f;
        if(t < 0.f) t = 359.9f;
        if(t > 360.f) t = 0.1f;
        // if(i >= LED_COUNT) i = 0;

        // std::this_thread::sleep_for(std::chrono::microseconds(750));
    }
    
    puts("LED controller exiting main loop");
}

void LEDController::run_dormant(){
    static std::unique_ptr<LEDMatrix> dormant_matrix = std::make_unique<LEDMatrix>();
    static Glow dormGlow(5, led_color_t{40, 120, 255}, led_color_t{5,5,10});
    
    // Clear the LED buffer
    dormant_matrix->Clear(leds);
    
    // Update the glow animation timing
    dormGlow.Update();
    
    // Get animation parameters
    float current_size = dormGlow.current_size;
    float max_size = static_cast<float>(dormGlow.max_size);
    float animation_phase = current_size / max_size;  // 0-1 through the cycle
    led_color_t base_color = dormGlow.base_color;
    led_color_t min_color = dormGlow.min_color;
    bool is_expanding = dormGlow.inc > 0;
    
    // Constant dim core intensity to match at beginning and end of cycle
    const float dim_core_intensity = 0.25f;
    
    // Keep track of cycle transition points
    static bool in_transition = false;
    static float transition_progress = 0.0f;
    const float transition_threshold = 0.1f; // Size threshold to consider in transition

    // Detect cycle transition points
    if (current_size < transition_threshold) {
        in_transition = true;
        transition_progress = current_size / transition_threshold;
    } else if (in_transition && current_size >= transition_threshold) {
        in_transition = false;
    }
    
    // CRITICAL: Set center ring first to ensure it's always on regardless of animation phase
    // This guarantees the center never turns off during any state
    dormGlow.set_ring(0, min_color + ((base_color - min_color) * dim_core_intensity));
    
    // For each ring, calculate its brightness
    for (int ring = 0; ring < 5; ring++) {
        // Skip center ring as we've already set it to ensure it's always on
        if (ring == 0) continue;
        
        float intensity = 0.0f;
        
        if (is_expanding) {
            // EXPANSION PHASE: smooth wave of light moving outward
            // Calculate how far the wave has progressed through this ring
            float wave_position = current_size - static_cast<float>(ring);
            
            // Create a window function for smooth transition (0->1->0)
            // Use a smoothstep-like function for more natural transitions
            if (wave_position >= -1.0f && wave_position <= 1.0f) {
                // Smooth transition as wave passes through ring (0->1)
                float t = (wave_position + 1.0f) * 0.5f; // normalize to 0-1
                // Smoothstep function: 3t² - 2t³ (smoother than linear)
                intensity = t * t * (3.0f - 2.0f * t);
            } else if (wave_position > 1.0f) {
                // After wave has passed, maintain a glow that fades with distance
                intensity = 1.0f - std::min(0.5f, (wave_position - 1.0f) * 0.1f);
            }
        } else {
            // CONTRACTION PHASE: smooth wave of darkness moving inward
            
            // During contraction, we want rings to gradually dim from outside to inside
            // Normalize current_size to go from max_size to 0
            float contraction_progress = current_size / max_size;
            
            // Calculate normalized ring position (0 = center, 1 = outermost)
            float ring_position = static_cast<float>(ring) / (max_size - 1.0f);
            
            // Calculate how far the dimming wave has progressed relative to this ring
            // Negative means ring is ahead of the wave (still bright)
            // Positive means wave has passed this ring (dimming/dimmed)
            float dimming_factor = ring_position - contraction_progress;
            
            // Apply a smooth curve for dimming transition
            // We want a value that goes from 1.0 (fully lit) to dim_core_intensity (mostly dimmed)
            if (dimming_factor <= 0.0f) {
                // Ring is ahead of the wave - still fully illuminated
                intensity = 1.0f;
            } else if (dimming_factor > 0.0f && dimming_factor < 0.5f) {
                // Ring is being dimmed by the wave - smooth transition
                // Use cubic ease-out function for gentle dimming start with accelerated falloff
                float t = dimming_factor / 0.5f; // normalize to 0-1
                intensity = 1.0f - (t * t * t); // cubic falloff
            } else {
                // Ring is well behind the wave - maintain a minimal glow
                // that gradually fades to the minimum as we approach the center
                float remaining = 0.15f * (1.0f - std::min(1.0f, (dimming_factor - 0.5f) * 2.0f));
                intensity = remaining;
            }
        }
        
        // Apply natural distance falloff from center
        float distance_falloff = 1.0f - (ring * 0.1f);
        intensity *= distance_falloff;
        
        // Apply easing at extremes of the animation for smooth cycle transition
        if (current_size < 0.5f || current_size > (max_size - 0.5f)) {
            // Ease intensity changes at the start/end of cycle
            float cycle_transition = std::min(current_size, max_size - current_size) * 2.0f;
            float transition_ease = cycle_transition * cycle_transition * 0.25f + 0.75f;
            
            // Apply transition easing where appropriate
            if ((!is_expanding && ring == 4)) {
                intensity *= transition_ease;
            }
        }
        
        // Apply intensity to color without gamma correction
        led_color_t ring_color = min_color + ((base_color - min_color) * intensity);
        
        // Set the calculated color for this ring
        dormGlow.set_ring(ring, ring_color);
    }
    
    // Draw to matrix and update LEDs
    dormGlow.Draw(dormant_matrix.get());
    dormant_matrix->Update(leds);
    update_leds();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void LEDController::run_respond_to_user(){
    static std::unique_ptr<LEDMatrix> respond_matrix = std::make_unique<LEDMatrix>();
    static Glow respondGlow(5, led_color_t{255, 140, 0}, led_color_t{10,5,0});  // Orange color
    
    // Clear the LED buffer
    respond_matrix->Clear(leds);
    
    // Update the glow animation timing
    respondGlow.Update();
    
    // Get animation parameters
    float current_size = respondGlow.current_size;
    float max_size = static_cast<float>(respondGlow.max_size);
    float animation_phase = current_size / max_size;  // 0-1 through the cycle
    led_color_t base_color = respondGlow.base_color;
    led_color_t min_color = respondGlow.min_color;
    bool is_expanding = respondGlow.inc > 0;
    
    // Constant dim core intensity to match at beginning and end of cycle
    const float dim_core_intensity = 0.25f;
    
    // Keep track of cycle transition points
    static bool in_transition = false;
    static float transition_progress = 0.0f;
    const float transition_threshold = 0.1f; // Size threshold to consider in transition

    // Detect cycle transition points
    if (current_size < transition_threshold) {
        in_transition = true;
        transition_progress = current_size / transition_threshold;
    } else if (in_transition && current_size >= transition_threshold) {
        in_transition = false;
    }
    
    // CRITICAL: Set center ring first to ensure it's always on regardless of animation phase
    // This guarantees the center never turns off during any state
    respondGlow.set_ring(0, min_color + ((base_color - min_color) * dim_core_intensity));
    
    // For each ring, calculate its brightness
    for (int ring = 0; ring < 5; ring++) {
        // Skip center ring as we've already set it to ensure it's always on
        if (ring == 0) continue;
        
        float intensity = 0.0f;
        
        if (is_expanding) {
            // EXPANSION PHASE: smooth wave of light moving outward
            // Calculate how far the wave has progressed through this ring
            float wave_position = current_size - static_cast<float>(ring);
            
            // Create a window function for smooth transition (0->1->0)
            // Use a smoothstep-like function for more natural transitions
            if (wave_position >= -1.0f && wave_position <= 1.0f) {
                // Smooth transition as wave passes through ring (0->1)
                float t = (wave_position + 1.0f) * 0.5f; // normalize to 0-1
                // Smoothstep function: 3t² - 2t³ (smoother than linear)
                intensity = t * t * (3.0f - 2.0f * t);
            } else if (wave_position > 1.0f) {
                // After wave has passed, maintain a glow that fades with distance
                intensity = 1.0f - std::min(0.5f, (wave_position - 1.0f) * 0.1f);
            }
        } else {
            // CONTRACTION PHASE: smooth wave of darkness moving inward
            
            // During contraction, we want rings to gradually dim from outside to inside
            // Normalize current_size to go from max_size to 0
            float contraction_progress = current_size / max_size;
            
            // Calculate normalized ring position (0 = center, 1 = outermost)
            float ring_position = static_cast<float>(ring) / (max_size - 1.0f);
            
            // Calculate how far the dimming wave has progressed relative to this ring
            // Negative means ring is ahead of the wave (still bright)
            // Positive means wave has passed this ring (dimming/dimmed)
            float dimming_factor = ring_position - contraction_progress;
            
            // Apply a smooth curve for dimming transition
            // We want a value that goes from 1.0 (fully lit) to dim_core_intensity (mostly dimmed)
            if (dimming_factor <= 0.0f) {
                // Ring is ahead of the wave - still fully illuminated
                intensity = 1.0f;
            } else if (dimming_factor > 0.0f && dimming_factor < 0.5f) {
                // Ring is being dimmed by the wave - smooth transition
                // Use cubic ease-out function for gentle dimming start with accelerated falloff
                float t = dimming_factor / 0.5f; // normalize to 0-1
                intensity = 1.0f - (t * t * t); // cubic falloff
            } else {
                // Ring is well behind the wave - maintain a minimal glow
                // that gradually fades to the minimum as we approach the center
                float remaining = 0.15f * (1.0f - std::min(1.0f, (dimming_factor - 0.5f) * 2.0f));
                intensity = remaining;
            }
        }
        
        // Apply natural distance falloff from center
        float distance_falloff = 1.0f - (ring * 0.1f);
        intensity *= distance_falloff;
        
        // Apply easing at extremes of the animation for smooth cycle transition
        if (current_size < 0.5f || current_size > (max_size - 0.5f)) {
            // Ease intensity changes at the start/end of cycle
            float cycle_transition = std::min(current_size, max_size - current_size) * 2.0f;
            float transition_ease = cycle_transition * cycle_transition * 0.25f + 0.75f;
            
            // Apply transition easing where appropriate
            if ((!is_expanding && ring == 4)) {
                intensity *= transition_ease;
            }
        }
        
        // Apply intensity to color without gamma correction
        led_color_t ring_color = min_color + ((base_color - min_color) * intensity);
        
        // Set the calculated color for this ring
        respondGlow.set_ring(ring, ring_color);
    }
    
    // Draw to matrix and update LEDs
    respondGlow.Draw(respond_matrix.get());
    respond_matrix->Update(leds);
    update_leds();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

void LEDController::run_boot() {
    // Same base implementation as run_prompt but with blue orb and white background
    static std::unique_ptr<LEDMatrix> boot_matrix = std::make_unique<LEDMatrix>();

    // Animation state shared across calls
    static float angle = 0.0f;
    static const float ROTATION_SPEED = 300.0f; // degrees per second
    static auto last_update = std::chrono::high_resolution_clock::now();

    // Clear matrix for this frame
    boot_matrix->Clear(leds);

    // Time keeping for smooth motion
    auto now = std::chrono::high_resolution_clock::now();
    float elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
    last_update = now;

    // Update rotation angle
    angle += (ROTATION_SPEED / 1000.0f) * elapsed_ms;
    if (angle >= 360.0f) angle -= 360.0f;

    // Orb polar coordinates (radius 3)
    polar_t orb_position = polar_t::Degrees(angle, 3);

    // Colour palette – dormant style blue for orb, white/grey background
    HSV orbHSV = {220.0f, 0.8f, 1.0f};       // Bright blue
    const led_color_t bg_colour = {128, 128, 128}; // 50% white background

    // Visual parameters
    float sigma      = 3.5f;  // blur radius
    float intensity  = 1.2f;  // overall brightness multiplier

    // Fill background first
    for (int i = 0; i < LED_COUNT; ++i) {
        leds[i] = bg_colour;
    }

    // Pre-allocate influence array
    std::vector<float> influence(LED_COUNT, 0.0f);

    // Compute Gaussian influence per LED
    for (int i = 0; i < LED_COUNT; ++i) {
        polar_t p = led_lut[i];
        float dθ  = angularDifference(p.theta, orb_position.theta);
        float r̄   = (p.r + orb_position.r) * 0.5f;
        float Δr  = p.r - orb_position.r;
        float d2  = (dθ * r̄) * (dθ * r̄) + (Δr * Δr);
        float F   = std::exp(-d2 / (2 * sigma * sigma));
        influence[i] = F;
    }

    // Apply colour to framebuffer
    for (int i = 0; i < LED_COUNT; ++i) {
        float F = influence[i];
        led_color_t orb_rgb = hsv2rgb(orbHSV) * (intensity * F);

        // Blend with background – stronger influence = more orb colour
        float blend = std::min(1.0f, F * 2.0f);
        leds[i].r = static_cast<uint8_t>((1.0f - blend) * bg_colour.r + blend * orb_rgb.r);
        leds[i].g = static_cast<uint8_t>((1.0f - blend) * bg_colour.g + blend * orb_rgb.g);
        leds[i].b = static_cast<uint8_t>((1.0f - blend) * bg_colour.b + blend * orb_rgb.b);
    }

    // Push to hardware
    update_leds();

    // Frame pacing
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void LEDController::run_connecting() {
    // Create a static matrix so we do not re-allocate each frame
    static std::unique_ptr<LEDMatrix> wifi_matrix = std::make_unique<LEDMatrix>();
    // Wi-Fi uses the same blue colour as DORMANT state for now
    const led_color_t wifi_blue = {255, 255, 255};

    // Clear LED buffer for this frame
    wifi_matrix->Clear(leds);

    // -------- Draw Wi-Fi symbol --------
    // Add simple state machine to animate symbol stages
    static int anim_stage = 0;                 // 0 = dot only, 1 = +ring1, 2 = +ring2, 3 = full symbol
    static auto last_stage_change = std::chrono::high_resolution_clock::now();
    const int   STAGE_DURATION_MS = 700;       // time each stage stays on-screen

    // Advance animation stage after duration elapsed
    auto now_time = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now_time - last_stage_change).count() >= STAGE_DURATION_MS) {
        anim_stage = (anim_stage + 1) % 4;     // loop through the 4 stages
        last_stage_change = now_time;
    }

    // 1. Central dot (radius 0)
    try {
        wifi_matrix->set_led(90.0f, 0, wifi_blue); // Use 90° so the symbol faces "up"
    } catch (...) {}

    // Helper lambda to draw an arc on a given ring between start & end angles (inclusive)
    auto draw_arc = [&](int radius, float start_deg, float end_deg, float step_deg){
        for(float ang = start_deg; ang <= end_deg; ang += step_deg){
            try {
                wifi_matrix->set_led(ang, radius, wifi_blue);
            } catch (...) {
                // Ignore out-of-range errors (e.g. if we mis-calculated indices)
            }
        }
    };

    if(anim_stage >= 1){
        // Ring 1 (radius 1): narrow arc around top (3 LEDs at 45°, 90°, 135°)
        draw_arc(1, 45.0f, 135.0f, 45.0f);
    }

    if(anim_stage >= 2){
        // Ring 2 (radius 2): wider arc – 30° LED spacing
        draw_arc(2, 60.0f, 120.0f, 30.0f);
    }

    if(anim_stage >= 3){
        // Ring 3 (radius 3): widest arc – 22.5° spacing
        draw_arc(3, 45.0f, 135.0f, 22.5f);
    }

    // Optional: Ring 4 (radius 4) – uncomment to include & extend stage count
    //if(anim_stage >= 4){
    //    draw_arc(4, 30.0f, 150.0f, 15.0f);
    //}

    // Update matrix -> leds array, then push to hardware
    wifi_matrix->Update(leds);
    update_leds();

    // Slow the update rate a bit so CPU usage is minimal (static image)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void LEDController::run_placeholder_transition() {
    // Matrix reused across frames
    static std::unique_ptr<LEDMatrix> ph_matrix = std::make_unique<LEDMatrix>();

    // Reset animation variables if requested
    if(!ph_initialized){
        ph_filled_angle_deg = 30.0f;
        ph_current_radius   = 0;
        ph_last_update      = std::chrono::high_resolution_clock::now();
        ph_initialized      = true;
    }

    // Tunable parameters
    constexpr float ANGULAR_SPEED = 720.0f;         // Degrees per second that the sector grows
    const     led_color_t OFF_COLOUR    = {0, 0, 0};

    //---------------------------------------------------------------------
    // Time keeping to make animation frame-rate independent
    auto now  = std::chrono::high_resolution_clock::now();
    float dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - ph_last_update).count();
    ph_last_update = now;

    // Grow the lit sector by ANGULAR_SPEED * dt
    ph_filled_angle_deg += ANGULAR_SPEED * (dt_ms / 1000.0f);

    // When the sector completes a full circle, move to the next ring
    if (ph_filled_angle_deg >= 360.0f) {
        ph_filled_angle_deg = 0.0f;
        if (ph_current_radius < 4) {
            ph_current_radius++;          // Advance to next ring (layer-by-layer)
        }
    }

    // Once every ring is filled, just keep the entire matrix lit
    bool fully_filled = (ph_current_radius >= 4 && ph_filled_angle_deg >= 359.9f);

    //---------------------------------------------------------------------
    // Clear framebuffer for this frame
    ph_matrix->Clear(leds);

    // Iterate through each LED to decide if it should be lit this frame
    for (int i = 0; i < LED_COUNT; ++i) {
        polar_t p = led_lut[i];

        // Normalize angle to [0, 360)
        float ang_deg = p.angle_deg();
        while (ang_deg < 0.0f)   ang_deg += 360.0f;
        while (ang_deg >= 360.0f) ang_deg -= 360.0f;

        bool radius_ok = (p.r <= static_cast<float>(ph_current_radius) + 0.01f);
        bool angle_ok  = (ang_deg <= ph_filled_angle_deg);

        if (fully_filled || (radius_ok && angle_ok)) {
            leds[i] = placeholderColor;
        } else {
            leds[i] = OFF_COLOUR;
        }
    }

    // Push framebuffer to LEDs
    update_leds();

    // Modest frame pacing to reduce CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void LEDController::SetPlaceholderColor(const led_color_t& c){
    placeholderColor = c;
    ph_initialized = false; // force animation reset next time it runs
}

#endif
