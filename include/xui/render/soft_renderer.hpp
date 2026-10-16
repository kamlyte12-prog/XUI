#pragma once
/// @file soft_renderer.hpp  Reference CPU rasteriser: executes the exact shader maths of sdf.hpp, deterministically.
/// Used for golden-image tests (no GPU variance) and as a working headless backend. Not fast; not meant to be.
#include <xui/core/handle.hpp>
#include <xui/render/image_io.hpp>
#include <xui/render/renderer.hpp>
#include <vector>

namespace xui {
	class SoftRenderer final : public Renderer {
	public:
		bool init(const RendererDesc& d) override;
		void shutdown() override { m_fb.clear(); m_tex.clear(); }
		void resize(u32 w, u32 h, f32 dpi) override;
		TextureHandle createTexture(const TextureDesc& d) override;
		void updateTexture(TextureHandle h, const void* pixels, Recti region) override;
		void destroyTexture(TextureHandle h) override;
		void beginFrame(const FrameInfo& f) override;
		void submit(const DrawList& dl) override;
		void endFrame() override {}
		void present() override {}
		RendererCaps caps() const override { return { "software", 8192, false, true, true }; }
		void setCustomHandler(CustomDrawFn fn, void* ctx) override { m_custom = fn; m_customCtx = ctx; }

		/// The framebuffer as a straight-alpha RGBA image.
		Image snapshot() const;
		/// Write straight-alpha RGBA pixels into caller-owned storage, reusing its capacity.
		void snapshotRGBA(std::vector<u8>& destination) const;
		/// Write opaque straight-alpha BGRA pixels into caller-owned storage, reusing its capacity.
		/// Intended for native software-window presenters that consume DIB-style channel order.
		void snapshotBGRA(std::vector<u8>& destination, Recti region = {}) const;
		u32 width() const { return m_w; }
		u32 height() const { return m_h; }
	private:
		struct Tex { u32 w = 0, h = 0; TextureFormat fmt = TextureFormat::RGBA8; std::vector<u8> data; bool alive = false; u32 gen = 1; };
		const Tex* find(TextureHandle h) const;
		void drawQuad(const Vertex& tl, const Vertex& br, const Recti& clip, const Tex* tex);
		u32 m_w = 0, m_h = 0; f32 m_dpi = 1.f;
		bool m_forceFullClear = true;
		std::vector<u8> m_fb;                       // premultiplied RGBA8
		std::vector<Tex> m_tex;
		CustomDrawFn m_custom = nullptr; void* m_customCtx = nullptr;
	};
}
