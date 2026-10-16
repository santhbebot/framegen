#pragma once

#include "framegen/FrameGenCore.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <array>
#include <atomic>
#include <mutex>

namespace framegen {

// ============================================================================
// Frame Interpolator Types
// ============================================================================

struct FrameBuffer {
    GLuint texture_id = 0;
    GLuint fbo_id = 0;
    int width = 0;
    int height = 0;
    int64_t timestamp_ns = 0;
    bool valid = false;
    
    FrameBuffer() = default;
    ~FrameBuffer() { release(); }
    
    void release() {
        if (fbo_id) {
            glDeleteFramebuffers(1, &fbo_id);
            fbo_id = 0;
        }
        if (texture_id) {
            glDeleteTextures(1, &texture_id);
            texture_id = 0;
        }
        valid = false;
    }
};

struct SynthesisProgram {
    GLuint program = 0;
    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    
    // Uniform locations
    GLint u_prev_texture = -1;
    GLint u_curr_texture = -1;
    GLint u_motion_texture = -1;
    GLint u_blend_factor = -1;
    GLint u_motion_scale = -1;
    GLint u_resolution = -1;
    
    SynthesisProgram() = default;
    ~SynthesisProgram() { release(); }
    
    void release() {
        if (program) {
            glDeleteProgram(program);
            program = 0;
        }
        if (vertex_shader) {
            glDeleteShader(vertex_shader);
            vertex_shader = 0;
        }
        if (fragment_shader) {
            glDeleteShader(fragment_shader);
            fragment_shader = 0;
        }
    }
};

// ============================================================================
// GlFrameInterpolator Implementation
// ============================================================================

class GlFrameInterpolator : public IFrameInterpolator {
public:
    GlFrameInterpolator();
    ~GlFrameInterpolator() override;
    
    bool initialize(int width, int height) override;
    void shutdown() override;
    bool installHook() override;
    void uninstallHook() override;
    void onSwapBuffers(void* display, void* surface) override;
    void updateConfig(const FrameGenConfig& config) override;
    void setPostFrameCallback(std::function<void(const RuntimeTelemetrySnapshot&)> callback) override;
    const RuntimeTelemetrySnapshot& getTelemetry() const override;
    void setEnabled(bool enabled) override;
    bool isEnabled() const override;

private:
    // Configuration
    std::atomic<bool> enabled_{false};
    std::atomic<FrameGenMode> frame_gen_mode_{FrameGenMode::Off};
    std::atomic<float> blend_strength_{1.0f};
    std::atomic<float> motion_scale_{1.0f};
    std::atomic<bool> hud_protection_{true};
    std::atomic<float> scene_cut_threshold_{42.0f};
    std::atomic<uint32_t> max_interpolation_frames_{2};
    std::atomic<float> gpu_memory_budget_mb_{128.0f};
    std::atomic<bool> enable_optical_flow_{false};
    std::atomic<bool> low_latency_mode_{true};
    
    // EGL Hook
    using EglSwapBuffersFn = EGLBoolean(*)(EGLDisplay, EGLSurface);
    EglSwapBuffersFn original_egl_swap_buffers_{nullptr};
    void* egl_swap_hook_{nullptr};
    
    // Frame buffers (ring buffer for previous frames)
    static constexpr size_t MAX_FRAME_BUFFERS = 4;
    std::array<FrameBuffer, MAX_FRAME_BUFFERS> frame_buffers_;
    std::atomic<size_t> current_buffer_index_{0};
    std::atomic<size_t> frame_count_{0};
    
    // Synthesis program
    SynthesisProgram synthesis_program_;
    
    // Quad rendering
    GLuint quad_vao_ = 0;
    GLuint quad_vbo_ = 0;
    bool quad_initialized_ = false;
    
    // Telemetry
    mutable RuntimeTelemetrySnapshot telemetry_;
    mutable std::mutex telemetry_mutex_;
    std::function<void(const RuntimeTelemetrySnapshot&)> post_frame_callback_;
    
    // Scene cut detection
    float prev_frame_hash_ = 0.0f;
    
    // Shader sources
    static constexpr const char* VERTEX_SHADER_SOURCE = R"GLSL(
#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texcoord;
out vec2 v_texcoord;
void main() {
    v_texcoord = a_texcoord;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)GLSL";
    
    static constexpr const char* FRAGMENT_SHADER_SOURCE = R"GLSL(
#version 300 es
precision highp float;
in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_prev_texture;
uniform sampler2D u_curr_texture;
uniform sampler2D u_motion_texture;
uniform float u_blend_factor;
uniform float u_motion_scale;
uniform vec2 u_resolution;

void main() {
    vec2 uv = v_texcoord;
    
    // Sample current frame
    vec4 curr = texture(u_curr_texture, uv);
    
    // Sample previous frame
    vec4 prev = texture(u_prev_texture, uv);
    
    // Simple temporal blend (no motion vectors for now)
    // In full implementation, would use motion vectors from u_motion_texture
    vec3 blended = mix(prev.rgb, curr.rgb, u_blend_factor);
    
    // HUD protection: detect high-contrast UI elements and preserve them
    float luminance = dot(curr.rgb, vec3(0.299, 0.587, 0.114));
    float prev_luminance = dot(prev.rgb, vec3(0.299, 0.587, 0.114));
    float luminance_diff = abs(luminance - prev_luminance);
    
    // If luminance difference is very high, likely HUD/text - prefer current frame
    float hud_factor = smoothstep(0.3, 0.5, luminance_diff);
    blended = mix(blended, curr.rgb, hud_factor);
    
    frag_color = vec4(blended, curr.a);
}
)GLSL";
    
    // Internal methods
    bool createSynthesisProgram();
    void destroySynthesisProgram();
    bool ensureGlResources(int width, int height);
    void releaseGlResources();
    FrameBuffer& acquireFrameBuffer(int width, int height);
    void captureBackbufferToTexture(FrameBuffer& buffer);
    void drawSynthesizedQuad(const FrameBuffer& prev, const FrameBuffer& curr, float blend_factor);
    float computeFrameDifference(const FrameBuffer& a, const FrameBuffer& b);
    bool detectSceneCut(const FrameBuffer& prev, const FrameBuffer& curr);
    void presentInterpolatedFrames(EGLDisplay display, EGLSurface surface, int count, const FrameBuffer& prev, const FrameBuffer& curr);
    
    // Static hook callback
    static EGLBoolean eglSwapBuffersHook(EGLDisplay display, EGLSurface surface);
    static GlFrameInterpolator* instance_;
};

} // namespace framegen