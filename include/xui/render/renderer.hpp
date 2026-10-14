#pragma once
/// @file renderer.hpp  The backend-agnostic Renderer interface. Widgets and application code never see a backend type.
#include <xui/core/color.hpp>
#include <xui/render/drawlist.hpp>

namespace xui {

enum class TextureFormat : u8 { A8, RGBA8 };   ///< A8: glyph coverage. RGBA8: straight-alpha colour.
struct TextureDesc {
    u32 width = 0, height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    const void* pixels = nullptr;              ///< optional initial contents, tightly packed rows
    bool linearFilter = true;
};
struct RendererDesc {
    u32 width = 0, height = 0;                 ///< backbuffer size, physical px
    f32 dpiScale = 1.f;
    void* nativeWindow = nullptr;              ///< backend-specific (GLFW: GLFWwindow*), unused by headless backends
    bool vsync = true;
};
struct FrameInfo {
    u32 width = 0, height = 0;                 ///< physical px
    f32 dpiScale = 1.f;
    Color clear{0, 0, 0, 1};
    u64 frameIndex = 0;
    Recti damage{};                 ///< physical-pixel dirty region when partialUpdate is true
    bool partialUpdate = false;     ///< preserve pixels outside damage; unsupported backends may repaint fully
};
struct RendererCaps {
    const char* name = "";
    u32 maxTextureSize = 0;
    bool shaderClip = false;                   ///< can clip in the shader (otherwise scissor only)
    bool dirtyRects = false;                   ///< can repaint a sub-rectangle (optional optimisation)
    bool customCommands = false;
};
/// Callback for DrawCmd::Custom: the backend invokes it at the right point in the command stream, with the scissor set.
using CustomDrawFn = void (*)(void* ctx, u32 customId, Recti clip);

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool init(const RendererDesc&) = 0;
    virtual void shutdown() = 0;
    virtual void resize(u32 w, u32 h, f32 dpiScale) = 0;

    virtual TextureHandle createTexture(const TextureDesc&) = 0;
    virtual void          updateTexture(TextureHandle, const void* pixels, Recti region) = 0;
    virtual void          destroyTexture(TextureHandle) = 0;

    virtual void beginFrame(const FrameInfo&) = 0;
    virtual void submit(const DrawList&) = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;

    virtual RendererCaps caps() const = 0;
    virtual void setCustomHandler(CustomDrawFn, void*) {}
};
}
