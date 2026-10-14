#include <xui/text/font.hpp>
#include <algorithm>
#include <cstring>

namespace xui {

// ---- table directory --------------------------------------------------------------------------

u32 Font::tableOffset(std::string_view tag) const {
    u16 n = u16At(4);
    for (u16 i = 0; i < n; ++i) {
        u32 rec = 12 + u32(i) * 16;
        if (m_data.compare(rec, 4, tag) == 0) return u32At(rec + 8);
    }
    return 0;
}

bool Font::load(std::string_view data, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        m_data.clear();
        return false;
    };
    if (data.size() < 12) return fail("file too small for an sfnt directory");
    m_data.assign(data);
    u32 magic = u32At(0);
    if (magic != 0x00010000u && magic != 0x74727565u /*'true'*/ && magic != 0x4F54544Fu /*'OTTO'*/)
        return fail("not a TrueType/OpenType font (bad sfnt version)");
    m_cff = (magic == 0x4F54544Fu);

    if (!(m_head = tableOffset("head"))) return fail("missing head table");
    if (!(m_maxp = tableOffset("maxp"))) return fail("missing maxp table");
    if (!(m_hhea = tableOffset("hhea"))) return fail("missing hhea table");
    if (!(m_hmtx = tableOffset("hmtx"))) return fail("missing hmtx table");
    if (!(m_loca = tableOffset("loca"))) return fail("missing loca table");
    if (!(m_glyf = tableOffset("glyf"))) return fail("missing glyf table");

    m_metrics.unitsPerEm = u16At(m_head + 18);
    if (m_metrics.unitsPerEm < 16 || m_metrics.unitsPerEm > 16384) return fail("bad unitsPerEm in head");
    m_longLoca = i16At(m_head + 50) != 0;
    m_numGlyphs = u16At(m_maxp + 4);
    m_metrics.glyphCount = m_numGlyphs;

    m_metrics.ascender = i16At(m_hhea + 4);
    m_metrics.descender = i16At(m_hhea + 6);
    m_metrics.lineGap = i16At(m_hhea + 8);
    m_metrics.underlinePosition = i16At(m_hhea + 10);
    m_metrics.underlineThickness = std::max<i16>(1, i16At(m_hhea + 12));
    m_numHMetrics = u16At(m_hhea + 34);
    if (m_numHMetrics == 0 || m_numHMetrics > m_numGlyphs) return fail("bad numberOfHMetrics");

    u32 cmap = tableOffset("cmap");
    if (!cmap || !parseCmap(cmap)) return fail("no usable cmap subtable (need format 4 or 12)");

    if (u32 os2 = tableOffset("OS/2")) {
        m_metrics.typoAscender = i16At(os2 + 68);
        m_metrics.typoDescender = i16At(os2 + 70);
    }
    if (u32 name = tableOffset("name")) parseName(name);
    if ((m_kern = tableOffset("kern"))) parseKern(m_kern);
    m_metrics.hasKernTable = !m_kernPairs.empty();
    return true;
}

// ---- character map ----------------------------------------------------------------------------

bool Font::parseCmap(u32 off) {
    u16 n = u16At(off + 2);
    for (u16 i = 0; i < n; ++i) {
        u32 rec = off + 4 + u32(i) * 8;
        u16 plat = u16At(rec), enc = u16At(rec + 2);
        u32 sub = off + u32At(rec + 4);
        if (sub + 2 > m_data.size()) continue;
        u16 fmt = u16At(sub);
        if ((plat == 3 && (enc == 1 || enc == 10)) || (plat == 0 && (enc == 4 || enc == 6))) {
            if (fmt == 4 && !m_cmap4) m_cmap4 = sub;
            if (fmt == 12 && !m_cmap12) m_cmap12 = sub;
        } else if (plat == 3 && enc == 0 && fmt == 4 && !m_cmap4) {
            m_cmap4 = sub;   // symbol fonts: chars map through 0xF000; the plain lookup still works
        }
    }
    return m_cmap4 || m_cmap12;
}

u32 Font::glyphIndex(u32 cp) const {
    if (m_cmap12) {   // full unicode: binary search the groups
        u32 n = u32At(m_cmap12 + 12);
        u32 lo = 0, hi = n;
        while (lo < hi) {
            u32 mid = (lo + hi) / 2, g = m_cmap12 + 16 + mid * 12;
            u32 start = u32At(g), end = u32At(g + 4);
            if (cp < start) hi = mid;
            else if (cp > end) lo = mid + 1;
            else return u32At(g + 8) + (cp - start);
        }
    }
    if (m_cmap4 && cp <= 0xFFFF) {
        u32 t = m_cmap4;
        u16 segCount = u16At(t + 6) / 2;
        const u32 endBase = t + 14, startBase = endBase + 2 * segCount + 2,
                   deltaBase = startBase + 2 * segCount, rangeBase = deltaBase + 2 * segCount;
        u32 lo = 0, hi = segCount;
        while (lo < hi) {                       // first segment whose end code >= cp
            u32 mid = (lo + hi) / 2;
            if (u16At(endBase + mid * 2) < cp) lo = mid + 1;
            else hi = mid;
        }
        if (lo >= segCount) return 0;
        u32 seg = lo;
        if (cp < u16At(startBase + seg * 2)) return 0;
        u16 delta = u16At(deltaBase + seg * 2), rangeOff = u16At(rangeBase + seg * 2);
        if (!rangeOff) return (cp + delta) & 0xFFFFu;
        u32 idAddr = rangeBase + seg * 2 + rangeOff + (cp - u16At(startBase + seg * 2)) * 2;
        u16 g = u16At(idAddr);
        return g ? (g + delta) & 0xFFFFu : 0;
    }
    return 0;
}

// ---- metrics ----------------------------------------------------------------------------------

f32 Font::advance(u32 g) const {
    if (g >= m_numGlyphs) return 0;
    u32 entry = g < m_numHMetrics ? g : m_numHMetrics - 1;
    return u16At(m_hmtx + entry * 4);
}

bool Font::kerning(u32 left, u32 right, i16& outX) const {
    u64 key = (u64(left) << 32) | right;
    auto it = std::lower_bound(m_kernPairs.begin(), m_kernPairs.end(), key,
                               [](const auto& p, u64 k) { return p.first < k; });
    if (it == m_kernPairs.end() || it->first != key) return false;
    outX = i16(it->second);
    return true;
}

// ---- kern / name ------------------------------------------------------------------------------

bool Font::parseKern(u32 off) {
    u16 nSub = u16At(off + 2);
    u32 p = off + 4;
    for (u16 i = 0; i < nSub; ++i) {
        if (p + 6 > m_data.size()) break;
        u16 length = u16At(p + 2);
        u16 coverage = u16At(p + 4);
        u16 format = coverage >> 8;
        bool horizontal = !(coverage & 1);
        if (format == 0 && horizontal && p + 14 <= m_data.size()) {
            u16 nPairs = u16At(p + 6);
            u32 q = p + 14;
            m_kernPairs.reserve(m_kernPairs.size() + nPairs);
            for (u16 k = 0; k < nPairs && q + 6 <= m_data.size(); ++k, q += 6)
                m_kernPairs.emplace_back((u64(u16At(q)) << 32) | u16At(q + 2), u32(i16At(q + 4)));
        }
        if (!length) break;
        p += length;
    }
    std::sort(m_kernPairs.begin(), m_kernPairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return true;
}

void Font::parseName(u32 off) {
    u16 count = u16At(off + 2);
    u16 strOff = u16(off + u16At(off + 4));
    for (u16 i = 0; i < count; ++i) {
        u32 rec = off + 6 + u32(i) * 12;
        if (u16At(rec + 6) != 1) continue;   // family name
        u16 plat = u16At(rec), len = u16At(rec + 8), str = u16At(rec + 10);
        u32 base = strOff + str;
        if (base + len > m_data.size()) continue;
        std::string out;
        if (plat == 0 || plat == 3) {                 // UTF-16BE -> UTF-8
            for (u16 j = 0; j + 1 < len; j += 2) {
                u32 cp = u16At(base + j);
                if (cp >= 0xD800 && cp < 0xDC00 && j + 3 < len) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (u16At(base + j + 2) - 0xDC00);
                    j += 2;
                }
                if (cp < 0x80) out += char(cp);
                else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
                else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
                else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
            }
        } else
            for (u16 j = 0; j < len; ++j) out += m_data[base + j];
        if (!out.empty()) { m_metrics.family = std::move(out); break; }
    }
}

// ---- outlines ---------------------------------------------------------------------------------
// Points are collected into one flat OutlineBuffer, then replayed into the caller's sink. Composite
// transforms are composed as Xform 2x3 matrices during the descent, so nested composites need no
// callback re-wrapping.

bool Font::glyphData(u32 g, u32& off, u32& len) const {
    if (g >= m_numGlyphs) return false;
    u32 a, b;
    if (m_longLoca) { a = u32At(m_loca + g * 4); b = u32At(m_loca + g * 4 + 4); }
    else { a = u32(u16At(m_loca + g * 2)) * 2; b = u32(u16At(m_loca + g * 2 + 2)) * 2; }
    if (b <= a) { off = 0; len = 0; return true; }       // empty glyph (space)
    off = m_glyf + a;
    len = b - a;
    return off + len <= m_data.size();
}

void Font::emitSimple(u32 off, u32 len, const Xform& xf, OutlineBuffer& b) const {
    if (len < 10) return;
    i16 nContours = i16At(off);
    if (nContours < 0 || u32(nContours) * 2 + 10 > len) return;
    std::vector<u16> endPts(nContours);
    for (i16 i = 0; i < nContours; ++i) endPts[i] = u16At(off + 10 + u32(i) * 2);
    u32 nPts = nContours ? endPts[nContours - 1] + 1 : 0;
    u32 p = off + 10 + u32(nContours) * 2;
    u16 instrLen = u16At(p);
    p += 2 + instrLen;
    std::vector<u8> flags(nPts);
    for (u32 i = 0; i < nPts;) {
        if (p >= m_data.size()) return;
        u8 f = u8(m_data[p++]);
        flags[i++] = f;
        if (f & 0x08) {                               // REPEAT_FLAG: next byte is a repetition count
            if (p >= m_data.size()) return;
            for (u8 rep = u8(m_data[p++]); rep-- && i < nPts;) flags[i++] = f;
        }
    }
    f32 x = 0, y = 0;
    std::vector<Vec2> raw(nPts);
    for (u32 i = 0; i < nPts; ++i) {                  // x deltas
        if (flags[i] & 0x02) { u8 d = u8(m_data[p++]); x += (flags[i] & 0x10) ? f32(d) : -f32(d); }
        else if (!(flags[i] & 0x10)) { x += i16At(p); p += 2; }
        raw[i].x = x;
    }
    for (u32 i = 0; i < nPts; ++i) {                  // y deltas
        if (flags[i] & 0x04) { u8 d = u8(m_data[p++]); y += (flags[i] & 0x20) ? f32(d) : -f32(d); }
        else if (!(flags[i] & 0x20)) { y += i16At(p); p += 2; }
        raw[i].y = y;
    }
    u32 first = 0;
    for (i16 ci = 0; ci < nContours; ++ci) {
        u32 last = endPts[ci];
        u32 n = last - first + 1;
        auto onCurve = [&](u32 k) { return (flags[first + k] & 0x01) != 0; };
        auto pt = [&](u32 k) { return raw[first + k]; };
        u32 start = 0;
        while (start < n && !onCurve(start)) ++start;
        bool allOff = start == n;
        Vec2 sp = allOff ? (pt(n - 1) + pt(0)) * 0.5f : pt(start);
        b.moveTo(xf.apply(sp));
        Vec2 prev = sp;
        u32 k = allOff ? 0 : (start + 1) % n;
        for (u32 step = allOff ? 0 : 1; step < (allOff ? n : n + 1); ++step, k = (k + 1) % n) {
            Vec2 cp = pt(k);
            if (onCurve(k)) { b.lineTo(xf.apply(cp)); prev = cp; }
            else {                                     // off-curve: quadratic through (prev, cp, next)
                u32 nk = (k + 1) % n;
                Vec2 next = onCurve(nk) ? pt(nk) : (cp + pt(nk)) * 0.5f;
                const int STEPS = 6;                   // < 0.05 font-unit error at UI sizes
                for (int s = 1; s <= STEPS; ++s) {
                    f32 t = f32(s) / STEPS, u = 1 - t;
                    b.lineTo(xf.apply({u * u * prev.x + 2 * u * t * cp.x + t * t * next.x,
                                       u * u * prev.y + 2 * u * t * cp.y + t * t * next.y}));
                }
                prev = next;
            }
        }
        first = last + 1;
    }
}

void Font::emitGlyph(u32 g, const Xform& xf, OutlineBuffer& b, int depth) const {
    if (depth > 8) return;                            // malformed fonts can loop; spec uses <= 2 levels
    u32 off, len;
    if (!glyphData(g, off, len) || !len) return;
    if (i16At(off) >= 0) { emitSimple(off, len, xf, b); return; }
    u32 p = off + 10;                                 // composite header done
    for (;;) {
        if (p + 4 > m_data.size()) return;
        u16 flags = u16At(p), glyph = u16At(p + 2);
        p += 4;
        f32 dx = 0, dy = 0;
        if (flags & 0x0001) { dx = i16At(p); dy = i16At(p + 2); p += 4; }
        else { dx = f32(static_cast<signed char>(m_data[p])); dy = f32(static_cast<signed char>(m_data[p + 1])); p += 2; }
        Xform cx;
        if (flags & 0x0008) { f32 s = i16At(p) / 65536.f; cx = {s, 0, 0, s, dx, dy}; p += 2; }
        else if (flags & 0x0040)
            cx = {i16At(p) / 65536.f, i16At(p + 2) / 65536.f, i16At(p + 4) / 65536.f,
                  i16At(p + 6) / 65536.f, dx, dy}, p += 8;
        else cx = {1, 0, 0, 1, dx, dy};
        emitGlyph(glyph, xf.then(cx), b, depth + 1);
        if (!(flags & 0x0020)) break;                 // MORE_COMPONENTS
    }
}

bool Font::outline(u32 g, const OutlineSink& sink) const {
    OutlineBuffer b;
    emitGlyph(g, Xform{}, b, 0);
    if (b.starts.empty()) return false;
    for (u32 ci = 0; ci < b.starts.size(); ++ci) {
        u32 s = b.starts[ci];
        u32 e = ci + 1 < b.starts.size() ? b.starts[ci + 1] : u32(b.pts.size());
        if (e > s) {
            sink.moveTo(sink.ctx, b.pts[s]);
            for (u32 j = s + 1; j < e; ++j) sink.lineTo(sink.ctx, b.pts[j]);
        }
    }
    return true;
}

Rect Font::bounds(u32 g) const {
    u32 off, len;
    if (!glyphData(g, off, len) || !len) return {};
    return Rect{f32(i16At(off + 2)), f32(i16At(off + 4)), f32(i16At(off + 6) - i16At(off + 2)),
                f32(i16At(off + 8) - i16At(off + 4))};   // xMin, yMin, w, h in font units (y up)
}

} // namespace xui
