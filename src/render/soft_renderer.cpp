#include <xui/render/sdf.hpp>
#include <xui/render/soft_renderer.hpp>
#include <algorithm>
#include <cmath>
namespace xui {

bool SoftRenderer::init(const RendererDesc& d) { resize(d.width, d.height, d.dpiScale); return d.width > 0 && d.height > 0; }
void SoftRenderer::resize(u32 w, u32 h, f32 dpi) { m_w = w; m_h = h; m_dpi = dpi; m_fb.assign(std::size_t(w) * h * 4, 0); m_forceFullClear = true; }

TextureHandle SoftRenderer::createTexture(const TextureDesc& d) {
    u32 i = 0;
    for (; i < m_tex.size(); ++i) if (!m_tex[i].alive) break;
    if (i == m_tex.size()) m_tex.emplace_back();
    Tex& t = m_tex[i]; t.w = d.width; t.h = d.height; t.fmt = d.format; t.alive = true;
    const std::size_t bpp = d.format == TextureFormat::A8 ? 1 : 4, bytes = std::size_t(d.width) * d.height * bpp;
    t.data.assign(bytes, 0);
    if (d.pixels) std::copy_n(static_cast<const u8*>(d.pixels), bytes, t.data.begin());
    return TextureHandle{i, t.gen};
}
const SoftRenderer::Tex* SoftRenderer::find(TextureHandle h) const {
    if (!h.valid() || h.index >= m_tex.size() || !m_tex[h.index].alive || m_tex[h.index].gen != h.gen) return nullptr;
    return &m_tex[h.index];
}
void SoftRenderer::updateTexture(TextureHandle h, const void* pixels, Recti r) {
    const Tex* ct = find(h);
    if (!ct || !pixels) return;
    Tex& t = m_tex[h.index]; const std::size_t bpp = t.fmt == TextureFormat::A8 ? 1 : 4;
    for (i32 y = 0; y < r.h; ++y) {
        if (r.y + y < 0 || u32(r.y + y) >= t.h) continue;
        for (i32 x = 0; x < r.w; ++x) {
            if (r.x + x < 0 || u32(r.x + x) >= t.w) continue;
            for (std::size_t c = 0; c < bpp; ++c)
                t.data[(std::size_t(r.y + y) * t.w + std::size_t(r.x + x)) * bpp + c] = static_cast<const u8*>(pixels)[(std::size_t(y) * std::size_t(r.w) + std::size_t(x)) * bpp + c];
        }
    }
}
void SoftRenderer::destroyTexture(TextureHandle h) {
    if (!find(h)) return;
    m_tex[h.index].alive = false; ++m_tex[h.index].gen; m_tex[h.index].data.clear();
}

void SoftRenderer::beginFrame(const FrameInfo& f) {
    if (f.width != m_w || f.height != m_h) resize(f.width, f.height, f.dpiScale);
    const f32 a = f.clear.a;
    const u8 px[4] = {u8(std::lround(std::min(1.f, f.clear.r * a) * 255.f)), u8(std::lround(std::min(1.f, f.clear.g * a) * 255.f)),
                      u8(std::lround(std::min(1.f, f.clear.b * a) * 255.f)), u8(std::lround(std::min(1.f, a) * 255.f))};
    i32 x0 = 0, y0 = 0, x1 = static_cast<i32>(m_w), y1 = static_cast<i32>(m_h);
    if (f.partialUpdate && !m_forceFullClear && f.damage.w > 0 && f.damage.h > 0) {
        x0 = std::clamp(f.damage.x, 0, static_cast<i32>(m_w)); y0 = std::clamp(f.damage.y, 0, static_cast<i32>(m_h));
        x1 = std::clamp(f.damage.x + f.damage.w, x0, static_cast<i32>(m_w)); y1 = std::clamp(f.damage.y + f.damage.h, y0, static_cast<i32>(m_h));
    }
    for (i32 y = y0; y < y1; ++y) for (i32 x = x0; x < x1; ++x) {
        u8* dst = &m_fb[(std::size_t(y) * m_w + static_cast<u32>(x)) * 4];
        dst[0] = px[0]; dst[1] = px[1]; dst[2] = px[2]; dst[3] = px[3];
    }
    m_forceFullClear = false;
}

namespace {
// Bilinear sample with clamp-to-edge. Colour is interpolated alpha-weighted (premultiplied) to avoid dark fringes.
sdf::SampleResult sampleTex(const std::vector<u8>& data, u32 w, u32 h, bool a8, f32 u, f32 v) {
    const f32 x = u * f32(w) - 0.5f, y = v * f32(h) - 0.5f;
    const i32 x0 = static_cast<i32>(std::floor(x)), y0 = static_cast<i32>(std::floor(y));
    const f32 fx = x - f32(x0), fy = y - f32(y0);
    auto fetch = [&](i32 xi, i32 yi) {
        xi = std::min(std::max(xi, 0), i32(w) - 1); yi = std::min(std::max(yi, 0), i32(h) - 1);
        return &data[(std::size_t(yi) * w + std::size_t(xi)) * (a8 ? 1 : 4)];
    };
    const u8* t[4] = {fetch(x0, y0), fetch(x0 + 1, y0), fetch(x0, y0 + 1), fetch(x0 + 1, y0 + 1)};
    const f32 wgt[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    if (a8) { f32 a = 0; for (int i = 0; i < 4; ++i) a += wgt[i] * f32(t[i][0]) / 255.f; return {1, 1, 1, a}; }
    f32 r = 0, g = 0, b = 0, a = 0;
    for (int i = 0; i < 4; ++i) { const f32 ta = f32(t[i][3]) / 255.f; r += wgt[i] * ta * f32(t[i][0]) / 255.f; g += wgt[i] * ta * f32(t[i][1]) / 255.f; b += wgt[i] * ta * f32(t[i][2]) / 255.f; a += wgt[i] * ta; }
    return a > 0 ? sdf::SampleResult{r / a, g / a, b / a, a} : sdf::SampleResult{0, 0, 0, 0};
}
}

void SoftRenderer::drawQuad(const Vertex& tl, const Vertex& br, const Recti& clip, const Tex* tex) {
    const i32 cx0 = std::max<i32>(0, clip.x), cy0 = std::max<i32>(0, clip.y);
    const i32 cx1 = std::min<i32>(i32(m_w), clip.x + clip.w), cy1 = std::min<i32>(i32(m_h), clip.y + clip.h);
    const f32 w = br.x - tl.x, h = br.y - tl.y;
    if (w <= 0 || h <= 0) return;
    const i32 px0 = std::max(cx0, static_cast<i32>(std::ceil(tl.x - 0.5f))), px1 = std::min(cx1, static_cast<i32>(std::ceil(br.x - 0.5f)));
    const i32 py0 = std::max(cy0, static_cast<i32>(std::ceil(tl.y - 0.5f))), py1 = std::min(cy1, static_cast<i32>(std::ceil(br.y - 0.5f)));
    sdf::Frag f;
    f.prim = tl.prim; f.flags = tl.flags; f.hx = tl.hx; f.hy = tl.hy; f.aux0 = tl.aux0; f.aux1 = tl.aux1; f.c0 = tl.c0; f.c1 = tl.c1;
    for (int i = 0; i < 4; ++i) f.rad[i] = f32(tl.rad[i]) * 0.25f;
    f.border = f32(tl.border) * 0.25f;
    auto sampler = [&](f32 u, f32 v) -> sdf::SampleResult {
        if (!tex) return {1, 1, 1, 1};                       // no texture bound: behaves as a white texture
        return sampleTex(tex->data, tex->w, tex->h, tex->fmt == TextureFormat::A8, u, v);
    };
    for (i32 py = py0; py < py1; ++py) {
        const f32 cy = f32(py) + 0.5f, ty = (cy - tl.y) / h;
        for (i32 px = px0; px < px1; ++px) {
            const f32 cx = f32(px) + 0.5f, tx = (cx - tl.x) / w;
            f.x = cx; f.y = cy; f.u = tl.u + (br.u - tl.u) * tx; f.v = tl.v + (br.v - tl.v) * ty;
            const sdf::Rgba src = sdf::shade(f, sampler);
            if (src.a <= 0.f && src.r <= 0.f && src.g <= 0.f && src.b <= 0.f) continue;
            u8* d = &m_fb[(std::size_t(py) * m_w + std::size_t(px)) * 4];
            const sdf::Rgba dst{f32(d[0]) / 255.f, f32(d[1]) / 255.f, f32(d[2]) / 255.f, f32(d[3]) / 255.f};
            const sdf::Rgba o = sdf::over(src, dst);
            d[0] = u8(std::lround(sdf::clamp01(o.r) * 255.f)); d[1] = u8(std::lround(sdf::clamp01(o.g) * 255.f));
            d[2] = u8(std::lround(sdf::clamp01(o.b) * 255.f)); d[3] = u8(std::lround(sdf::clamp01(o.a) * 255.f));
        }
    }
}

void SoftRenderer::submit(const DrawList& dl) {
    for (u32 c = 0; c < dl.nCmds; ++c) {
        const DrawCmd& cmd = dl.cmds[c];
        if (cmd.kind == DrawCmd::Custom) { if (m_custom) m_custom(m_customCtx, cmd.customId, cmd.clip); continue; }
        const Tex* tex = find(cmd.tex);
        for (u32 i = cmd.idxOffset; i + 6 <= cmd.idxOffset + cmd.idxCount; i += 6)
            drawQuad(dl.verts[dl.idx[i]], dl.verts[dl.idx[i + 2]], cmd.clip, tex);   // quads: indices 0,1,2,0,2,3 -> TL and BR
    }
}

Image SoftRenderer::snapshot() const {
    Image img; img.width = m_w; img.height = m_h;
    snapshotRGBA(img.rgba);
    return img;
}

void SoftRenderer::snapshotRGBA(std::vector<u8>& destination) const {
    destination.resize(m_fb.size());
    for (std::size_t i = 0; i < m_fb.size(); i += 4) {
        const u32 a = m_fb[i + 3];
        for (int c = 0; c < 3; ++c) destination[i + std::size_t(c)] = a == 0 ? 0 : u8(std::min<u32>(255, (u32(m_fb[i + std::size_t(c)]) * 255 + a / 2) / a));
        destination[i + 3] = u8(a);
    }
}

void SoftRenderer::snapshotBGRA(std::vector<u8>& destination, Recti region) const {
    destination.resize(m_fb.size());
    const i32 x0 = std::clamp(region.x, 0, static_cast<i32>(m_w));
    const i32 y0 = std::clamp(region.y, 0, static_cast<i32>(m_h));
    const i32 x1 = region.w <= 0 || region.h <= 0 ? static_cast<i32>(m_w) : std::clamp(region.x + region.w, x0, static_cast<i32>(m_w));
    const i32 y1 = region.w <= 0 || region.h <= 0 ? static_cast<i32>(m_h) : std::clamp(region.y + region.h, y0, static_cast<i32>(m_h));
    for (i32 y = y0; y < y1; ++y) for (i32 x = x0; x < x1; ++x) {
        const std::size_t i = (std::size_t(y) * m_w + static_cast<u32>(x)) * 4;
        const u32 a = m_fb[i + 3];
        const u8 r = a == 0 ? 0 : u8(std::min<u32>(255, (u32(m_fb[i]) * 255 + a / 2) / a));
        const u8 g = a == 0 ? 0 : u8(std::min<u32>(255, (u32(m_fb[i + 1]) * 255 + a / 2) / a));
        const u8 b = a == 0 ? 0 : u8(std::min<u32>(255, (u32(m_fb[i + 2]) * 255 + a / 2) / a));
        destination[i] = b; destination[i + 1] = g; destination[i + 2] = r; destination[i + 3] = 255;
    }
}
}
