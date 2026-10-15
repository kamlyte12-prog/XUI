#include <xui/render/image_io.hpp>
#include <array>
#include <cstdio>
#include <cstring>
namespace xui {
	namespace {
		std::array<u32, 256> makeCrcTable() {
			std::array<u32, 256> t{};
			for (u32 n = 0; n < 256; ++n) { u32 c = n; for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[n] = c; }
			return t;
		}
		u32 crc32(const u8* d, std::size_t n, u32 crc = 0) {
			static const auto table = makeCrcTable();
			crc = ~crc;
			for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ d[i]) & 255] ^ (crc >> 8);
			return ~crc;
		}
		void be32(std::vector<u8>& v, u32 x) { v.push_back(u8(x >> 24)); v.push_back(u8(x >> 16)); v.push_back(u8(x >> 8)); v.push_back(u8(x)); }
		void chunk(std::vector<u8>& out, const char* type, const std::vector<u8>& data) {
			be32(out, static_cast<u32>(data.size()));
			std::vector<u8> body(type, type + 4); body.insert(body.end(), data.begin(), data.end());
			out.insert(out.end(), body.begin(), body.end());
			be32(out, crc32(body.data(), body.size()));
		}

		// Bit writer for deflate (LSB-first). Huffman codes are written most-significant-bit first, so they are bit-reversed.
		struct BitWriter {
			std::vector<u8>& out; u32 acc = 0; int n = 0;
			explicit BitWriter(std::vector<u8>& o) : out(o) {}
			void bits(u32 v, int count) { acc |= v << n; n += count; while (n >= 8) { out.push_back(u8(acc)); acc >>= 8; n -= 8; } }
			void code(u32 c, int len) { u32 r = 0; for (int i = 0; i < len; ++i) r |= ((c >> i) & 1u) << (len - 1 - i); bits(r, len); }
			void flush() { if (n > 0) { out.push_back(u8(acc)); acc = 0; n = 0; } }
		};
		void fixedLiteral(BitWriter& w, u32 sym) {
			if (sym < 144) w.code(0x30 + sym, 8); else if (sym < 256) w.code(0x190 + (sym - 144), 9);
			else if (sym < 280) w.code(sym - 256, 7); else w.code(0xC0 + (sym - 280), 8);
		}
		// Deflate (fixed Huffman). Matches are only searched at a single distance (4 bytes = one RGBA pixel, or the same byte of
		// the previous pixel): UI images are dominated by flat runs, which this captures. Not a general-purpose compressor.
		std::vector<u8> deflateRuns(const std::vector<u8>& raw) {
			static const u16 lenBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
			static const u8 lenExtra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
			std::vector<u8> out = { 0x78, 0x01 }; BitWriter w(out);
			w.bits(1, 1); w.bits(1, 2);                                   // final block, fixed Huffman
			const std::size_t dist = 4;
			for (std::size_t i = 0; i < raw.size();) {
				std::size_t len = 0;
				if (i >= dist) while (i + len < raw.size() && len < 258 && raw[i + len] == raw[i + len - dist]) ++len;
				if (len >= 3) {
					int c = 28; while (lenBase[c] > len) --c;
					fixedLiteral(w, 257 + u32(c)); w.bits(u32(len - lenBase[c]), lenExtra[c]);
					w.code(3, 5);                                          // distance code 3 = distance 4, no extra bits
					i += len;
				}
				else { fixedLiteral(w, raw[i]); ++i; }
			}
			fixedLiteral(w, 256); w.flush();
			u32 a = 1, b = 0; for (u8 c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
			be32(out, (b << 16) | a);
			return out;
		}

		// Inflate for exactly what this file writes: stored blocks and fixed-Huffman blocks with any distance.
		bool inflateSubset(const u8* d, std::size_t size, std::vector<u8>& out) {
			static const u16 lenBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
			static const u8 lenExtra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
			static const u16 distBase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
			static const u8 distExtra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
			if (size < 2) return false;
			std::size_t pos = 2; u32 acc = 0; int n = 0;
			auto need = [&](int c) { while (n < c) { if (pos >= size) return false; acc |= u32(d[pos++]) << n; n += 8; } return true; };
			auto get = [&](int c, u32& v) { if (c == 0) { v = 0; return true; } if (!need(c)) return false; v = acc & ((1u << c) - 1); acc >>= c; n -= c; return true; };
			auto huff = [&](u32& sym) {           // decode one fixed-Huffman literal/length symbol
				u32 code = 0;
				for (int len = 1; len <= 9; ++len) {
					u32 b; if (!get(1, b)) return false; code = (code << 1) | b;
					if (len == 7 && code <= 0x17) { sym = 256 + code; return true; }
					if (len == 8 && code >= 0x30 && code <= 0xBF) { sym = code - 0x30; return true; }
					if (len == 8 && code >= 0xC0 && code <= 0xC7) { sym = 280 + (code - 0xC0); return true; }
					if (len == 9 && code >= 0x190 && code <= 0x1FF) { sym = 144 + (code - 0x190); return true; }
				}
				return false;
				};
			for (;;) {
				u32 fin, type; if (!get(1, fin) || !get(2, type)) return false;
				if (type == 0) {                                                    // stored
					acc = 0; n = 0; if (pos + 4 > size) return false;
					const u32 len = d[pos] | (u32(d[pos + 1]) << 8); pos += 4;
					if (pos + len > size) return false;
					out.insert(out.end(), d + pos, d + pos + len);
					pos += len;
				}
				else if (type == 1) {
					for (;;) {
						u32 sym; if (!huff(sym)) return false;
						if (sym < 256) out.push_back(u8(sym));
						else if (sym == 256) break;
						else {
							const u32 c = sym - 257; if (c >= 29) return false;
							u32 extra; if (!get(lenExtra[c], extra)) return false; const u32 len = lenBase[c] + extra;
							u32 dc = 0; for (int i = 0; i < 5; ++i) { u32 b; if (!get(1, b)) return false; dc = (dc << 1) | b; }
							if (dc >= 30) return false;
							u32 dx; if (!get(distExtra[dc], dx)) return false; const std::size_t dist = distBase[dc] + dx;
							if (dist > out.size()) return false;
							for (u32 i = 0; i < len; ++i) out.push_back(out[out.size() - dist]);
						}
					}
				}
				else return false;
				if (fin) return true;
			}
		}
		u32 rd32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3]; }
	}

	bool writePng(const std::string& path, const Image& img) {
		if (img.width == 0 || img.height == 0 || img.rgba.size() != std::size_t(img.width) * img.height * 4) return false;
		const std::size_t stride = std::size_t(img.width) * 4;
		std::vector<u8> raw; raw.reserve((stride + 1) * img.height);
		for (u32 y = 0; y < img.height; ++y) {                                  // filter type 2 ("Up"): identical rows become zero runs
			raw.push_back(2);
			for (std::size_t x = 0; x < stride; ++x) raw.push_back(u8(img.px(0, y)[x] - (y ? img.px(0, y - 1)[x] : 0)));
		}
		std::vector<u8> out = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		std::vector<u8> ihdr; be32(ihdr, img.width); be32(ihdr, img.height); ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 });
		chunk(out, "IHDR", ihdr); chunk(out, "IDAT", deflateRuns(raw)); chunk(out, "IEND", {});
		FILE* f = std::fopen(path.c_str(), "wb");
		if (!f) return false;
		const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
		std::fclose(f); return ok;
	}

	bool readPng(const std::string& path, Image& img) {
		FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return false;
		std::vector<u8> d; u8 buf[4096]; std::size_t n;
		while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
		std::fclose(f);
		static const u8 sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		if (d.size() < 8 || std::memcmp(d.data(), sig, 8) != 0) return false;
		std::vector<u8> idat; u32 w = 0, h = 0;
		for (std::size_t p = 8; p + 12 <= d.size();) {
			const u32 len = rd32(&d[p]); const char* type = reinterpret_cast<const char*>(&d[p + 4]);
			if (p + 12 + len > d.size()) return false;
			if (!std::memcmp(type, "IHDR", 4)) {
				w = rd32(&d[p + 8]); h = rd32(&d[p + 12]);
				if (d[p + 16] != 8 || d[p + 17] != 6) return false;
			}
			else if (!std::memcmp(type, "IDAT", 4)) idat.insert(idat.end(), d.begin() + static_cast<std::ptrdiff_t>(p + 8), d.begin() + static_cast<std::ptrdiff_t>(p + 8 + len));
			p += 12 + len;
		}
		if (!w || !h || idat.size() < 2) return false;
		std::vector<u8> raw;
		if (!inflateSubset(idat.data(), idat.size(), raw)) return false;
		if (raw.size() != (std::size_t(w) * 4 + 1) * h) return false;
		img.width = w; img.height = h; img.rgba.resize(std::size_t(w) * h * 4);
		const std::size_t stride = std::size_t(w) * 4;
		for (u32 y = 0; y < h; ++y) {
			const u8 filter = raw[std::size_t(y) * (stride + 1)];
			if (filter != 0 && filter != 2) return false;
			for (std::size_t x = 0; x < stride; ++x)
				img.px(0, y)[x] = u8(raw[std::size_t(y) * (stride + 1) + 1 + x] + (filter == 2 && y ? img.px(0, y - 1)[x] : 0));
		}
		return true;
	}

	ImageDiff compareImages(const Image& a, const Image& b, u32 tol) {
		ImageDiff r;
		if (a.width != b.width || a.height != b.height) { r.sizeMismatch = true; return r; }
		for (u32 y = 0; y < a.height; ++y)
			for (u32 x = 0; x < a.width; ++x) {
				u32 worst = 0;
				for (int c = 0; c < 4; ++c) {
					const int d = std::abs(int(a.px(x, y)[c]) - int(b.px(x, y)[c]));
					worst = std::max<u32>(worst, u32(d));
				}
				r.maxDelta = std::max(r.maxDelta, worst);
				if (worst > tol) { if (!r.badPixels) { r.firstX = x; r.firstY = y; } ++r.badPixels; }
			}
		return r;
	}
	Image diffImage(const Image& a, const Image& b, u32 tol) {
		Image o; o.width = a.width; o.height = a.height; o.rgba.assign(a.rgba.size(), 255);
		if (a.width != b.width || a.height != b.height) return o;
		for (u32 y = 0; y < a.height; ++y)
			for (u32 x = 0; x < a.width; ++x) {
				u32 worst = 0; for (int c = 0; c < 4; ++c) worst = std::max<u32>(worst, u32(std::abs(int(a.px(x, y)[c]) - int(b.px(x, y)[c]))));
				u8* p = o.px(x, y);
				if (worst > tol) { p[0] = 255; p[1] = 0; p[2] = 0; p[3] = 255; }
				else { const u8 g = u8(255 - (255 - a.px(x, y)[0]) / 4); p[0] = p[1] = p[2] = g; p[3] = 255; }
			}
		return o;
	}
}
