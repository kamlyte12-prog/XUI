#include <xui/render/null_renderer.hpp>
namespace xui {
TextureHandle NullRenderer::createTexture(const TextureDesc& d) {
    u32 i = 0;
    for (; i < m_textures.size(); ++i) if (!m_textures[i].alive) break;
    if (i == m_textures.size()) m_textures.emplace_back();
    Tex& t = m_textures[i]; t.w = d.width; t.h = d.height; t.fmt = d.format; t.alive = true;
    ++m_liveTextures;
    return TextureHandle{i, t.gen};
}
void NullRenderer::destroyTexture(TextureHandle h) {
    if (h.index >= m_textures.size() || !m_textures[h.index].alive || m_textures[h.index].gen != h.gen) return;
    m_textures[h.index].alive = false; ++m_textures[h.index].gen; --m_liveTextures;
}
void NullRenderer::submit(const DrawList& dl) {
    m_last.verts.assign(dl.verts, dl.verts + dl.nVerts);
    m_last.idx.assign(dl.idx, dl.idx + dl.nIdx);
    m_last.cmds.assign(dl.cmds, dl.cmds + dl.nCmds);
    m_drawCallsThisFrame += dl.nCmds;
    for (u32 i = 0; i < dl.nCmds; ++i) if (dl.cmds[i].kind == DrawCmd::Custom && m_custom) m_custom(m_customCtx, dl.cmds[i].customId, dl.cmds[i].clip);
}
}
