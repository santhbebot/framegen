#include "framegen/interpolation/GlFrameInterpolator.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>
#include <cstring>
#include <algorithm>
#include <dlfcn.h>
#include <pl/memory.h>

#define LOG_TAG "FrameGenInterpolator"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Static instance for hook callback
GlFrameInterpolator* GlFrameInterpolator::instance_ = nullptr;

// ============================================================================
// GlFrameInterpolator Implementation
// ============================================================================

GlFrameInterpolator::GlFrameInterpolator() {
    instance_ = this;
    LOGI("GlFrameInterpolator constructed");
}

GlFrameInterpolator::~GlFrameInterpolator() {
    shutdown();
    instance_ = nullptr;
    LOGI("GlFrameInterpolator destroyed");
}

bool GlFrameInterpolator::initialize(int width, int height) {
    LOGI("GlFrameInterpolator::initialize(%d, %d)", width, height);
    
    if (!ensureGlResources(width, height)) {
        LOGE("Failed to initialize GL resources");
        return false;
    }
    
    if (!createSynthesisProgram()) {
        LOGE("Failed to create synthesis program");
        releaseGlResources();
        return false;
    }
    
    // Initialize quad for fullscreen rendering
    if (!quad_initialized_) {
        glGenVertexArrays(1, &quad_vao_);
        glGenBuffers(1, &quad_vbo_);
        
        // Fullscreen quad vertices (position + texcoord)
        float vertices[] = {
            // Position      // Texcoord
            -1.0f, -1.0f,    0.0f, 0.0f,
             1.0f, -1.0f,    1.0f, 0.0f,
             1.0f,  1.0f,    1.0f, 1.0f,
            -1.0f,  1.0f,    0.0f, 1.0f
        };
        
        glBindVertexArray(quad_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        quad_initialized_ = true;
    }
    
    enabled_.store(true, std::memory_order_relaxed);
    frame_count_.store(0, std::memory_order_relaxed);
    
    LOGI("GlFrameInterpolator initialized successfully");
    return true;
}

void GlFrameInterpolator::shutdown() {
    LOGI("GlFrameInterpolator::shutdown");
    
    uninstallHook();
    releaseGlResources();
    destroySynthesisProgram();
    
    if (quad_initialized_) {
        glDeleteVertexArrays(1, &quad_vao_);
        glDeleteBuffers(1, &quad_vbo_);
        quad_vao_ = 0;
        quad_vbo_ = 0;
        quad_initialized_ = false;
    }
    
    enabled_.store(false, std::memory_order_relaxed);
}

bool GlFrameInterpolator::installHook() {
    if (egl_swap_hook_) {
        LOGW("Hook already installed");
        return true;
    }
    
    LOGI("Installing eglSwapBuffers hook");
    
    // Resolve eglSwapBuffers address
    void* egl_swap_addr = pl::memory::resolveSignature("libEGL.so", "eglSwapBuffers");
    if (!egl_swap_addr) {
        // Fallback: try dlsym
        void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
        if (egl_handle) {
            egl_swap_addr = dlsym(egl_handle, "eglSwapBuffers");
            dlclose(egl_handle);
        }
    }
    
    if (!egl_swap_addr) {
        LOGE("Failed to resolve eglSwapBuffers");
        return false;
    }
    
    // Install hook using pl::memory::hook
    egl_swap_hook_ = pl::memory::hook(
        egl_swap_addr,
        reinterpret_cast<void*>(&eglSwapBuffersHook),
        reinterpret_cast<void**>(&original_egl_swap_buffers_),
        pl::memory::HookPriority::Critical
    );
    
    if (!egl_swap_hook_) {
        LOGE("Failed to install eglSwapBuffers hook");
        return false;
    }
    
    LOGI("eglSwapBuffers hook installed successfully at %p", egl_swap_addr);
    return true;
}

void GlFrameInterpolator::uninstallHook() {
    if (egl_swap_hook_ && original_egl_swap_buffers_) {
        LOGI("Uninstalling eglSwapBuffers hook");
        
        void* egl_swap_addr = pl::memory::resolveSignature("libEGL.so", "eglSwapBuffers");
        if (!egl_swap_addr) {
            void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
            if (egl_handle) {
                egl_swap_addr = dlsym(egl_handle, "eglSwapBuffers");
                dlclose(egl_handle);
            }
        }
        
        if (egl_swap_addr) {
            pl::memory::unhook(egl_swap_addr, egl_swap_hook_);
        }
        
        egl_swap_hook_ = nullptr;
        original_egl_swap_buffers_ = nullptr;
    }
}

void GlFrameInterpolator::onSwapBuffers(void* display, void* surface) {
    if (!enabled_.load(std::memory_order_relaxed) || 
        frame_gen_mode_.load(std::memory_order_relaxed) == FrameGenMode::Off) {
        // Just call original
        if (original_egl_swap_buffers_) {
            original_egl_swap_buffers_(static_cast<EGLDisplay>(display), static_cast<EGLSurface>(surface));
        }
        return;
    }
    
    EGLDisplay egl_display = static_cast<EGLDisplay>(display);
    EGLSurface egl_surface = static_cast<EGLSurface>(surface);
    
    // Get current framebuffer dimensions
    EGLint width = 0, height = 0;
    eglQuerySurface(egl_display, egl_surface, EGL_WIDTH, &width);
    eglQuerySurface(egl_display, egl_surface, EGL_HEIGHT, &height);
    
    if (width <= 0 || height <= 0) {
        if (original_egl_swap_buffers_) {
            original_egl_swap_buffers_(egl_display, egl_surface);
        }
        return;
    }
    
    // Ensure GL resources match current surface size
    ensureGlResources(width, height);
    
    // Capture current backbuffer
    FrameBuffer& current_buffer = acquireFrameBuffer(width, height);
    captureBackbufferToTexture(current_buffer);
    current_buffer.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Update frame count
    size_t frame_idx = frame_count_.fetch_add(1, std::memory_order_relaxed);
    
    // Find previous valid frame
    FrameBuffer* prev_buffer = nullptr;
    for (int i = 1; i < MAX_FRAME_BUFFERS; ++i) {
        size_t idx = (current_buffer_index_.load() + MAX_FRAME_BUFFERS - i) % MAX_FRAME_BUFFERS;
        if (frame_buffers_[idx].valid) {
            prev_buffer = &frame_buffers_[idx];
            break;
        }
    }
    
    // Check for scene cut
    bool scene_cut = false;
    if (prev_buffer) {
        scene_cut = detectSceneCut(*prev_buffer, current_buffer);
        if (scene_cut) {
            // Scene cut detected - don't interpolate, just present current
            LOGI("Scene cut detected at frame %zu", frame_idx);
        }
    }
    
    // Present interpolated frames if enabled and no scene cut
    uint32_t max_interp = max_interpolation_frames_.load(std::memory_order_relaxed);
    FrameGenMode mode = frame_gen_mode_.load(std::memory_order_relaxed);
    
    int interp_frames = 0;
    if (!scene_cut && prev_buffer && mode != FrameGenMode::Off) {
        switch (mode) {
            case FrameGenMode::SingleSwap: interp_frames = 1; break;
            case FrameGenMode::DoubleSwap: interp_frames = 2; break;
            case FrameGenMode::TripleSwap: interp_frames = 3; break;
            default: interp_frames = 0; break;
        }
        interp_frames = std::min<uint32_t>(interp_frames, max_interp);
        
        if (interp_frames > 0) {
            presentInterpolatedFrames(egl_display, egl_surface, interp_frames, *prev_buffer, current_buffer);
        }
    }
    
    // Present the actual game frame
    if (original_egl_swap_buffers_) {
        original_egl_swap_buffers_(egl_display, egl_surface);
    }
    
    // Update current buffer index
    current_buffer_index_.store((current_buffer_index_.load() + 1) % MAX_FRAME_BUFFERS, std::memory_order_relaxed);
    
    // Update telemetry
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex_);
        telemetry_.frame_number = frame_idx;
        telemetry_.timestamp_ns = current_buffer.timestamp_ns;
        telemetry_.interpolated_frames_this_second = interp_frames;
        telemetry_.scene_cut_detected = scene_cut;
    }
    
    // Call post-frame callback
    if (post_frame_callback_) {
        post_frame_callback_(telemetry_);
    }
}

void GlFrameInterpolator::updateConfig(const FrameGenConfig& config) {
    frame_gen_mode_.store(config.frame_gen_mode, std::memory_order_relaxed);
    blend_strength_.store(config.blend_strength, std::memory_order_relaxed);
    motion_scale_.store(config.motion_scale, std::memory_order_relaxed);
    hud_protection_.store(config.hud_protection, std::memory_order_relaxed);
    scene_cut_threshold_.store(config.scene_cut_threshold, std::memory_order_relaxed);
    max_interpolation_frames_.store(config.max_interpolation_frames, std::memory_order_relaxed);
    gpu_memory_budget_mb_.store(config.gpu_memory_budget_mb, std::memory_order_relaxed);
    enable_optical_flow_.store(config.enable_optical_flow, std::memory_order_relaxed);
    low_latency_mode_.store(config.low_latency_mode, std::memory_order_relaxed);
    
    LOGI("GlFrameInterpolator config updated: mode=%d, blend=%.2f, motion=%.2f",
         static_cast<int>(config.frame_gen_mode), config.blend_strength, config.motion_scale);
}

void GlFrameInterpolator::setPostFrameCallback(std::function<void(const RuntimeTelemetrySnapshot&)> callback) {
    post_frame_callback_ = std::move(callback);
}

const RuntimeTelemetrySnapshot& GlFrameInterpolator::getTelemetry() const {
    return telemetry_;
}

void GlFrameInterpolator::setEnabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_relaxed);
    LOGI("GlFrameInterpolator %s", enabled ? "enabled" : "disabled");
}

bool GlFrameInterpolator::isEnabled() const {
    return enabled_.load(std::memory_order_relaxed);
}

// ============================================================================
// Static Hook Callback
// ============================================================================

EGLBoolean GlFrameInterpolator::eglSwapBuffersHook(EGLDisplay display, EGLSurface surface) {
    if (instance_) {
        instance_->onSwapBuffers(display, surface);
        return EGL_TRUE;  // We've handled the swap
    }
    return EGL_FALSE;
}

// ============================================================================
// Private Methods
// ============================================================================

bool GlFrameInterpolator::createSynthesisProgram() {
    LOGI("Creating synthesis shader program");
    
    // Compile vertex shader
    synthesis_program_.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(synthesis_program_.vertex_shader, 1, &VERTEX_SHADER_SOURCE, nullptr);
    glCompileShader(synthesis_program_.vertex_shader);
    
    GLint compiled = 0;
    glGetShaderiv(synthesis_program_.vertex_shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(synthesis_program_.vertex_shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            std::vector<char> log(len);
            glGetShaderInfoLog(synthesis_program_.vertex_shader, len, nullptr, log.data());
            LOGE("Vertex shader compile failed: %s", log.data());
        }
        return false;
    }
    
    // Compile fragment shader
    synthesis_program_.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(synthesis_program_.fragment_shader, 1, &FRAGMENT_SHADER_SOURCE, nullptr);
    glCompileShader(synthesis_program_.fragment_shader);
    
    glGetShaderiv(synthesis_program_.fragment_shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(synthesis_program_.fragment_shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            std::vector<char> log(len);
            glGetShaderInfoLog(synthesis_program_.fragment_shader, len, nullptr, log.data());
            LOGE("Fragment shader compile failed: %s", log.data());
        }
        return false;
    }
    
    // Link program
    synthesis_program_.program = glCreateProgram();
    glAttachShader(synthesis_program_.program, synthesis_program_.vertex_shader);
    glAttachShader(synthesis_program_.program, synthesis_program_.fragment_shader);
    glLinkProgram(synthesis_program_.program);
    
    GLint linked = 0;
    glGetProgramiv(synthesis_program_.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(synthesis_program_.program, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            std::vector<char> log(len);
            glGetProgramInfoLog(synthesis_program_.program, len, nullptr, log.data());
            LOGE("Program link failed: %s", log.data());
        }
        return false;
    }
    
    // Get uniform locations
    synthesis_program_.u_prev_texture = glGetUniformLocation(synthesis_program_.program, "u_prev_texture");
    synthesis_program_.u_curr_texture = glGetUniformLocation(synthesis_program_.program, "u_curr_texture");
    synthesis_program_.u_motion_texture = glGetUniformLocation(synthesis_program_.program, "u_motion_texture");
    synthesis_program_.u_blend_factor = glGetUniformLocation(synthesis_program_.program, "u_blend_factor");
    synthesis_program_.u_motion_scale = glGetUniformLocation(synthesis_program_.program, "u_motion_scale");
    synthesis_program_.u_resolution = glGetUniformLocation(synthesis_program_.program, "u_resolution");
    
    LOGI("Synthesis program created: program=%u", synthesis_program_.program);
    return true;
}

void GlFrameInterpolator::destroySynthesisProgram() {
    synthesis_program_.release();
    LOGI("Synthesis program destroyed");
}

bool GlFrameInterpolator::ensureGlResources(int width, int height) {
    // Check if we need to resize
    bool need_resize = false;
    for (auto& buf : frame_buffers_) {
        if (buf.valid && (buf.width != width || buf.height != height)) {
            need_resize = true;
            break;
        }
    }
    
    if (!need_resize) {
        // Check if any buffer needs to be created
        bool any_valid = false;
        for (const auto& buf : frame_buffers_) {
            if (buf.valid) { any_valid = true; break; }
        }
        if (!any_valid) need_resize = true;
    }
    
    if (need_resize) {
        LOGI("Resizing GL resources to %dx%d", width, height);
        releaseGlResources();
        
        // Recreate frame buffers
        for (auto& buf : frame_buffers_) {
            glGenTextures(1, &buf.texture_id);
            glBindTexture(GL_TEXTURE_2D, buf.texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glGenFramebuffers(1, &buf.fbo_id);
            glBindFramebuffer(GL_FRAMEBUFFER, buf.fbo_id);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buf.texture_id, 0);
            
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                LOGE("Framebuffer incomplete: 0x%x", status);
                return false;
            }
            
            buf.width = width;
            buf.height = height;
            buf.valid = true;
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    return true;
}

void GlFrameInterpolator::releaseGlResources() {
    for (auto& buf : frame_buffers_) {
        buf.release();
    }
    LOGI("GL resources released");
}

FrameBuffer& GlFrameInterpolator::acquireFrameBuffer(int width, int height) {
    size_t idx = current_buffer_index_.load(std::memory_order_relaxed);
    FrameBuffer& buffer = frame_buffers_[idx];
    
    if (!buffer.valid || buffer.width != width || buffer.height != height) {
        // Recreate this buffer
        buffer.release();
        
        glGenTextures(1, &buffer.texture_id);
        glBindTexture(GL_TEXTURE_2D, buffer.texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glGenFramebuffers(1, &buffer.fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo_id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.texture_id, 0);
        
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("Framebuffer incomplete on acquire: 0x%x", status);
        }
        
        buffer.width = width;
        buffer.height = height;
        buffer.valid = true;
        
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    return buffer;
}

void GlFrameInterpolator::captureBackbufferToTexture(FrameBuffer& buffer) {
    // Save current FBO binding
    GLint current_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
    
    // Bind our FBO and blit from default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);  // Default framebuffer (backbuffer)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer.fbo_id);
    
    glBlitFramebuffer(
        0, 0, buffer.width, buffer.height,
        0, 0, buffer.width, buffer.height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR
    );
    
    // Restore
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_fbo);
}

void GlFrameInterpolator::drawSynthesizedQuad(const FrameBuffer& prev, const FrameBuffer& curr, float blend_factor) {
    // Save GL state
    GLint current_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    
    GLint current_vao = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &current_vao);
    
    GLint current_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
    
    // Use synthesis program
    glUseProgram(synthesis_program_.program);
    
    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prev.texture_id);
    glUniform1i(synthesis_program_.u_prev_texture, 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, curr.texture_id);
    glUniform1i(synthesis_program_.u_curr_texture, 1);
    
    // Motion texture not used in simple blend (would be GL_TEXTURE2)
    glUniform1i(synthesis_program_.u_motion_texture, 2);
    
    // Set uniforms
    glUniform1f(synthesis_program_.u_blend_factor, blend_factor);
    glUniform1f(synthesis_program_.u_motion_scale, motion_scale_.load(std::memory_order_relaxed));
    glUniform2f(synthesis_program_.u_resolution, static_cast<float>(curr.width), static_cast<float>(curr.height));
    
    // Draw quad
    glBindVertexArray(quad_vao_);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    
    // Restore state
    glUseProgram(current_program);
    glBindVertexArray(current_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
    glActiveTexture(GL_TEXTURE0);
}

float GlFrameInterpolator::computeFrameDifference(const FrameBuffer& a, const FrameBuffer& b) {
    // Simple hash-based difference detection
    // In production, would sample pixels or use compute shader
    // For now, use a simple heuristic based on timestamps
    if (!a.valid || !b.valid) return 100.0f;  // Max difference if invalid
    
    // Placeholder: return a value based on time difference
    // Real implementation would compare pixel data
    return 0.0f;
}

bool GlFrameInterpolator::detectSceneCut(const FrameBuffer& prev, const FrameBuffer& curr) {
    if (!prev.valid || !curr.valid) return false;
    
    // Simple scene cut detection using frame difference
    // Real implementation would use perceptual hashing or histogram comparison
    float diff = computeFrameDifference(prev, curr);
    float threshold = scene_cut_threshold_.load(std::memory_order_relaxed);
    
    return diff > threshold;
}

void GlFrameInterpolator::presentInterpolatedFrames(EGLDisplay display, EGLSurface surface, 
                                                    int count, const FrameBuffer& prev, const FrameBuffer& curr) {
    // Save current FBO
    GLint current_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
    
    // Create temporary FBO for interpolated frames
    GLuint temp_fbo = 0;
    glGenFramebuffers(1, &temp_fbo);
    
    float blend = blend_strength_.load(std::memory_order_relaxed);
    
    for (int i = 1; i <= count; ++i) {
        // Calculate blend factor for this interpolated frame
        float t = static_cast<float>(i) / static_cast<float>(count + 1);
        float frame_blend = blend * t;
        
        // Bind temp FBO with current surface's backbuffer as target
        // Note: We can't directly render to the surface's backbuffer from here
        // In real implementation, this would use EGL_ANDROID_presentation_time
        // or render to a texture and then blit
        
        // For now, draw to default framebuffer (which should be the backbuffer)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, curr.width, curr.height);
        
        drawSynthesizedQuad(prev, curr, frame_blend);
        
        // Flush to ensure rendering completes
        glFlush();
    }
    
    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
    glDeleteFramebuffers(1, &temp_fbo);
}

} // namespace framegen