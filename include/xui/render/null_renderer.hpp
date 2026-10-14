#pragma once
/// @file null_renderer.hpp  Headless Renderer that records what it is given. For unit tests and golden-image plumbing.
#include <xui/core/handle.hpp>
#include <xui/render/renderer.hpp>
#include <vector>

namespace xui {
class NullRenderer final : public Renderer {
public:
    /// A deep copy of the most recently submitted DrawList (the producer's buffers are reused every frame).
    struct Recorded { std::vector<Vertex> verts; std::vector<u32> idx; std::vector<DrawCmd> cmds; };

    bool init(const RendererDesc& d) override { m_desc = d; m_inited = true; return true; }
    void shutdown() override { m_inited = false; m_textures.clear(); }
    void resize(u32 w, u32 h, f32 dpi) override { m_desc.width = w; m_desc.height = h; m_desc.dpiScale = dpi; }
    TextureHandle createTexture(const TextureDesc& d) override;
    void updateTexture(TextureHandle, const void*, Recti) override { ++m_textureUpdates; }
    void destroyTexture(TextureHandle h) override;
    void beginFrame(const FrameInfo& f) override { m_frame = f; m_inFrame = true; m_drawCallsThisFrame = 0; }
    void submit(const DrawList& dl) override;
    void endFrame() override { m_inFrame = false; m_drawCallsLastFrame = m_drawCallsThisFrame; ++m_frames; }
    void present() override {}
    RendererCaps caps() const override { return {"null", 16384, false, false, true}; }
    void setCustomHandler(CustomDrawFn fn, void* ctx) override { m_custom = fn; m_customCtx = ctx; }

    const Recorded& last() const { return m_last; }
    u32 drawCallsLastFrame() const { return m_drawCallsLastFrame; }
    u64 frames() const { return m_frames; }
    u32 liveTextures() const { return static_cast<u32>(m_liveTextures); }
    u32 textureUpdates() const { return m_textureUpdates; }
    const RendererDesc& desc() const { return m_desc; }
    const FrameInfo& frameInfo() const { return m_frame; }
private:
    struct Tex { u32 w = 0, h = 0; TextureFormat fmt = TextureFormat::RGBA8; bool alive = false; u32 gen = 1; };
    RendererDesc m_desc; FrameInfo m_frame; Recorded m_last;
    std::vector<Tex> m_textures; std::size_t m_liveTextures = 0;
    bool m_inited = false, m_inFrame = false;
    u32 m_drawCallsThisFrame = 0, m_drawCallsLastFrame = 0, m_textureUpdates = 0; u64 m_frames = 0;
    CustomDrawFn m_custom = nullptr; void* m_customCtx = nullptr;
};
}
