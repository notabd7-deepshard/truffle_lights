#ifdef __aarch64__

#include "ledcontrol.h"

#include <random>
#include <exception>
#include <stdexcept>
#include <cmath>
#include <algorithm>

LEDController::LEDController() : spi(WS2812B_SPI_SPEED) {
    buildLUT();
    if(spi.state == SPI_OPEN){
        off();
        // Set initial state to PROMPT
        state.store(LEDState::PROMPT, std::memory_order_relaxed);
        puts("Starting in PROMPT state");
        control_thread = std::thread(&LEDController::run, this);
    }
    else puts("ledcontrol failed to init SPI");
}

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

void LEDController::run_dormant(bool commit) {
    auto* matrix = liveMatrix_.get();
    static Glow dormGlow(4, { 40,120,255 }, { 1,2,3 });   // size, base, min

    // clear back-buffer
    matrix->Clear(leds);

    // animate & draw
    dormGlow.Update();
    dormGlow.Draw(matrix);

    // push to framebuffer & hardware
    matrix->Update(leds);
    if (commit) {
        update_leds();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // fps-cap
    }
}

void LEDController::run_active(bool commit) {
    auto* matrix = liveMatrix_.get();
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
    if (commit) update_leds();
    
    // Update angle for next frame
    t -= 10.f;
    if(t < 0.f) t = 359.9f;
    if(t > 360.f) t = 0.1f;
}

void LEDController::run_respond_to_user(bool commit) {
    auto* matrix = liveMatrix_.get();
    // Darker orange glow with orange undertone to ensure no green tint appears
    static Glow respondGlow(5, led_color_t{225, 100, 0}, led_color_t{25,10,0});  // Darker orange color

    // Clear the matrix
    matrix->Clear(leds);

    // Update the animation
    respondGlow.Update();
    respondGlow.Draw(matrix);

    // Push to framebuffer & hardware
    matrix->Update(leds);
    if (commit) {
        update_leds();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // fps-cap
    }
}

void LEDController::run_error(bool commit) {
    auto* matrix = liveMatrix_.get();
    // Orange glow - vibrant orange color with very subtle red undertone
    static Glow respondGlow(5, led_color_t{255, 0, 0}, led_color_t{10,0,0});  // Red color

    // Clear the matrix
    matrix->Clear(leds);

    // Update the animation
    respondGlow.Update();
    respondGlow.Draw(matrix);

    // Push to framebuffer & hardware
    matrix->Update(leds);
    if (commit) {
        update_leds();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));   // fps-cap
    }
}

void LEDController::run_prompt(bool commit) {
    auto* matrix = liveMatrix_.get();
    
    // Static variables for animation state
    static float angle = 0.0f;
    static const float ROTATION_SPEED = 300.0f; // degrees per second - increased by 5x
    static auto last_update = std::chrono::high_resolution_clock::now();
    
    // Create our single large orb with constant parameters
    static polar_t orb_position = polar_t::Degrees(angle, 3); // Radius 3

    // Clear matrix
    matrix->Clear(leds); 

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
    if (commit) {
        update_leds();
        // Smooth timing
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void LEDController::run_processing(bool commit)
{
    auto *matrix = liveMatrix_.get();
    // Persistent state across frames
    static bool              initialised   = false;
    static std::chrono::high_resolution_clock::time_point t0;
    static constexpr int     kMaxOrbs      = 3;
    static std::array<float, kMaxOrbs>     spawnDelayMs = { 0.f, 1500.f, 3000.f };  // staggered starts
    static std::array<float, kMaxOrbs>     speedMul     = { 1.0f, 1.4f, 1.8f };     // later → faster
    static std::array<HSV,   kMaxOrbs>     orbHSV       = {
        HSV{245.f, 0.8f, 1.f},                     // white-ish
        HSV{ 40.f, 0.8f, 1.f},                     // blue
        HSV{240.f, 0.8f, 1.f}                      // magenta
    };

    // Gaussian parameters for light contribution
    static std::array<float, kMaxOrbs>     sigma       = { 1.5f, 1.5f, 1.5f };
    static std::array<float, kMaxOrbs>     intensity   = { 1.0f, 1.0f, 1.0f };

    static bool fused = false;              // set once all orbs reach the centre
    static std::chrono::high_resolution_clock::time_point fusedTime;
    static Glow postGlow(4, led_color_t{40, 120, 255}, led_color_t{1,2,3});

    if (!initialised) {
        initialised = true;
        fused       = false;
        t0          = std::chrono::high_resolution_clock::now();
    }

    // Orb positions (re-computed every frame)
    std::vector<polar_t> orbPos;
    std::vector<int>     activeIdx;         // keep which orb is which

    constexpr float startRadius = 3.5f;
    constexpr float spiralDurationMs = 4000.f;      // time for edge→centre
    constexpr float totalRevs       = 3.0f;         // number of full circles while spiralling in

    // Determine which orbs are active and compute their positions
    for (int i = 0; i < kMaxOrbs; ++i) {
        float localT = std::chrono::duration<float,std::milli>(std::chrono::high_resolution_clock::now() - t0).count() - spawnDelayMs[i];
        if (localT < 0.f) continue;                  // not spawned yet

        float tNorm = std::clamp(localT / spiralDurationMs, 0.f, 1.f);

        // radius decreases linearly to centre
        float r = mixf(startRadius, 0.f, tNorm);

        // angle advances clockwise – faster for later orbs
        float theta = -DEG2RAD(360.f * totalRevs * tNorm * speedMul[i]);

        orbPos.push_back( polar_t{ theta, r } );
        activeIdx.push_back(i);
    }

    // After all orbs spawned & reached centre → flag fusion
    if (orbPos.size() == kMaxOrbs) {
        bool allCentre = true;
        for (auto &p : orbPos) if (p.r > 0.1f) { allCentre = false; break; }
        if (allCentre) {
            fused     = true;
            fusedTime = std::chrono::high_resolution_clock::now();
        }
    }

    // Zero framebuffer directly (don't affect matrix rings)
    for (int i = 0; i < LED_COUNT; ++i) leds[i] = {0,0,0};

    // Blend contributions from each active orb (gaussian fall-off)
    for (size_t idx = 0; idx < orbPos.size(); ++idx) {
        int orbIdx = activeIdx[idx];
        polar_t C  = orbPos[idx];

        for (int i = 0; i < LED_COUNT; ++i) {
            polar_t P = led_lut[i];

            float dθ = angularDifference(P.theta, C.theta);
            float r̄  = (P.r + C.r) * 0.5f;
            float Δr  = P.r - C.r;
            float d2 = (dθ*r̄)*(dθ*r̄) + (Δr*Δr);

            float F = expf(-d2 / (2.f * sigma[orbIdx] * sigma[orbIdx]));

            led_color_t base = hsv2rgb(orbHSV[orbIdx]);
            led_color_t contrib = base * (intensity[orbIdx] * F);

            leds[i] = leds[i] + contrib;      // additive with clamping
        }
    }

    // If fusion completed → just run glow afterwards
    if (fused) {
        // Draw glow using matrix helpers
        matrix->Clear(leds);
        postGlow.Update();
        postGlow.Draw(matrix);
        matrix->Update(leds);
        if (commit) {
            update_leds();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return;
    }

    // Push frame directly to hardware
    if (commit) {
        update_leds();
    }
}

void LEDController::renderSnapshot(LEDState s, LEDFrame& out)
{
    // Render the requested state into the controller's LED buffer (leds)
    switch (s) {
        case LEDState::DORMANT:  run_dormant(false);       break;
        case LEDState::PROMPT:   run_prompt(false);        break;
        case LEDState::ACTIVE:   run_active(false);        break;
        case LEDState::RESPOND_TO_USER: run_respond_to_user(false); break;
        case LEDState::ERROR:    run_error(false);         break;
        case LEDState::PROCESSING: run_processing(false);  break;
        default:                 run_active(false);        break;
    }

    // Convert the current LED byte colours into a linear-float frame for the
    // transition engine.  We map the 0‥255 integer channels to 0‥1 floats and
    // keep full brightness (v = 1).
    for (int i = 0; i < LED_COUNT; ++i) {
        out[i].r = leds[i].r / 255.f;
        out[i].g = leds[i].g / 255.f;
        out[i].b = leds[i].b / 255.f;
        out[i].v = 1.f;
    }
}

void LEDController::run() {
    update_leds();
    puts("enter loop");
    
    while(should_run.load(std::memory_order_relaxed)) {
        LEDState current_state = state.load(std::memory_order_relaxed);
        
        // ─── Pending state change → start a new cross-fade ────────────────────
        if (pendingState_ != current_state && !inTransition_) {
            printf("Starting transition from state %d to state %d\n", 
                   static_cast<int>(current_state), static_cast<int>(pendingState_));

            transitionFrom_   = current_state;
            transitionTo_     = pendingState_;
            transitionStart_  = std::chrono::high_resolution_clock::now();
            transitionDurationMs_ = 1200.f; // slightly longer default; tweak as needed

            inTransition_ = true;
        }

        // ─── Blend the two states while in transition ────────────────────────
        if (inTransition_) {
            using clk = std::chrono::high_resolution_clock;
            float t = std::chrono::duration<float,std::milli>(clk::now() - transitionStart_).count() / transitionDurationMs_;

            if (t >= 1.0f) {
                // Transition finished – lock in the new state
                inTransition_ = false;
                state.store(transitionTo_, std::memory_order_relaxed);
                printf("Transition to state %d complete\n", static_cast<int>(transitionTo_));
                continue; // Next loop will render normally
            }

            // Easing curve (cubic ease-in-out)
            float e = (t < .5f) ? 4.f*t*t*t : 1.f - powf(-2.f*t + 2.f, 3.f) / 2.f;

            // Render fresh snapshots so both states keep animating
            renderSnapshot(transitionFrom_, curF_);
            renderSnapshot(transitionTo_,   nextF_);

            // Blend and write into leds
            auto toByte = [](float x){ return uint8_t(std::clamp(x*255.f,0.f,255.f)); };
            for (int i = 0; i < LED_COUNT; ++i) {
                float r = lerpf(curF_[i].r, nextF_[i].r, e);
                float g = lerpf(curF_[i].g, nextF_[i].g, e);
                float b = lerpf(curF_[i].b, nextF_[i].b, e);

                leds[i].r = toByte(r);
                leds[i].g = toByte(g);
                leds[i].b = toByte(b);
            }
            update_leds();
            continue; // Skip the normal per-state rendering this frame
        }
        
        // Normal rendering for current state
        switch(current_state) {
            case LEDState::DORMANT:
                run_dormant(true);
                break;
            case LEDState::RESPOND_TO_USER:
                run_respond_to_user(true);
                break;
            case LEDState::PROMPT:
                run_prompt(true);
                break;
            case LEDState::ERROR:
                run_error(true);
                break;
            case LEDState::PROCESSING:
                run_processing(true);
                break;
            default:
                run_active(true);
                break;
        }
        
        // Consistent frame pacing for all states
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // 60fps
    }
    
    puts("exit loop");
}

#endif
