#include <xui/platform/null_platform.hpp>
#include <algorithm>

namespace xui {

	bool NullPlatform::init(const PlatformDesc&) { return true; }
	void NullPlatform::shutdown() { m_wins.clear(); m_queue.clear(); }

	NullPlatform::Win* NullPlatform::find(WindowHandle h) {
		if (h.index >= m_wins.size()) return nullptr;
		Win& w = m_wins[h.index];
		return (w.alive && w.gen == h.gen) ? &w : nullptr;
	}

	WindowHandle NullPlatform::createWindow(const WindowDesc& d) {
		for (u32 i = 0; i < m_wins.size(); ++i)
			if (!m_wins[i].alive) {
				m_wins[i] = { std::string(d.title), d.width, d.height, true, m_wins[i].gen };
				return { i, m_wins[i].gen };
			}
		m_wins.push_back({ std::string(d.title), d.width, d.height, true, 1 });
		return { u32(m_wins.size() - 1), 1 };
	}

	void NullPlatform::destroyWindow(WindowHandle h) {
		if (Win* w = find(h)) { w->alive = false; ++w->gen; }
	}

	void NullPlatform::setWindowTitle(WindowHandle h, std::string_view t) {
		if (Win* w = find(h)) w->title.assign(t);
	}

	f32 NullPlatform::dpiScale(WindowHandle h) const { return find(h) ? m_dpi : 0.f; }

	Vec2 NullPlatform::sizeLogical(WindowHandle h) const {
		const Win* w = find(h);
		return w ? Vec2{ f32(w->w), f32(w->h) } : Vec2{};
	}
	Vec2i NullPlatform::sizePixels(WindowHandle h) const {
		const Win* w = find(h);
		return w ? Vec2i{ i32(w->w * m_dpi + 0.5f), i32(w->h * m_dpi + 0.5f) } : Vec2i{};
	}

	bool NullPlatform::pumpEvents(Event& out) {
		if (!m_queue.empty()) {                    // scripted events drain first
			out = m_queue.front();
			m_queue.pop_front();
			return true;
		}
		if (m_pendingTicks > 0) {                  // then the coalesced redraw request
			--m_pendingTicks;
			out = Event{};
			out.type = EventType::Tick;
			return true;
		}
		out = Event{};
		return false;
	}

	void NullPlatform::requestRedraw(WindowHandle w) {
		if (!find(w)) return;
		m_pendingTicks = 1;                        // coalesce: one Tick per drain cycle
	}

	void NullPlatform::simulateResize(WindowHandle w, u32 lw, u32 lh, f32 newDpi) {
		Win* win = find(w);
		if (!win) return;
		win->w = lw; win->h = lh;
		Event e{};
		e.type = EventType::Resize; e.window = w; e.w = f32(lw); e.h = f32(lh);
		m_queue.push_back(e);
		if (newDpi > 0 && newDpi != m_dpi) {
			f32 old = m_dpi;
			m_dpi = newDpi;
			Event d = e;
			d.type = EventType::DpiChange; d.scale = newDpi; d.w = old;
			m_queue.push_back(d);
		}
	}

	void NullPlatform::click(MouseButton b, f32 x, f32 y, u8 mods) {
		Event down{}, up{};
		down.type = EventType::MouseDown; up.type = EventType::MouseUp;
		down.button = up.button = b;
		down.mods = up.mods = mods;
		down.x = up.x = x; down.y = up.y = y;
		m_queue.push_back(down);
		m_queue.push_back(up);
	}

	void NullPlatform::keyTap(Key k, u8 mods) {
		Event down{}, up{};
		down.type = EventType::KeyDown; up.type = EventType::KeyUp;
		down.key = up.key = k;
		down.mods = up.mods = mods;
		m_queue.push_back(down);
		m_queue.push_back(up);
	}

	void NullPlatform::typeText(std::string_view utf8) {
		// one event per UTF-8 sequence, mirroring real text input
		for (std::size_t i = 0; i < utf8.size();) {
			unsigned char c = static_cast<unsigned char>(utf8[i]);
			std::size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
			u32 cp = 0;
			if (n == 1) cp = c;
			else {
				cp = c & (0xFFu >> (n + 1));   // leading byte: strip the marker bits
				for (std::size_t j = i + 1; j < std::min(i + n, utf8.size()); ++j)
					cp = (cp << 6) | (static_cast<unsigned char>(utf8[j]) & 0x3Fu);
			}
			Event e{};
			e.type = EventType::TextInput;
			e.codepoint = cp;
			m_queue.push_back(e);
			i += n;
		}
	}

} // namespace xui
