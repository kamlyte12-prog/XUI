#include <xui/ui/runtime.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace xui {
	namespace {
		constexpr auto kFpsRefreshInterval = std::chrono::milliseconds(250);
		constexpr auto kCaretBlinkInterval = std::chrono::milliseconds(530);
		std::string_view literal(const Document& document, const StringInterner& strings, u32 expression) {
			if (expression >= document.exprs.size()) return {};
			const XExpr& e = document.exprs[expression];
			if (e.kind == XExprKind::String || e.kind == XExprKind::Name) return strings.view(e.name);
			return {};
		}

		bool typeIs(StrId type, const StringInterner& strings, std::string_view name) {
			return strings.view(type) == name;
		}

		bool layoutLength(const XExpr& expression, Length& output) {
			if (expression.kind != XExprKind::Number) return false;
			switch (expression.unit) {
			case Unit::None: case Unit::Px: case Unit::Dp:
				output = Length::px(expression.num); return true;
			case Unit::Percent:
				output = Length::pct(expression.num); return true;
			default: return false;
			}
		}

		bool layoutPixels(const XExpr& expression, f32& output) {
			if (expression.kind != XExprKind::Number ||
				(expression.unit != Unit::None && expression.unit != Unit::Px && expression.unit != Unit::Dp)) return false;
			output = expression.num;
			return true;
		}

		bool layoutEdges(const XExpr& expression, Edges& output) {
			if (expression.kind == XExprKind::Size) {
				output = { expression.num, expression.num2, expression.num, expression.num2 };
				return true;
			}
			f32 value = 0;
			if (!layoutPixels(expression, value)) return false;
			output = Edges::all(value);
			return true;
		}

		Color hsv(f32 hue, f32 saturation, f32 value) {
			hue -= std::floor(hue);
			const f32 h = hue * 6.f, c = value * saturation, x = c * (1.f - std::fabs(std::fmod(h, 2.f) - 1.f)), m = value - c;
			Color out;
			if (h < 1.f) out = { c, x, 0, 1 }; else if (h < 2.f) out = { x, c, 0, 1 };
			else if (h < 3.f) out = { 0, c, x, 1 }; else if (h < 4.f) out = { 0, x, c, 1 };
			else if (h < 5.f) out = { x, 0, c, 1 }; else out = { c, 0, x, 1 };
			out.r += m; out.g += m; out.b += m; return out;
		}
		[[maybe_unused]] std::string colorHex(Color c) { char text[10]; std::snprintf(text, sizeof(text), "#%08x", c.toRGBA8()); return text; }
		std::string colorText(Color c, std::string_view format, bool alpha, u32 precision) {
			precision = std::min<u32>(precision, 4);
			char text[96]{};
			const f32 scale = std::pow(10.f, static_cast<f32>(precision));
			auto rounded = [scale](f32 v) { return std::round(v * scale) / scale; };
			if (format == "hex") {
				const u32 rgba = c.toRGBA8();
				if (alpha) std::snprintf(text, sizeof(text), "#%08X", rgba);
				else std::snprintf(text, sizeof(text), "#%06X", rgba >> 8);
			}
			else if (format == "rgb") {
				if (alpha) std::snprintf(text, sizeof(text), "rgba(%d, %d, %d, %.*f)", int(c.r * 255.f + 0.5f), int(c.g * 255.f + 0.5f), int(c.b * 255.f + 0.5f), int(precision), rounded(c.a));
				else std::snprintf(text, sizeof(text), "rgb(%d, %d, %d)", int(c.r * 255.f + 0.5f), int(c.g * 255.f + 0.5f), int(c.b * 255.f + 0.5f));
			}
			else {
				const f32 hi = std::max({ c.r,c.g,c.b }), lo = std::min({ c.r,c.g,c.b }), delta = hi - lo;
				f32 h = 0.f, s = 0.f, l = (hi + lo) * 0.5f;
				if (delta > 0.f) {
					if (hi == c.r) h = std::fmod((c.g - c.b) / delta, 6.f);
					else if (hi == c.g) h = (c.b - c.r) / delta + 2.f;
					else h = (c.r - c.g) / delta + 4.f;
					h *= 60.f; if (h < 0.f) h += 360.f;
					s = format == "hsv" ? delta / hi : delta / (1.f - std::fabs(2.f * l - 1.f));
				}
				if (format == "hsv") {
					if (alpha) std::snprintf(text, sizeof(text), "hsva(%.*f, %.*f%%, %.*f%%, %.*f)", int(precision), rounded(h), int(precision), rounded(s * 100), int(precision), rounded(hi * 100), int(precision), rounded(c.a));
					else std::snprintf(text, sizeof(text), "hsv(%.*f, %.*f%%, %.*f%%)", int(precision), rounded(h), int(precision), rounded(s * 100), int(precision), rounded(hi * 100));
				}
				else if (alpha) std::snprintf(text, sizeof(text), "hsla(%.*f, %.*f%%, %.*f%%, %.*f)", int(precision), rounded(h), int(precision), rounded(s * 100), int(precision), rounded(l * 100), int(precision), rounded(c.a));
				else std::snprintf(text, sizeof(text), "hsl(%.*f, %.*f%%, %.*f%%)", int(precision), rounded(h), int(precision), rounded(s * 100), int(precision), rounded(l * 100));
			}
			return text;
		}
		std::array<std::string, 5> pickerEditorValues(Color c, std::string_view format, bool alpha, u32 precision) {
			std::array<std::string, 5> values;
			const u32 rgba = c.toRGBA8();
			char text[32]{};
			if (format == "hex") {
				std::snprintf(text, sizeof(text), alpha ? "#%08X" : "#%06X", alpha ? rgba : (rgba >> 8));
				values[4] = text;
				std::snprintf(text, sizeof(text), "%u", u32(c.r * 255.f + 0.5f)); values[0] = text;
				std::snprintf(text, sizeof(text), "%u", u32(c.g * 255.f + 0.5f)); values[1] = text;
				std::snprintf(text, sizeof(text), "%u", u32(c.b * 255.f + 0.5f)); values[2] = text;
				std::snprintf(text, sizeof(text), "%.*f", int(precision), double(c.a)); values[3] = text;
			}
			else {
				if (format == "rgb") {
					std::snprintf(text, sizeof(text), "%u", u32(c.r * 255.f + 0.5f)); values[0] = text;
					std::snprintf(text, sizeof(text), "%u", u32(c.g * 255.f + 0.5f)); values[1] = text;
					std::snprintf(text, sizeof(text), "%u", u32(c.b * 255.f + 0.5f)); values[2] = text;
				}
				else {
					const f32 hi = std::max({ c.r, c.g, c.b }), lo = std::min({ c.r, c.g, c.b }), delta = hi - lo;
					f32 h = 0.f, s = 0.f, third = (hi + lo) * 0.5f;
					if (delta > 0.f) {
						if (hi == c.r) h = std::fmod((c.g - c.b) / delta, 6.f);
						else if (hi == c.g) h = (c.b - c.r) / delta + 2.f;
						else h = (c.r - c.g) / delta + 4.f;
						h *= 60.f; if (h < 0.f) h += 360.f;
						s = format == "hsv" ? delta / hi : delta / (1.f - std::fabs(2.f * third - 1.f));
					}
					if (format == "hsv") third = hi;
					std::snprintf(text, sizeof(text), "%.*f", int(precision), double(h)); values[0] = text;
					std::snprintf(text, sizeof(text), "%.*f", int(precision), double(s * 100.f)); values[1] = text;
					std::snprintf(text, sizeof(text), "%.*f", int(precision), double(third * 100.f)); values[2] = text;
				}
				std::snprintf(text, sizeof(text), "%.*f", int(precision), double(c.a)); values[3] = text;
				values[4] = colorText(c, "hex", alpha, precision);
			}
			if (!alpha && format == "hex") values[4] = colorText(c, "hex", false, precision);
			return values;
		}
		bool parseColorText(std::string_view input, Color& out) {
			while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
			while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);
			if (Color::parseHex(input, out)) return true;
			const auto open = input.find('('), close = input.rfind(')');
			if (open == std::string_view::npos || close != input.size() - 1 || close <= open) return false;
			const auto kind = input.substr(0, open);
			const bool rgb = kind == "rgb" || kind == "rgba", hsl = kind == "hsl" || kind == "hsla", hsvFormat = kind == "hsv" || kind == "hsva";
			if (!rgb && !hsl && !hsvFormat) return false;
			std::array<f32, 4> v{ 0,0,0,1 }; std::array<bool, 4> percent{}; u32 n = 0;
			std::string_view body = input.substr(open + 1, close - open - 1);
			while (!body.empty()) {
				const auto comma = body.find(','); auto part = body.substr(0, comma);
				if (n >= 4) return false;
				while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front()))) part.remove_prefix(1);
				while (!part.empty() && (std::isspace(static_cast<unsigned char>(part.back())) || part.back() == '%')) { if (part.back() == '%') percent[n] = true; part.remove_suffix(1); }
				if (n >= 4 || part.empty()) return false;
				std::string copy(part); char* end = nullptr; const float x = std::strtof(copy.c_str(), &end);
				if (end == copy.c_str() || *end != '\0' || !std::isfinite(x)) return false;
				v[n++] = x; if (comma == std::string_view::npos) break; body.remove_prefix(comma + 1);
			}
			if (n != 3 && n != 4) return false;
			if (rgb) { for (u32 i = 0;i < 3;++i) v[i] = std::clamp(percent[i] ? v[i] / 100.f : v[i] / 255.f, 0.f, 1.f); out = { v[0],v[1],v[2],std::clamp(v[3],0.f,1.f) }; return true; }
			const f32 h = std::fmod(v[0], 360.f) / 360.f, s = std::clamp(percent[1] ? v[1] / 100.f : v[1], 0.f, 1.f), third = std::clamp(percent[2] ? v[2] / 100.f : v[2], 0.f, 1.f);
			if (hsvFormat) out = hsv(h, s, third);
			else {
				const f32 chroma = (1.f - std::fabs(2.f * third - 1.f)) * s, x = chroma * (1.f - std::fabs(std::fmod(h * 6.f, 2.f) - 1.f)), m = third - chroma * 0.5f, segment = h * 6.f;
				Color c; if (segment < 1)c = { chroma,x,0,1 }; else if (segment < 2)c = { x,chroma,0,1 }; else if (segment < 3)c = { 0,chroma,x,1 }; else if (segment < 4)c = { 0,x,chroma,1 }; else if (segment < 5)c = { x,0,chroma,1 }; else c = { chroma,0,x,1 };
				out = { c.r + m,c.g + m,c.b + m,1 };
			}
			out.a = std::clamp(v[3], 0.f, 1.f); return true;
		}
		void appendUtf8(std::string& out, u32 cp) {
			if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) cp = 0xFFFDu;
			if (cp <= 0x7Fu) out.push_back(static_cast<char>(cp));
			else if (cp <= 0x7FFu) { out.push_back(static_cast<char>(0xC0u | (cp >> 6))); out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu))); }
			else if (cp <= 0xFFFFu) { out.push_back(static_cast<char>(0xE0u | (cp >> 12))); out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu))); out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu))); }
			else { out.push_back(static_cast<char>(0xF0u | (cp >> 18))); out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu))); out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu))); out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu))); }
		}
		std::size_t previousUtf8(std::string_view value, std::size_t at) {
			if (!at) return 0;
			--at;
			while (at && (static_cast<u8>(value[at]) & 0xC0u) == 0x80u) --at;
			return at;
		}
		std::size_t nextUtf8(std::string_view value, std::size_t at) {
			if (at >= value.size()) return value.size();
			++at;
			while (at < value.size() && (static_cast<u8>(value[at]) & 0xC0u) == 0x80u) ++at;
			return at;
		}
		std::string truncateUtf8(std::string_view value, u32 maxCodepoints) {
			if (!maxCodepoints) return std::string(value);
			std::size_t end = 0;
			for (u32 count = 0; count < maxCodepoints && end < value.size(); ++count) end = nextUtf8(value, end);
			return std::string(value.substr(0, end));
		}
		u32 codepointCount(std::string_view value) {
			u32 count = 0;
			for (std::size_t at = 0; at < value.size(); at = nextUtf8(value, at)) ++count;
			return count;
		}
		Rect popupBounds(Rect anchor, bool dropdown, std::size_t optionCount, f32 windowHeight, f32 pickerWidth = 196.f, f32 pickerHeight = 204.f) {
			const f32 height = dropdown ? 28.f * static_cast<f32>(optionCount) : pickerHeight;
			const f32 y = anchor.y + anchor.h + height <= windowHeight ? anchor.y + anchor.h : std::max(0.f, anchor.y - height);
			return { anchor.x, y, dropdown ? std::max(anchor.w, 150.f) : std::max(196.f,pickerWidth), height };
		}
	} // namespace

	std::string formatNumberText(f32 value, u32 precision) {
		char text[64];
		if (precision) std::snprintf(text, sizeof(text), "%.*f", static_cast<int>(std::min<u32>(precision, 6)), double(value));
		else std::snprintf(text, sizeof(text), "%.6g", double(value));
		return text;
	}

	/// Debug substitution: space -> U+00B7 middle dot, tab -> U+2192 right arrow. Both map one
	/// codepoint to one codepoint, so byte offsets into the original text stay codepoint-aligned
	/// when caret prefixes are measured through the same substitution.
	std::string visualizeWhitespace(std::string_view text) {
		std::string out;
		out.reserve(text.size());
		for (std::size_t i = 0; i < text.size();) {
			if (text[i] == ' ') { out += "\xC2\xB7"; ++i; }
			else if (text[i] == '\t') { out += "\xE2\x86\x92"; ++i; }
			else { out += text[i]; ++i; }
		}
		return out;
	}

	/// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's civil-from-days inverse).
	u32 daysFromCivil(i32 year, u32 month, u32 day) {
		i32 y = year - (month <= 2 ? 1 : 0);
		const i32 era = (y >= 0 ? y : y - 399) / 400;
		const u32 yoe = static_cast<u32>(y - era * 400);
		const u32 mp = month > 2u ? month - 3u : month + 9u;   // shifted month, always positive
		const u32 doy = (153u * mp + 2u) / 5u + day - 1u;
		const u32 doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
		return static_cast<u32>(era * 146097 + static_cast<i32>(doe) - 719468);
	}

	/// Weekday with Monday = 0 (1970-01-01 was a Thursday = 3).
	u32 weekdayMonday0(i32 year, u32 month, u32 day) { return (daysFromCivil(year, month, day) + 3u) % 7u; }

	u32 daysInMonth(i32 year, u32 month) {
		static const u32 days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (month == 2) { const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; return leap ? 29u : 28u; }
		return month >= 1 && month <= 12 ? days[month - 1] : 31u;
	}

	const char* const kMonthNames[12] = { "January", "February", "March", "April", "May", "June",
										 "July", "August", "September", "October", "November", "December" };

	/// Shifts a civil date by a signed day count, carrying month/year boundaries.
	void shiftDate(i32& year, u32& month, u32& day, i32 days) {
		while (days > 0) {
			const u32 dim = daysInMonth(year, month);
			if (day + static_cast<u32>(days) <= dim) { day = day + static_cast<u32>(days); days = 0; }
			else { days -= static_cast<i32>(dim - day + 1u); day = 1u; if (++month > 12u) { month = 1u; ++year; } }
		}
		while (days < 0) {
			if (static_cast<i32>(day) - 1 + days >= 1) { day = static_cast<u32>(static_cast<i32>(day) + days); days = 0; }
			else { days += static_cast<i32>(day) - 1; if (month == 1u) { month = 12u; --year; } else --month; day = daysInMonth(year, month); }
		}
	}

	std::string formatDateText(i32 year, u32 month, u32 day) {
		char text[32];
		std::snprintf(text, sizeof(text), "%04d-%02u-%02u", year, month, day);
		return text;
	}

	/// Human label for a captured key combination ("Ctrl+Shift+S"). Printable keys use their ASCII value.
	std::string keyLabel(Key key, u8 mods) {
		std::string combo;
		if (mods & static_cast<u8>(Mod::Ctrl)) combo += "Ctrl+";
		if (mods & static_cast<u8>(Mod::Shift)) combo += "Shift+";
		if (mods & static_cast<u8>(Mod::Alt)) combo += "Alt+";
		if (mods & static_cast<u8>(Mod::Super)) combo += "Super+";
		const u16 code = static_cast<u16>(key);
		if (code >= 32 && code <= 126 && key != Key::Space) {
			combo += static_cast<char>(code);
			return combo;
		}
		switch (key) {
		case Key::Space: combo += "Space"; break;
		case Key::Escape: combo += "Escape"; break;
		case Key::Enter: case Key::KpEnter: combo += "Enter"; break;
		case Key::Backspace: combo += "Backspace"; break;
		case Key::Insert: combo += "Insert"; break;
		case Key::Delete: combo += "Delete"; break;
		case Key::Right: combo += "Right"; break;
		case Key::Left: combo += "Left"; break;
		case Key::Down: combo += "Down"; break;
		case Key::Up: combo += "Up"; break;
		case Key::PageUp: combo += "PageUp"; break;
		case Key::PageDown: combo += "PageDown"; break;
		case Key::Home: combo += "Home"; break;
		case Key::End: combo += "End"; break;
		case Key::F1: case Key::F2: case Key::F4: case Key::F5: case Key::F6: case Key::F7:
		case Key::F8: case Key::F9: case Key::F10: case Key::F11: case Key::F12:
			combo += "F" + std::to_string(code - 0xE030u + 1u); break;
		case Key::Kp0: case Key::Kp1: case Key::Kp2: case Key::Kp3: case Key::Kp4:
		case Key::Kp5: case Key::Kp6: case Key::Kp7: case Key::Kp8: case Key::Kp9:
			combo += "Kp" + std::to_string(code - 0xE040u); break;
		case Key::KpDecimal: combo += "Kp."; break;
		case Key::KpDivide: combo += "Kp/"; break;
		case Key::KpMultiply: combo += "Kp*"; break;
		case Key::KpSubtract: combo += "Kp-"; break;
		case Key::KpAdd: combo += "Kp+"; break;
		default: combo += "Key" + std::to_string(code); break;
		}
		return combo;
	}

	bool loadUiAssets(std::string_view xuiSource, std::string_view xssSource, StringInterner& strings,
		UiAssetBundle& out, std::string& error, std::string_view xuiName, std::string_view xssName) {
		auto document = parseXui(xuiSource, xuiName, strings);
		if (!document) { error = document.error().format(); return false; }
		auto styles = compileXss(xssSource, xssName, strings);
		if (!styles) { error = styles.error().format(); return false; }
		Program program;
		program.files.push_back(std::move(*document));
		auto valid = validate(program, XuiSchema::withCoreWidgets(), strings);
		if (!valid) { error = valid.error().format(); return false; }
		out.document = std::move(program.files.front());
		out.styles = std::move(*styles);
		error.clear();
		return true;
	}

	bool UiRuntime::build(const Document& document, StyleSheet& styles, StringInterner& strings,
		TextSystem& text, FontHandle font, std::string& error) {
		if (m_root.valid() || !m_nodes.empty()) { error = "this UiRuntime already has a document"; return false; }
		if (document.roots.size() != 1) { error = "a runtime document must contain exactly one Window root"; return false; }
		if (font.valid() && (!text.font(font) || !text.atlas()->ready())) {
			error = "a valid UiRuntime font handle needs an initialized glyph atlas";
			return false;
		}

		m_dockHosts.clear();
		m_styles = &styles;
		m_strings = &strings;
		m_text = &text;
		m_font = font;
		MatchInput tooltipMatch; tooltipMatch.type = strings.intern("Tooltip");
		m_tooltipStyleSet = styles.match(tooltipMatch);

		const XNode& rootNode = document.nodes[document.roots.front()];
		if (!typeIs(rootNode.type, strings, "Window")) { error = "the root widget must be Window"; return false; }
		for (const XArg& arg : document.argsOf(rootNode)) {
			const std::string_view name = strings.view(arg.name);
			if (name == "title") {
				const std::string_view value = literal(document, strings, arg.expr);
				if (!value.empty()) m_windowTitle.assign(value);
			}
			else if (name == "size" && arg.expr < document.exprs.size()) {
				const XExpr& e = document.exprs[arg.expr];
				if (e.kind == XExprKind::Size && e.num > 0 && e.num2 > 0) {
					m_windowWidth = static_cast<u32>(e.num);
					m_windowHeight = static_cast<u32>(e.num2);
				}
			}
		}

		m_layout.setLeafMeasure(&UiRuntime::measureLeaf, this);
		if (!buildNode(document, document.roots.front(), 0xFFFFFFFFu, error)) {
			m_nodes.clear();
			m_root = NodeId{};
			return false;
		}
		m_root = m_nodes.front().layout;
		for (u32 hostIndex = 0; hostIndex < m_nodes.size(); ++hostIndex) {
			Node& hostNode = m_nodes[hostIndex];
			if (!hostNode.dockHost) continue;
			DockHostState host;
			host.node = hostIndex;
			host.gutter = hostNode.dockGutter;
			for (u32 panelIndex = 0; panelIndex < m_nodes.size(); ++panelIndex)
				if (m_nodes[panelIndex].parentIndex == hostIndex) {
					if (!m_nodes[panelIndex].dockPanel) {
						error = "DockHost may contain only DockPanel children";
						return false;
					}
					host.panelNodes.push_back(panelIndex);
				}
			if (host.panelNodes.empty()) { error = "DockHost must contain at least one DockPanel"; return false; }
			host.tree.reserve(static_cast<u32>(host.panelNodes.size()), static_cast<u32>(host.panelNodes.size() * 2 + 1));
			host.appliedBounds.resize(host.panelNodes.size(), Rect{ -1.f, -1.f, -1.f, -1.f });
			for (u32 ordinal = 0; ordinal < host.panelNodes.size(); ++ordinal) {
				const u32 panelIndex = host.panelNodes[ordinal];
				Node& panel = m_nodes[panelIndex];
				panel.dockHeaderHeight = hostNode.dockHeaderHeight;
				const LayoutStyle& style = m_layout.style(panel.layout);
				host.authoredPanelStyles.push_back(style);
				auto resolve = [](Length length, f32 base, f32 fallback) {
					if (length.k == LenKind::Px) return std::max(0.f, length.v);
					if (length.k == LenKind::Percent) return std::max(0.f, length.v * base * 0.01f);
					return fallback;
				};
				const Vec2 minimum{
					std::max(style.minW, resolve(style.minWidth, f32(m_windowWidth), 0.f)),
					std::max(style.minH, resolve(style.minHeight, f32(m_windowHeight), 0.f)) };
				const Vec2 maximum{
					std::min(style.maxW, resolve(style.maxWidth, f32(m_windowWidth), kInf)),
					std::min(style.maxH, resolve(style.maxHeight, f32(m_windowHeight), kInf)) };
				Vec2 preferred = panel.dockPreferredSize;
				if (style.width.k != LenKind::Auto) preferred.x = resolve(style.width, f32(m_windowWidth), preferred.x);
				if (style.height.k != LenKind::Auto) preferred.y = resolve(style.height, f32(m_windowHeight), preferred.y);
				preferred.x = std::max(preferred.x, minimum.x);
				preferred.y = std::max(preferred.y, minimum.y);
				if (!host.tree.addPanel(panelIndex, minimum, preferred,
					{ 28.f + f32(ordinal) * 18.f, 28.f + f32(ordinal) * 18.f, preferred.x, preferred.y }, maximum)) {
					error = "duplicate DockPanel runtime key";
					return false;
				}
				if (panel.dockFloatingInitial || panel.dockZoneName == "Floating")
					host.tree.undock(panelIndex, { 28.f + f32(ordinal) * 18.f, 28.f + f32(ordinal) * 18.f, preferred.x, preferred.y });
			}
			u32 firstDocked = 0xFFFFFFFFu;
			for (u32 panelIndex : host.panelNodes) {
				Node& panel = m_nodes[panelIndex];
				if (panel.dockFloatingInitial || panel.dockZoneName == "Floating") continue;
				if (host.tree.root() == 0xFFFFFFFFu) {
					host.tree.dockRoot(panelIndex);
					firstDocked = panelIndex;
					continue;
				}
				if (!host.tree.floating(panelIndex)) { if (firstDocked == 0xFFFFFFFFu) firstDocked = panelIndex; continue; }
				u32 target = firstDocked;
				if (!panel.dockTargetId.empty()) {
					target = 0xFFFFFFFFu;
					for (u32 candidate : host.panelNodes)
						if (m_nodes[candidate].id.v && m_strings->view(m_nodes[candidate].id) == panel.dockTargetId) { target = candidate; break; }
					if (target == 0xFFFFFFFFu) { error = "DockPanel target '" + panel.dockTargetId + "' is not in its DockHost"; return false; }
				}
				if (target == 0xFFFFFFFFu || !host.tree.contains(target)) { host.tree.dockRoot(panelIndex); firstDocked = panelIndex; continue; }
				DockZone zone = panel.dockZoneName == "Left" ? DockZone::Left : panel.dockZoneName == "Right" ? DockZone::Right :
					panel.dockZoneName == "Top" ? DockZone::Top : panel.dockZoneName == "Bottom" ? DockZone::Bottom : DockZone::Center;
				if (!host.tree.dock(panelIndex, target, zone)) { error = "could not build the authored DockHost layout"; return false; }
				if (firstDocked == 0xFFFFFFFFu) firstDocked = panelIndex;
			}
			m_dockHosts.push_back(std::move(host));
		}
		// Picker editors are retained TextBox nodes owned by the picker, but laid out in its popup
		// overlay. They use the same focus, selection, clipboard and text-input paths as authored fields.
		const u32 authoredNodeCount = static_cast<u32>(m_nodes.size());
		m_authoredNodeCount = authoredNodeCount;
		for (u32 ownerIndex = 0; ownerIndex < authoredNodeCount; ++ownerIndex) {
			const Node owner = m_nodes[ownerIndex];
			if (!owner.colorPicker || !owner.pickerShowInputs) continue;
			const auto values = pickerEditorValues(owner.pickerColor, owner.pickerFormat, owner.pickerAlpha, owner.pickerPrecision);
			for (u8 slot = 0; slot < 5; ++slot) {
				Node editor;
				editor.layout = owner.layout;
				editor.parentIndex = ownerIndex;
				editor.type = m_strings->intern("TextBox");
				editor.classes.push_back(m_strings->intern(slot == 4 ? "picker-hex" : "picker-channel"));
				editor.textBox = true;
				editor.pickerEditor = true;
				editor.pickerOwner = ownerIndex;
				editor.pickerSlot = slot;
				editor.disabled = owner.disabled;
				editor.text = values[slot];
				editor.caret = editor.text.size();
				MatchInput input; input.type = editor.type; input.classes = editor.classes;
				editor.styleSet = m_styles->match(input);
				m_styles->resolve(editor.styleSet, stateBit(State::Resting), m_theme, editor.style);
				const PropId inherited[] = { PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing };
				for (u8 i = 0; i < std::size(inherited); ++i) {
					const PropId property = inherited[i];
					if (!editor.style.has(property) && owner.style.has(property)) {
						editor.style.v[static_cast<int>(property)] = owner.style.get(property);
						editor.style.mask |= u64{ 1 } << static_cast<int>(property);
						editor.inheritedTextMask |= u8(1u << i);
					}
				}
				editor.font = selectFont(editor.style);
				m_interactiveNodes.push_back(static_cast<u32>(m_nodes.size()));
				m_nodes.push_back(std::move(editor));
			}
		}
		return true;
	}

	bool UiRuntime::setFont(FontHandle font, std::string& error) {
		if (!font.valid() || !m_text || !m_text->font(font)) { error = "font handle is invalid"; return false; }
		if (!m_text->atlas()->ready()) { error = "initialize TextSystem's atlas with a renderer before attaching its font"; return false; }
		m_font = font;
		for (Node& node : m_nodes) {
			node.font = selectFont(node.style);
			if (!node.text.empty()) m_layout.markContentChanged(node.layout);
		}
		return true;
	}

	bool UiRuntime::registerFont(std::string_view family, u16 weight, FontHandle font, std::string& error) {
		if (family.empty() || !m_text || !m_text->font(font) || !m_text->atlas()->ready()) {
			error = "registerFont needs a family name and a font loaded into an initialized TextSystem";
			return false;
		}
		const StrId familyId = m_strings ? m_strings->intern(family) : StrId{};
		if (!familyId.v) { error = "registerFont must be called after build has supplied a StringInterner"; return false; }
		auto it = std::find_if(m_fontFaces.begin(), m_fontFaces.end(), [&](const FontFace& face) { return face.family == familyId && face.weight == weight; });
		if (it == m_fontFaces.end()) m_fontFaces.push_back({ familyId, weight, font }); else it->font = font;
		for (Node& node : m_nodes) {
			node.font = selectFont(node.style);
			if (!node.text.empty()) m_layout.markContentChanged(node.layout);
		}
		return true;
	}

	bool UiRuntime::setTheme(std::string_view name) {
		if (!m_styles || !m_strings || name.empty()) return false;
		const auto index = m_styles->themeIndex(m_strings->intern(name));
		if (!index) return false;
		if (m_theme == *index) return true;
		m_theme = *index;
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			const StateMask states = activeStates(node);
			m_styles->resolve(node.styleSet, states, m_theme, node.style);
			node.resolvedStates = states;
			node.styleResolved = true;
			node.inheritedTextMask = 0;
			if (node.parentIndex < i) {
				const PropId inherited[] = { PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing };
				const PropTable& parentStyle = m_nodes[node.parentIndex].style;
				for (u8 propertyIndex = 0; propertyIndex < std::size(inherited); ++propertyIndex) {
					const PropId property = inherited[propertyIndex];
					if (!node.style.has(property) && parentStyle.has(property)) {
						node.style.v[static_cast<int>(property)] = parentStyle.get(property);
						node.style.mask |= u64{ 1 } << static_cast<int>(property);
						node.inheritedTextMask |= u8(1u << propertyIndex);
					}
				}
			}
			node.font = selectFont(node.style);
		}
		m_layout.markAllDirty(m_root);
		addDamage({ 0, 0, m_lastLayoutWidth, m_lastLayoutHeight });
		return true;
	}

	bool UiRuntime::buildNode(const Document& document, u32 index, u32 parentIndex, std::string& error) {
		if (index >= document.nodes.size()) { error = "invalid node index in .xui document"; return false; }
		const XNode& source = document.nodes[index];
		const u32 runtimeIndex = static_cast<u32>(m_nodes.size());
		Node node;
		node.type = source.type;
		node.parentIndex = parentIndex;
		node.button = typeIs(source.type, *m_strings, "Button");
		node.checkItem = typeIs(source.type, *m_strings, "CheckItem");
		node.checkBox = typeIs(source.type, *m_strings, "CheckBox");
		node.split = typeIs(source.type, *m_strings, "Split");
		node.dockHost = typeIs(source.type, *m_strings, "DockHost") || typeIs(source.type, *m_strings, "DockSpace");
		node.dockPanel = typeIs(source.type, *m_strings, "DockPanel");
		node.scrollView = typeIs(source.type, *m_strings, "ScrollView");
		node.slider = typeIs(source.type, *m_strings, "Slider");
		node.progressBar = typeIs(source.type, *m_strings, "ProgressBar");
		node.dropdown = typeIs(source.type, *m_strings, "Dropdown");
		node.colorPicker = typeIs(source.type, *m_strings, "ColorPicker");
		node.textBox = typeIs(source.type, *m_strings, "TextBox");
		node.textArea = typeIs(source.type, *m_strings, "TextArea");
		node.tabs = typeIs(source.type, *m_strings, "Tabs");
		node.tab = typeIs(source.type, *m_strings, "Tab");
		node.fpsCounter = typeIs(source.type, *m_strings, "FpsCounter");
		node.radioGroup = typeIs(source.type, *m_strings, "RadioGroup");
		node.switchToggle = typeIs(source.type, *m_strings, "Switch");
		node.toggleButton = typeIs(source.type, *m_strings, "ToggleButton");
		node.numberField = typeIs(source.type, *m_strings, "NumberField");
		node.searchField = typeIs(source.type, *m_strings, "SearchField");
		node.label = typeIs(source.type, *m_strings, "Label");
		node.badge = typeIs(source.type, *m_strings, "Badge");
		node.separator = typeIs(source.type, *m_strings, "Separator") || typeIs(source.type, *m_strings, "Divider");
		node.spinner = typeIs(source.type, *m_strings, "Spinner");
		node.image = typeIs(source.type, *m_strings, "Image");
		node.datePicker = typeIs(source.type, *m_strings, "DatePicker");
		node.segmented = typeIs(source.type, *m_strings, "SegmentedControl");
		node.stepper = typeIs(source.type, *m_strings, "Stepper");
		node.rating = typeIs(source.type, *m_strings, "Rating");
		node.breadcrumb = typeIs(source.type, *m_strings, "Breadcrumb");
		node.keybindField = typeIs(source.type, *m_strings, "KeybindField");
		if (node.numberField) { node.textBox = true; node.value = 0.f; }
		if (node.searchField) node.textBox = true;
		if (node.rating) { node.minValue = 0.f; node.maxValue = 5.f; }   // an authored max overrides below
		if (node.stepper) { node.value = 0.f; node.minValue = 0.f; node.maxValue = 100.f; }

		if (typeIs(source.type, *m_strings, "Window") || typeIs(source.type, *m_strings, "Column") ||
			typeIs(source.type, *m_strings, "Panel") || typeIs(source.type, *m_strings, "CheckList") || node.dockPanel) {
			node.layout = m_layout.create(LayoutKind::Column);
		}
		else if (node.dockHost) node.layout = m_layout.create(LayoutKind::Stack);
		else if (typeIs(source.type, *m_strings, "Row") || typeIs(source.type, *m_strings, "Toolbar") || typeIs(source.type, *m_strings, "StatusBar")) {
			node.layout = m_layout.create(LayoutKind::Row);
		}
		else if (node.split) {
			node.layout = m_layout.create(LayoutKind::Split);
		}
		else if (node.scrollView) {
			node.layout = m_layout.create(LayoutKind::Scroll);
		}
		else if (node.tabs) {
			node.layout = m_layout.create(LayoutKind::Stack);
		}
		else if (node.tab) {
			node.layout = m_layout.create(LayoutKind::Column);
		}
		else if (typeIs(source.type, *m_strings, "Text") || node.button || node.checkItem || node.checkBox || node.slider || node.progressBar || node.fpsCounter || node.dropdown || node.colorPicker || node.textBox || node.textArea || node.radioGroup || node.switchToggle || node.toggleButton || node.label || node.badge || node.separator || node.spinner || node.image || node.datePicker || node.segmented || node.stepper || node.rating || node.breadcrumb || node.keybindField) {
			node.layout = m_layout.create(LayoutKind::Leaf);
		}
		else {
			error = "unsupported runtime widget '" + std::string(m_strings->view(source.type)) + "'";
			return false;
		}

		if (node.layout.index != m_nodes.size()) { error = "runtime node storage lost its layout index invariant"; return false; }
		if (parentIndex != 0xFFFFFFFFu && !m_layout.append(m_nodes[parentIndex].layout, node.layout)) {
			error = "could not attach .xui child to its parent";
			return false;
		}
		if (node.dockPanel && (parentIndex == 0xFFFFFFFFu || !m_nodes[parentIndex].dockHost)) {
			error = "DockPanel must be a direct child of DockHost";
			return false;
		}

		if ((typeIs(source.type, *m_strings, "Text") || node.button || node.checkItem || node.checkBox || node.fpsCounter || node.label || node.badge || node.toggleButton) && source.argCount) {
			node.text.assign(literal(document, *m_strings, document.args[source.argFirst].expr));
		}
		if (node.tab && source.argCount) node.tabTitle.assign(literal(document, *m_strings, document.args[source.argFirst].expr));
		if (node.dockPanel) {
			for (const XArg& arg : document.argsOf(source))
				if (m_strings->view(arg.name) == "title") node.text.assign(literal(document, *m_strings, arg.expr));
		}
		if (node.image && source.argCount) node.imageSource.assign(literal(document, *m_strings, document.args[source.argFirst].expr));
		for (const XArg& arg : document.argsOf(source)) {
			const std::string_view name = m_strings->view(arg.name);
			if (name == "checked" && arg.expr < document.exprs.size())
				node.checked = document.exprs[arg.expr].kind == XExprKind::Bool && document.exprs[arg.expr].bval;
			if (node.tabs && name == "selected" && arg.expr < document.exprs.size() && document.exprs[arg.expr].kind == XExprKind::Number)
				node.selectedTab = static_cast<u32>(std::max(0.f, document.exprs[arg.expr].num));
			if (node.split && name == "ratio" && arg.expr < document.exprs.size() && document.exprs[arg.expr].kind == XExprKind::Number)
				node.splitRatio = std::clamp(document.exprs[arg.expr].num, 0.05f, 0.95f);
		}
		for (const XProp& prop : document.propsOf(source)) {
			const std::string_view name = m_strings->view(prop.name);
			const std::string_view value = literal(document, *m_strings, prop.expr);
			if (node.dockHost && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number) {
				if (name == "gutter") node.dockGutter = std::max(0.f, document.exprs[prop.expr].num);
				else if (name == "headerHeight") node.dockHeaderHeight = std::clamp(document.exprs[prop.expr].num, 20.f, 64.f);
			}
			if (node.dockPanel && prop.role == XPropRole::Value) {
				if (name == "dock" || name == "region") node.dockZoneName.assign(value);
				else if (name == "target") node.dockTargetId.assign(value);
				else if (name == "icon") node.dockIcon.assign(value);
				else if (name == "floating" && prop.expr < document.exprs.size())
					node.dockFloatingInitial = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
				else if (name == "closable" && prop.expr < document.exprs.size())
					node.dockClosable = document.exprs[prop.expr].kind != XExprKind::Bool || document.exprs[prop.expr].bval;
				else if (name == "preferredSize" && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Size)
					node.dockPreferredSize = { document.exprs[prop.expr].num, document.exprs[prop.expr].num2 };
			}
			if (node.scrollView && name == "scrollSpeed" && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
				{ node.scrollSpeed = std::clamp(document.exprs[prop.expr].num, 1.f, 400.f); node.xuiScrollSpeed = true; }
			else if (node.scrollView && name == "scrollBehavior") { node.scrollSmooth = value == "smooth" || value == "Smooth"; node.xuiScrollBehavior = true; }
			else if (node.scrollView && name == "scrollbar") {
				if (value == "always" || value == "Always") node.scrollBarVisibility = 1;
				else if (value == "never" || value == "Never" || value == "hidden") node.scrollBarVisibility = 2;
				node.xuiScrollbarVisibility = true;
			}
			else if (node.scrollView && name == "horizontal" && prop.expr < document.exprs.size()) {
				LayoutStyle layout = m_layout.style(node.layout);
				layout.scrollX = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
				m_layout.setStyle(node.layout, layout);
			}
			else if (node.scrollView && name == "vertical" && prop.expr < document.exprs.size()) {
				LayoutStyle layout = m_layout.style(node.layout);
				layout.scrollY = document.exprs[prop.expr].kind != XExprKind::Bool || document.exprs[prop.expr].bval;
				m_layout.setStyle(node.layout, layout);
			}
			if ((node.numberField || node.stepper) && prop.role == XPropRole::Value && prop.expr < document.exprs.size() &&
				(name == "value" || name == "min" || name == "max" || name == "step" || name == "precision" || name == "unit")) {
				const XExpr& expression = document.exprs[prop.expr];
				if (name == "unit") node.unit.assign(value);
				else if (name == "precision" && expression.kind == XExprKind::Number)
					node.numberPrecision = static_cast<u32>(std::clamp(expression.num, 0.f, 6.f));
				else if (name == "step" && expression.kind == XExprKind::Number)
					node.stepValue = expression.num != 0.f ? expression.num : 1.f;
				else if (name == "min" && expression.kind == XExprKind::Number) { node.minValue = expression.num; node.numberBounded = true; }
				else if (name == "max" && expression.kind == XExprKind::Number) { node.maxValue = expression.num; node.numberBounded = true; }
				else if (name == "value" && expression.kind == XExprKind::Number) node.value = expression.num;
			}
			else if (name == "label" && (node.button || node.checkItem || node.switchToggle)) node.text.assign(value);
			else if (name == "label" && (node.checkBox || node.toggleButton)) node.text.assign(value);
			else if ((node.label || node.badge) && name == "text") node.text.assign(value);
			else if (name == "tooltip" && prop.role == XPropRole::Value) node.tooltipText.assign(value);
			else if (name == "placeholder" && prop.role == XPropRole::Value) node.placeholder.assign(value);
			else if (node.colorPicker && name == "format" && prop.role == XPropRole::Value) node.pickerFormat.assign(value);
			else if (node.colorPicker && name == "formats" && prop.role == XPropRole::Value) {
				node.pickerFormats.clear(); std::size_t begin = 0;
				while (begin <= value.size()) {
					const std::size_t end = value.find('|', begin); std::string item(value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin));
					if (item == "hex" || item == "rgb" || item == "hsl" || item == "hsv") node.pickerFormats.push_back(std::move(item));
					if (end == std::string_view::npos) break;
					begin = end + 1;
				}
			}
			else if (node.colorPicker && name == "precision" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
				node.pickerPrecision = static_cast<u32>(std::clamp(document.exprs[prop.expr].num, 0.f, 4.f));
			else if (node.colorPicker && name == "popupSize" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Size) {
				const XExpr& size = document.exprs[prop.expr];
				node.pickerWidth = std::max(260.f, size.num); node.pickerHeight = std::max(340.f, size.num2);
			}
			else if (node.colorPicker && (name == "alpha" || name == "showAlpha" || name == "showInputs" || name == "showFormatSwitcher" || name == "showHue" || name == "showSaturationValue") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				const bool enabled = document.exprs[prop.expr].kind != XExprKind::Bool || document.exprs[prop.expr].bval;
				if (name == "alpha") node.pickerAlpha = enabled;
				if (name == "showAlpha") node.pickerShowAlpha = enabled;
				if (name == "showInputs") node.pickerShowInputs = enabled;
				if (name == "showFormatSwitcher") node.pickerShowFormatSwitcher = enabled;
				if (name == "showHue") node.pickerShowHue = enabled;
				if (name == "showSaturationValue") node.pickerShowSaturationValue = enabled;
			}
			else if (((name == "value" && (node.textBox || node.textArea || node.colorPicker || node.dropdown || node.radioGroup || node.segmented || node.breadcrumb || node.datePicker || node.keybindField)) || (name == "selected" && node.tabs)) && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				const XExpr& expression = document.exprs[prop.expr];
				if ((node.textBox || node.textArea) && name == "value") node.text = value;
				else if (node.tabs && name == "selected" && expression.kind == XExprKind::Number) node.selectedTab = static_cast<u32>(std::max(0.f, expression.num));
				else if (node.colorPicker && name == "value" && expression.kind == XExprKind::Color) node.pickerColor = Color::fromRGBA8(expression.color);
				else if (node.colorPicker && name == "value" && expression.kind == XExprKind::String) { Color parsed; if (parseColorText(value, parsed)) node.pickerColor = parsed; }
				else if ((node.dropdown || node.radioGroup || node.segmented || node.breadcrumb) && name == "value") {
					for (u32 i = 0; i < node.options.size(); ++i) {
						if (node.options[i] == value) node.selectedOption = i;
					}
				}
				else if (node.datePicker && name == "value") {
					i32 year = 0; u32 month = 0, day = 0;
					if (std::sscanf(value.data(), "%4d-%2u-%2u", &year, &month, &day) == 3 && month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month)) {
						node.dateYear = year; node.dateMonth = month; node.dateDay = day;
						node.viewYear = year; node.viewMonth = month;
					}
				}
				else if (node.keybindField && name == "value") node.text.assign(value);
			}
			else if (name == "maxLength" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
				node.maxLength = static_cast<u32>(std::max(0.f, document.exprs[prop.expr].num));
			else if (name == "tabSize" && node.textArea && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
				node.tabSize = static_cast<u32>(std::clamp(document.exprs[prop.expr].num, 1.f, 16.f));
			else if ((name == "tooltipShowDelay" || name == "tooltipHideDelay") && prop.role == XPropRole::Value &&
				prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number) {
				const u32 delay = static_cast<u32>(std::clamp(document.exprs[prop.expr].num, 0.f, 60000.f));
				if (name == "tooltipShowDelay") node.tooltipShowDelayMs = delay;
				else node.tooltipHideDelayMs = delay;
			}
			else if (name == "tooltipTrigger" && prop.role == XPropRole::Value) {
				const std::string_view trigger = literal(document, *m_strings, prop.expr);
				if (trigger == "none") node.tooltipTrigger = 0;
				else if (trigger == "hover") node.tooltipTrigger = 1;
				else if (trigger == "focus") node.tooltipTrigger = 2;
				else if (trigger == "both") node.tooltipTrigger = 3;
			}
			else if ((name == "tooltipHideOnClick" || name == "tooltipDisabled") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				const bool flag = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
				if (name == "tooltipHideOnClick") node.tooltipHideOnClick = flag;
				else node.tooltipDisabled = flag;
			}
			else if (name == "showFrameTime" && prop.role == XPropRole::Value && prop.expr < document.exprs.size())
				node.showFrameTime = !(document.exprs[prop.expr].kind == XExprKind::Bool && !document.exprs[prop.expr].bval);
			else if (node.image && name == "fit" && prop.role == XPropRole::Value) {
				if (value == "Fill") node.imageFit = 0;
				else if (value == "Cover") node.imageFit = 2;
				else node.imageFit = 1;   // Contain is the default
			}
			else if (node.image && name == "source" && prop.role == XPropRole::Value) node.imageSource.assign(value);
			else if (node.progressBar && name == "indeterminate" && prop.role == XPropRole::Value && prop.expr < document.exprs.size())
				node.indeterminate = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
			else if ((name == "readOnly" || name == "password") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				const bool flag = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
				if (name == "readOnly") node.readOnly = flag;
				else node.password = flag;
			}
			else if (name == "id" && prop.role == XPropRole::Value) node.id = m_strings->intern(value);
			else if (name == "class" && prop.role == XPropRole::Value) {
				std::size_t start = 0;
				while (start < value.size()) {
					while (start < value.size() && value[start] == ' ') ++start;
					std::size_t end = start;
					while (end < value.size() && value[end] != ' ') ++end;
					if (end > start) node.classes.push_back(m_strings->intern(value.substr(start, end - start)));
					start = end;
				}
			}
			else if (name == "press" && prop.role == XPropRole::Event && prop.expr < document.exprs.size()) {
				const XExpr& handler = document.exprs[prop.expr];
				if (handler.kind == XExprKind::Name) node.pressHandler = handler.name;
			}
			else if (name == "change" && prop.role == XPropRole::Event && prop.expr < document.exprs.size()) {
				const XExpr& handler = document.exprs[prop.expr];
				if (handler.kind == XExprKind::Name) node.pressHandler = handler.name;
			}
			else if ((name == "dock" || name == "undock" || name == "resize" || name == "tabChange" || name == "close") &&
				prop.role == XPropRole::Event && prop.expr < document.exprs.size()) {
				const XExpr& handler = document.exprs[prop.expr];
				if (handler.kind == XExprKind::Name) {
					if (name == "dock") node.dockHandler.assign(m_strings->view(handler.name));
					else if (name == "undock") node.undockHandler.assign(m_strings->view(handler.name));
					else if (name == "resize") node.resizeHandler.assign(m_strings->view(handler.name));
					else if (name == "tabChange") node.tabChangeHandler.assign(m_strings->view(handler.name));
					else node.closeHandler.assign(m_strings->view(handler.name));
				}
			}
			else if (name == "disabled" && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				node.disabled = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
			}
			else if (name == "options" && prop.role == XPropRole::Value) {
				std::size_t begin = 0;
				while (begin <= value.size()) {
					const std::size_t end = value.find('|', begin);
					std::string option(value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin));
					if (!option.empty()) node.options.push_back(std::move(option));
					if (end == std::string_view::npos) break;
					begin = end + 1;
				}
			}
			else if ((name == "value" || name == "min" || name == "max") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				const XExpr& expression = document.exprs[prop.expr];
				if (expression.kind == XExprKind::Number) {
					if (name == "value") node.value = expression.num;
					else if (name == "min") node.minValue = expression.num;
					else node.maxValue = expression.num;
				}
				else if (name == "value" && node.colorPicker && expression.kind == XExprKind::Color)
					node.pickerColor = Color::fromRGBA8(expression.color);
				else if (name == "value" && node.dropdown)
					for (u32 i = 0; i < node.options.size(); ++i) if (node.options[i] == value) node.selectedOption = i;
			}
			else if (name == "checked" && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
				node.checked = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
			}
			else if (prop.role == XPropRole::If || prop.role == XPropRole::For) {
				error = "runtime .xui subset does not yet evaluate if/for control-flow properties";
				return false;
			}
		}

		if (node.colorPicker) {
			if (node.pickerFormats.empty()) node.pickerFormats = { "hex" };
			if (std::find(node.pickerFormats.begin(), node.pickerFormats.end(), node.pickerFormat) == node.pickerFormats.end()) node.pickerFormat = node.pickerFormats.front();
			if (!node.pickerAlpha) node.pickerColor.a = 1.f;
			node.text = colorText(node.pickerColor, node.pickerFormat, node.pickerAlpha, node.pickerPrecision);
		}
		MatchInput match;
		match.type = node.type;
		match.id = node.id;
		match.classes = node.classes;
		node.styleSet = m_styles->match(match);
		if (node.dockPanel || node.dockHost) {
			auto partSet = [&](std::string_view type) {
				MatchInput part;
				part.type = m_strings->intern(type);
				part.id = node.id;
				part.classes = node.classes;
				return m_styles->match(part);
			};
			node.dockHeaderStyleSet = partSet("DockHeader");
			node.dockTabStyleSet = partSet("DockTab");
			node.dockCloseStyleSet = partSet("DockCloseButton");
			node.dockSplitterStyleSet = partSet("DockSplitter");
			node.dockPreviewStyleSet = partSet("DockPreview");
		}
		m_styles->resolve(node.styleSet, stateBit(State::Resting), m_theme, node.style);
		if (node.scrollView) {
			if (node.style.has(PropId::ScrollbarSize)) node.scrollBarSize = std::clamp(number(node.style.get(PropId::ScrollbarSize), 0, 10.f), 4.f, 24.f);
			if (!node.xuiScrollSpeed && node.style.has(PropId::ScrollSpeed)) node.scrollSpeed = std::clamp(number(node.style.get(PropId::ScrollSpeed), 0, 42.f), 1.f, 400.f);
			if (!node.xuiScrollbarVisibility && node.style.has(PropId::ScrollbarVisibility)) {
				const Atom& mode = node.style.get(PropId::ScrollbarVisibility).atoms[0];
				if (mode.kind == AtomKind::Ident) {
					const std::string_view value = m_strings->view(mode.str());
					if (value == "always") node.scrollBarVisibility = 1;
					else if (value == "never" || value == "hidden") node.scrollBarVisibility = 2;
				}
			}
			if (!node.xuiScrollBehavior && node.style.has(PropId::ScrollBehavior)) {
				const Atom& behavior = node.style.get(PropId::ScrollBehavior).atoms[0];
				node.scrollSmooth = behavior.kind == AtomKind::Ident && m_strings->view(behavior.str()) == "smooth";
			}
		}
		if (parentIndex != 0xFFFFFFFFu && parentIndex < m_nodes.size()) {
			const PropId inherited[] = { PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing };
			const PropTable& parentStyle = m_nodes[parentIndex].style;
			for (u8 i = 0; i < std::size(inherited); ++i) {
				const PropId property = inherited[i];
				if (!node.style.has(property) && parentStyle.has(property)) {
					node.style.v[static_cast<int>(property)] = parentStyle.get(property);
					node.style.mask |= u64{ 1 } << static_cast<int>(property);
					node.inheritedTextMask |= u8(1u << i);
				}
			}
		}
		node.font = selectFont(node.style);
		applyLayoutStyle(node);
		if (node.split) {
			LayoutStyle splitStyle = m_layout.style(node.layout);
			splitStyle.splitDir = SplitDir::Horizontal;
			for (const XArg& arg : document.argsOf(source)) {
				const std::string_view name = m_strings->view(arg.name);
				if (name == "direction" && arg.expr < document.exprs.size()) {
					const XExpr& expr = document.exprs[arg.expr];
					if (expr.kind == XExprKind::Name && m_strings->view(expr.name) == "Vertical")
						splitStyle.splitDir = SplitDir::Vertical;
				}
			}
			for (const XProp& prop : document.propsOf(source)) {
				if (m_strings->view(prop.name) == "gutter" && prop.expr < document.exprs.size()) {
					const XExpr& expr = document.exprs[prop.expr];
					if (expr.kind == XExprKind::Number) splitStyle.gutter = std::max(0.f, expr.num);
				}
				if (m_strings->view(prop.name) == "ratio" && prop.expr < document.exprs.size()) {
					const XExpr& expr = document.exprs[prop.expr];
					if (expr.kind == XExprKind::Number) node.splitRatio = std::clamp(expr.num, 0.05f, 0.95f);
				}
			}
			m_layout.setStyle(node.layout, splitStyle);
		}

		// .xui owns structural layout. Apply its geometry after stylesheet defaults so authored
		// size/spacing/flex values take precedence while .xss continues to supply visual defaults.
		LayoutStyle authoredLayout = m_layout.style(node.layout);
		auto failLayout = [&](std::string_view property) {
			error = "unsupported value for .xui layout property '" + std::string(property) + "'";
			return false;
			};
		auto parseGap = [&](const XExpr& expression, std::string_view property) {
			if (expression.kind == XExprKind::Size) {
				authoredLayout.gap = { std::max(0.f, expression.num), std::max(0.f, expression.num2) };
				return true;
			}
			f32 value = 0;
			if (!layoutPixels(expression, value)) return failLayout(property);
			const f32 gap = std::max(0.f, value);
			if (typeIs(source.type, *m_strings, "Row")) authoredLayout.gap.x = gap;
			else authoredLayout.gap.y = gap;
			return true;
			};
		// Row/Column accept gap as a named constructor argument; CheckList accepts it as a property.
		for (const XArg& arg : document.argsOf(source)) {
			if (m_strings->view(arg.name) == "gap" && arg.expr < document.exprs.size() &&
				!parseGap(document.exprs[arg.expr], "gap")) return false;
		}
		for (const XProp& prop : document.propsOf(source)) {
			if (prop.role != XPropRole::Value || prop.expr >= document.exprs.size()) continue;
			const std::string_view name = m_strings->view(prop.name);
			const XExpr& expression = document.exprs[prop.expr];
			if (name == "grow" || name == "shrink") {
				f32 value = 0;
				if (!layoutPixels(expression, value)) return failLayout(name);
				if (name == "grow") authoredLayout.grow = std::max(0.f, value);
				else authoredLayout.shrink = std::max(0.f, value);
			}
			else if (name == "size" || name == "minSize" || name == "maxSize") {
				f32 width = 0, height = 0;
				if (expression.kind == XExprKind::Size) { width = expression.num; height = expression.num2; }
				else if (!layoutPixels(expression, width)) return failLayout(name);
				else height = width;
				if (name == "size") { authoredLayout.width = Length::px(width); authoredLayout.height = Length::px(height); }
				else if (name == "minSize") { authoredLayout.minW = std::max(0.f, width); authoredLayout.minH = std::max(0.f, height); }
				else { authoredLayout.maxW = std::max(0.f, width); authoredLayout.maxH = std::max(0.f, height); }
			}
			else if (name == "width" || name == "height") {
				Length value;
				if (!layoutLength(expression, value)) return failLayout(name);
				if (name == "width") authoredLayout.width = value; else authoredLayout.height = value;
			}
			else if (name == "minWidth" || name == "maxWidth" || name == "minHeight" || name == "maxHeight") {
				Length value;
				if (!layoutLength(expression, value)) return failLayout(name);
				if (name == "minWidth") authoredLayout.minWidth = value;
				else if (name == "maxWidth") authoredLayout.maxWidth = value;
				else if (name == "minHeight") authoredLayout.minHeight = value;
				else authoredLayout.maxHeight = value;
			}
			else if (name == "pad" || name == "margin") {
				Edges value;
				if (!layoutEdges(expression, value)) return failLayout(name);
				if (name == "pad") { authoredLayout.padding = value; node.hasXuiPadding = true; node.xuiPadding = value; }
				else authoredLayout.margin = value;
			}
			else if (name == "align") {
				if (expression.kind != XExprKind::Name) return failLayout(name);
				const std::string_view value = m_strings->view(expression.name);
				Align alignValue = Align::Auto;
				if (value == "Start") alignValue = Align::Start;
				else if (value == "Center") alignValue = Align::Center;
				else if (value == "End") alignValue = Align::End;
				else if (value == "Stretch") alignValue = Align::Stretch;
				else return failLayout(name);
				const bool container = typeIs(source.type, *m_strings, "Window") || typeIs(source.type, *m_strings, "Column") ||
					typeIs(source.type, *m_strings, "Row") || typeIs(source.type, *m_strings, "Panel") ||
					typeIs(source.type, *m_strings, "CheckList") || typeIs(source.type, *m_strings, "Tab") || node.dockPanel || node.dockHost;
				if (container) authoredLayout.alignItems = alignValue;
				else authoredLayout.alignSelf = alignValue;
			}
			else if (name == "justify") {
				if (expression.kind != XExprKind::Name) return failLayout(name);
				const std::string_view value = m_strings->view(expression.name);
				if (value == "Start") authoredLayout.justify = Justify::Start;
				else if (value == "Center") authoredLayout.justify = Justify::Center;
				else if (value == "End") authoredLayout.justify = Justify::End;
				else if (value == "SpaceBetween") authoredLayout.justify = Justify::SpaceBetween;
				else if (value == "SpaceAround") authoredLayout.justify = Justify::SpaceAround;
				else if (value == "SpaceEvenly") authoredLayout.justify = Justify::SpaceEvenly;
				else return failLayout(name);
			}
			else if (name == "position") {
				if (expression.kind != XExprKind::Name) return failLayout(name);
				const std::string_view value = m_strings->view(expression.name);
				if (value == "Flow" || value == "flow") authoredLayout.position = Position::Flow;
				else if (value == "Absolute" || value == "absolute") authoredLayout.position = Position::Absolute;
				else return failLayout(name);
			}
			else if (name == "left" || name == "top" || name == "right" || name == "bottom") {
				Length value;
				if (!layoutLength(expression, value)) return failLayout(name);
				if (name == "left") authoredLayout.left = value;
				else if (name == "top") authoredLayout.top = value;
				else if (name == "right") authoredLayout.right = value;
				else authoredLayout.bottom = value;
			}
			else if (name == "gap" && typeIs(source.type, *m_strings, "CheckList")) {
				if (!parseGap(expression, name)) return false;
			}
		}
		if (node.dockPanel && parentIndex < m_nodes.size()) authoredLayout.padding.t += m_nodes[parentIndex].dockHeaderHeight;
		m_layout.setStyle(node.layout, authoredLayout);
		m_nodes.push_back(std::move(node));
		if (m_nodes[runtimeIndex].dropdown || m_nodes[runtimeIndex].colorPicker) {
			Node& control = m_nodes[runtimeIndex];
			if (control.dropdown) {
				if (control.options.empty()) control.options = { "Option 1", "Option 2", "Option 3" };
				if (control.selectedOption < control.options.size()) control.text = control.options[control.selectedOption];
			}
			else { control.text = colorText(control.pickerColor, control.pickerFormat, control.pickerAlpha, control.pickerPrecision); }
		}
		if (Node& group = m_nodes[runtimeIndex]; group.radioGroup || group.segmented || group.breadcrumb) {
			if (group.options.empty()) group.options = { "Option 1", "Option 2", "Option 3" };
			group.selectedOption = std::min(group.selectedOption, static_cast<u32>(group.options.size() - 1));
		}
		if (Node& picker = m_nodes[runtimeIndex]; picker.datePicker)
			picker.text = formatDateText(picker.dateYear, picker.dateMonth, picker.dateDay);
		if (m_nodes[runtimeIndex].textBox || m_nodes[runtimeIndex].textArea) {
			Node& field = m_nodes[runtimeIndex];
			field.text = truncateUtf8(field.text, field.maxLength);
			field.caret = field.text.size();
		}
		if (Node& field = m_nodes[runtimeIndex]; field.numberField || field.stepper) {
			if (field.numberBounded || field.stepper) field.value = std::clamp(field.value, std::min(field.minValue, field.maxValue), std::max(field.minValue, field.maxValue));
			field.text = formatNumberText(field.value, field.numberPrecision);
			if (!field.unit.empty()) { field.text += " "; field.text += field.unit; }
			field.caret = field.text.size();
		}
		if (m_nodes[runtimeIndex].button || m_nodes[runtimeIndex].checkItem || m_nodes[runtimeIndex].checkBox || m_nodes[runtimeIndex].scrollView || m_nodes[runtimeIndex].slider || m_nodes[runtimeIndex].dropdown || m_nodes[runtimeIndex].colorPicker || m_nodes[runtimeIndex].textBox || m_nodes[runtimeIndex].textArea || m_nodes[runtimeIndex].tabs ||
			m_nodes[runtimeIndex].switchToggle || m_nodes[runtimeIndex].toggleButton || m_nodes[runtimeIndex].radioGroup ||
			m_nodes[runtimeIndex].segmented || m_nodes[runtimeIndex].stepper || m_nodes[runtimeIndex].rating || m_nodes[runtimeIndex].breadcrumb || m_nodes[runtimeIndex].keybindField || m_nodes[runtimeIndex].datePicker) m_interactiveNodes.push_back(runtimeIndex);
		if (m_nodes[runtimeIndex].split) m_splitNodes.push_back(runtimeIndex);

		for (u32 child : document.childrenOf(source))
			if (!buildNode(document, child, runtimeIndex, error)) return false;
		if (m_nodes[runtimeIndex].tabs) {
			u32 pageCount = 0;
			for (NodeId child = m_layout.firstChild(m_nodes[runtimeIndex].layout); child.valid(); child = m_layout.nextSibling(child)) {
				if (!m_nodes[child.index].tab) { error = "Tabs may contain only Tab children"; return false; }
				++pageCount;
			}
			if (!pageCount) { error = "Tabs must contain at least one Tab child"; return false; }
			m_nodes[runtimeIndex].selectedTab = std::min(m_nodes[runtimeIndex].selectedTab, pageCount - 1);
			LayoutStyle tabsStyle = m_layout.style(m_nodes[runtimeIndex].layout);
			tabsStyle.padding.t += 34.f;
			m_layout.setStyle(m_nodes[runtimeIndex].layout, tabsStyle);
		}
		if (node.split) {
			Node& splitNode = m_nodes[runtimeIndex];
			NodeId child = m_layout.firstChild(splitNode.layout);
			u32 ordinal = 0;
			while (child.valid()) {
				LayoutStyle childStyle = m_layout.style(child);
				childStyle.splitWeight = ordinal == 0 ? splitNode.splitRatio : (ordinal == 1 ? 1.f - splitNode.splitRatio : 1.f);
				m_layout.setStyle(child, childStyle);
				child = m_layout.nextSibling(child);
				++ordinal;
			}
		}
		return true;
	}

	void UiRuntime::layout(f32 width, f32 height) {
		if (width != m_lastLayoutWidth || height != m_lastLayoutHeight) {
			addDamage({ 0, 0, width, height });
			m_lastLayoutWidth = width; m_lastLayoutHeight = height;
		}
		if (m_root.valid()) m_layout.layout(m_root, width, height);
		updateDockLayouts();
	}

	void UiRuntime::updateDockLayouts() {
		for (DockHostState& host : m_dockHosts) updateDockHostLayout(host);
	}

	void UiRuntime::updateDockHostLayout(DockHostState& host, bool force) {
		if (host.node >= m_nodes.size()) return;
		Node& hostNode = m_nodes[host.node];
		const Rect hostRect = m_layout.absRect(hostNode.layout);
		const LayoutStyle& hostStyle = m_layout.style(hostNode.layout);
		const f32 borderLeft = hostStyle.border.l, borderRight = hostStyle.border.r;
		const f32 borderTop = hostStyle.border.t, borderBottom = hostStyle.border.b;
		const Rect nextArea{ 0.f, 0.f,
			std::max(0.f, hostRect.w - borderLeft - borderRight - hostStyle.padding.l - hostStyle.padding.r),
			std::max(0.f, hostRect.h - borderTop - borderBottom - hostStyle.padding.t - hostStyle.padding.b) };
		const bool areaChanged = nextArea.w != host.contentArea.w || nextArea.h != host.contentArea.h;
		host.contentArea = nextArea;
		for (std::size_t panelOrdinal = 0; panelOrdinal < host.panelNodes.size(); ++panelOrdinal) {
			const u32 panelIndex = host.panelNodes[panelOrdinal];
			if (panelIndex >= m_nodes.size() || !host.tree.contains(panelIndex)) continue;
			if (panelOrdinal >= host.authoredPanelStyles.size()) continue;
			const LayoutStyle& panelStyle = host.authoredPanelStyles[panelOrdinal];
			auto resolve = [](Length length, f32 base, f32 fallback) {
				if (length.k == LenKind::Px) return std::max(0.f, length.v);
				if (length.k == LenKind::Percent) return std::max(0.f, length.v * base * 0.01f);
				return fallback;
			};
			const Vec2 minimum{
				std::max(panelStyle.minW, resolve(panelStyle.minWidth, nextArea.w, 0.f)),
				std::max(panelStyle.minH, resolve(panelStyle.minHeight, nextArea.h, 0.f)) };
			const Vec2 maximum{
				std::min(panelStyle.maxW, resolve(panelStyle.maxWidth, nextArea.w, kInf)),
				std::min(panelStyle.maxH, resolve(panelStyle.maxHeight, nextArea.h, kInf)) };
			Vec2 preferred = m_nodes[panelIndex].dockPreferredSize;
			if (panelStyle.width.k != LenKind::Auto) preferred.x = resolve(panelStyle.width, nextArea.w, preferred.x);
			if (panelStyle.height.k != LenKind::Auto) preferred.y = resolve(panelStyle.height, nextArea.h, preferred.y);
			host.tree.setPanelConstraints(panelIndex, minimum, preferred, maximum);
		}
		host.tree.layout(host.contentArea, host.gutter);
		bool changed = force || areaChanged;
		const std::span<const DockPanelLayout> panels = host.tree.panels();
		for (u32 ordinal = 0; ordinal < host.panelNodes.size(); ++ordinal) {
			const u32 panelIndex = host.panelNodes[ordinal];
			if (panelIndex >= m_nodes.size()) continue;
			const bool exists = host.tree.contains(panelIndex);
			Rect bounds = ordinal < panels.size() && exists ? panels[ordinal].bounds : Rect{};
			if (ordinal >= host.appliedBounds.size()) continue;
			if (!force && host.appliedBounds[ordinal] == bounds) continue;
			host.appliedBounds[ordinal] = bounds;
			LayoutStyle style = m_layout.style(m_nodes[panelIndex].layout);
			style.position = Position::Absolute;
			style.left = Length::px(bounds.x);
			style.top = Length::px(bounds.y);
			style.right = Length::autoLen();
			style.bottom = Length::autoLen();
			style.width = Length::px(bounds.w);
			style.height = Length::px(bounds.h);
			m_layout.setStyle(m_nodes[panelIndex].layout, style);
			m_nodes[panelIndex].dockClosed = !exists;
			changed = true;
		}
		if (changed && hostRect.w > 0.f && hostRect.h > 0.f) m_layout.layout(hostNode.layout, hostRect.w, hostRect.h);
	}

	UiRuntime::DockHostState* UiRuntime::dockHostForPanel(u32 panelNode) {
		for (DockHostState& host : m_dockHosts)
			if (std::find(host.panelNodes.begin(), host.panelNodes.end(), panelNode) != host.panelNodes.end()) return &host;
		return nullptr;
	}

	const UiRuntime::DockHostState* UiRuntime::dockHostForPanel(u32 panelNode) const {
		for (const DockHostState& host : m_dockHosts)
			if (std::find(host.panelNodes.begin(), host.panelNodes.end(), panelNode) != host.panelNodes.end()) return &host;
		return nullptr;
	}

	UiRuntime::DockHostState* UiRuntime::dockHostAt(Vec2 point) {
		for (auto it = m_dockHosts.rbegin(); it != m_dockHosts.rend(); ++it)
			if (m_layout.absRect(m_nodes[it->node].layout).contains(point)) return &*it;
		return nullptr;
	}

	void UiRuntime::addDockDamage(const DockHostState& host) {
		addDamage(m_layout.absRect(m_nodes[host.node].layout));
	}

	void UiRuntime::fireDockHandler(u32 panelNode, const std::string& handler) {
		if (m_eventFn && panelNode < m_nodes.size() && !handler.empty()) m_eventFn(m_eventContext, handler);
	}

	u32 UiRuntime::dockTabAt(const DockHostState& host, u32 groupPanel, Vec2 point) const {
		const Rect group = host.tree.groupBounds(groupPanel);
		if (!group.contains(point) || point.y >= group.y + m_nodes[host.node].dockHeaderHeight) return 0xFFFFFFFFu;
		const u32 count = host.tree.panelCount(groupPanel);
		if (!count) return 0xFFFFFFFFu;
		const f32 width = group.w / f32(count);
		for (u32 panelNode : host.panelNodes) {
			if (host.tree.groupOf(panelNode) != host.tree.groupOf(groupPanel)) continue;
			const u32 order = host.tree.panelOrder(panelNode);
			if (point.x >= group.x + width * f32(order) && point.x < group.x + width * f32(order + 1)) return panelNode;
		}
		return 0xFFFFFFFFu;
	}

	bool UiRuntime::dockPointerDown(Vec2 point) {
		for (std::size_t hostIndex = m_dockHosts.size(); hostIndex > 0; --hostIndex) {
			DockHostState& host = m_dockHosts[hostIndex - 1];
			if (!isNodeVisible(host.node)) continue;
			Node& hostNode = m_nodes[host.node];
			if (hostNode.disabled) continue;
			const Rect rect = m_layout.absRect(hostNode.layout);
			const LayoutStyle& style = m_layout.style(hostNode.layout);
			const Vec2 origin{ rect.x + style.border.l + style.padding.l, rect.y + style.border.t + style.padding.t };
			const Vec2 local{ point.x - origin.x, point.y - origin.y };
			const Rect area{ 0, 0, host.contentArea.w, host.contentArea.h };
			if (area.contains(local)) {
				bool horizontal = true;
				const u32 split = host.tree.splitAt(local, &horizontal);
				if (split != 0xFFFFFFFFu) {
					const Rect bounds = host.tree.splitBounds(split);
					m_dockDragHost = static_cast<u32>(hostIndex - 1);
					m_dockDragSplit = split;
					m_dockSplitDragging = true;
					m_dockSplitHorizontal = horizontal;
					m_dockSplitStartRatio = host.tree.splitRatio(split);
					m_dockSplitStartPointer = horizontal ? local.x : local.y;
					m_dockSplitExtent = std::max(1.f, horizontal ? bounds.w : bounds.h);
					addDockDamage(host);
					return true;
				}
			}
			u32 panel = 0xFFFFFFFFu;
			u32 active = host.tree.panelAt(local);
			if (active != 0xFFFFFFFFu) panel = dockTabAt(host, active, local);
			if (panel == 0xFFFFFFFFu) panel = active;
			if (panel >= m_nodes.size() || !m_nodes[panel].dockPanel || m_nodes[panel].disabled || m_nodes[panel].dockClosed) continue;
			const Rect panelBounds = host.tree.panelBounds(panel);
			const f32 headerHeight = std::min(panelBounds.h, hostNode.dockHeaderHeight);
			const Rect header{ panelBounds.x, panelBounds.y, panelBounds.w, headerHeight };
			if (!header.contains(local)) continue;
			const u32 count = host.tree.panelCount(panel);
			const f32 tabWidth = count ? host.tree.groupBounds(panel).w / f32(count) : panelBounds.w;
			const f32 tabX = count > 1 ? host.tree.groupBounds(panel).x + tabWidth * f32(host.tree.panelOrder(panel)) : panelBounds.x;
			const Rect closeRect{ tabX + std::max(0.f, tabWidth - headerHeight), panelBounds.y, std::min(headerHeight, tabWidth), headerHeight };
			if (m_nodes[panel].dockClosable && closeRect.contains(local)) {
				const u32 previousActive = host.tree.activePanel(panel);
				if (host.tree.close(panel)) {
					m_nodes[panel].dockClosed = true;
					updateDockHostLayout(host, true);
					addDockDamage(host);
					fireDockHandler(panel, m_nodes[panel].closeHandler);
					const u32 activeAfterClose = host.tree.activePanel(panel);
					if (previousActive != activeAfterClose && activeAfterClose < m_nodes.size())
						fireDockHandler(activeAfterClose, m_nodes[activeAfterClose].tabChangeHandler);
					return true;
				}
			}
			m_dockCandidate = panel;
			m_dockDragPanel = panel;
			m_dockDragHost = static_cast<u32>(hostIndex - 1);
			m_dockDragStart = local;
			m_dockFloatStart = panelBounds;
			m_dockTabDragX = tabX;
			m_dockPreview = {};
			m_dockDragging = false;
			return true;
		}
		return false;
	}

	bool UiRuntime::dockPointerMove(Vec2 point) {
		if (m_dockSplitDragging && m_dockDragHost < m_dockHosts.size()) {
			DockHostState& host = m_dockHosts[m_dockDragHost];
			const Rect hostRect = m_layout.absRect(m_nodes[host.node].layout);
			const LayoutStyle& style = m_layout.style(m_nodes[host.node].layout);
			const f32 origin = m_dockSplitHorizontal ? hostRect.x + style.border.l + style.padding.l : hostRect.y + style.border.t + style.padding.t;
			const f32 pointer = (m_dockSplitHorizontal ? point.x : point.y) - origin;
			const f32 nextRatio = m_dockSplitStartRatio + (pointer - m_dockSplitStartPointer) / m_dockSplitExtent;
			const bool moved = host.tree.resizeSplit(m_dockDragSplit, nextRatio);
			if (moved) { updateDockHostLayout(host); addDockDamage(host); }
			return moved;
		}
		if (m_dockCandidate == 0xFFFFFFFFu || m_dockDragHost >= m_dockHosts.size()) return false;
		DockHostState& host = m_dockHosts[m_dockDragHost];
		const Rect hostRect = m_layout.absRect(m_nodes[host.node].layout);
		const LayoutStyle& style = m_layout.style(m_nodes[host.node].layout);
		const Vec2 origin{ hostRect.x + style.border.l + style.padding.l, hostRect.y + style.border.t + style.padding.t };
		const Vec2 local{ point.x - origin.x, point.y - origin.y };
		const f32 distance = std::fabs(local.x - m_dockDragStart.x) + std::fabs(local.y - m_dockDragStart.y);
		if (!m_dockDragging && distance < 5.f) return false;
		m_dockDragging = true;
		m_dockPreview = {};
		const Rect area{ 0, 0, host.contentArea.w, host.contentArea.h };
		if (area.contains(local)) {
			const u32 target = host.tree.panelAt(local);
			if (target != 0xFFFFFFFFu && target != m_dockDragPanel) {
				const Rect group = host.tree.groupBounds(target);
				DockZone zone = DockZone::Center;
				const f32 edgeX = std::min(group.w * 0.25f, 72.f);
				const f32 edgeY = std::min(group.h * 0.25f, 72.f);
				if (local.x < group.x + edgeX) zone = DockZone::Left;
				else if (local.x >= group.right() - edgeX) zone = DockZone::Right;
				else if (local.y < group.y + edgeY) zone = DockZone::Top;
				else if (local.y >= group.bottom() - edgeY) zone = DockZone::Bottom;
				m_dockPreview = host.tree.preview(target, zone);
			}
		}
		addDockDamage(host);
		return true;
	}

	bool UiRuntime::dockPointerUp(Vec2 point) {
		if (m_dockSplitDragging && m_dockDragHost < m_dockHosts.size()) {
			DockHostState& host = m_dockHosts[m_dockDragHost];
			m_dockSplitDragging = false;
			for (u32 panel : host.panelNodes) fireDockHandler(panel, m_nodes[panel].resizeHandler);
			addDockDamage(host);
			m_dockDragHost = m_dockDragSplit = 0xFFFFFFFFu;
			return true;
		}
		if (m_dockCandidate == 0xFFFFFFFFu || m_dockDragHost >= m_dockHosts.size()) return false;
		DockHostState& host = m_dockHosts[m_dockDragHost];
		const u32 panel = m_dockDragPanel;
		bool changed = false;
		if (m_dockDragging && panel < m_nodes.size()) {
			const Rect hostRect = m_layout.absRect(m_nodes[host.node].layout);
			const LayoutStyle& style = m_layout.style(m_nodes[host.node].layout);
			const Vec2 origin{ hostRect.x + style.border.l + style.padding.l, hostRect.y + style.border.t + style.padding.t };
			const Vec2 local{ point.x - origin.x, point.y - origin.y };
			const DockZone oldZone = host.tree.zone(panel);
			const bool wasFloating = host.tree.floating(panel);
			const bool releasedInsideHost = Rect{ 0, 0, host.contentArea.w, host.contentArea.h }.contains(local);
			// The release position is authoritative. A pointer-up can arrive outside
			// without an intervening move event (for example, a fast native drag), so
			// never apply a stale in-host preview to an outside release.
			if (releasedInsideHost && m_dockPreview.valid)
				changed = host.tree.dock(panel, m_dockPreview.targetPanel, m_dockPreview.zone);
			else if (!releasedInsideHost) {
				Rect floating = m_dockFloatStart;
				floating.x = local.x - (m_dockDragStart.x - m_dockFloatStart.x);
				floating.y = local.y - (m_dockDragStart.y - m_dockFloatStart.y);
				changed = wasFloating ? host.tree.moveFloating(panel, floating) : host.tree.undock(panel, floating);
			}
			if (changed) {
				updateDockHostLayout(host, true);
				addDockDamage(host);
				if (wasFloating && !host.tree.floating(panel)) fireDockHandler(panel, m_nodes[panel].dockHandler);
				else if (!wasFloating && host.tree.floating(panel)) fireDockHandler(panel, m_nodes[panel].undockHandler);
				else if (!wasFloating && !host.tree.floating(panel) && oldZone != host.tree.zone(panel)) fireDockHandler(panel, m_nodes[panel].dockHandler);
			}
		}
		else if (panel < m_nodes.size()) {
			const u32 previous = host.tree.activePanel(panel);
			changed = host.tree.activate(panel);
			if (changed) {
				updateDockHostLayout(host);
				addDockDamage(host);
				fireDockHandler(panel, m_nodes[panel].tabChangeHandler);
				if (previous < m_nodes.size()) fireDockHandler(previous, m_nodes[previous].tabChangeHandler);
			}
		}
		m_dockCandidate = m_dockDragPanel = m_dockDragHost = 0xFFFFFFFFu;
		m_dockDragging = false;
		m_dockPreview = {};
		return true;
	}

	Rect UiRuntime::scrollBarRect(u32 index, bool horizontal, Rect* trackOut) const {
		if (index >= m_nodes.size() || !m_nodes[index].scrollView || m_nodes[index].scrollBarVisibility == 2) return {};
		const Node& node = m_nodes[index];
		const Rect view = m_layout.absRect(node.layout);
		const Vec2 content = m_layout.contentSize(node.layout);
		const Vec2 offset = m_layout.scrollOffset(node.layout);
		const LayoutStyle& style = m_layout.style(node.layout);
		const f32 thickness = node.scrollBarSize;
		Rect track, thumb;
		if (horizontal) {
			if (!style.scrollX || content.x <= view.w + 0.5f || view.w <= 0.f) return {};
			track = { view.x, view.bottom() - thickness, view.w - (style.scrollY && content.y > view.h ? thickness : 0.f), thickness };
			const f32 length = std::max(thickness * 1.5f, track.w * view.w / std::max(view.w, content.x));
			const f32 travel = std::max(0.f, track.w - length);
			thumb = { track.x + travel * offset.x / std::max(1.f, content.x - view.w), track.y, length, track.h };
		}
		else {
			if (!style.scrollY || content.y <= view.h + 0.5f || view.h <= 0.f) return {};
			track = { view.right() - thickness, view.y, thickness, view.h - (style.scrollX && content.x > view.w ? thickness : 0.f) };
			const f32 length = std::max(thickness * 1.5f, track.h * view.h / std::max(view.h, content.y));
			const f32 travel = std::max(0.f, track.h - length);
			thumb = { track.x, track.y + travel * offset.y / std::max(1.f, content.y - view.h), track.w, length };
		}
		if (trackOut) *trackOut = track;
		return thumb;
	}

	bool UiRuntime::pointInsideScrollAncestors(u32 index, Vec2 point) const {
		if (index >= m_nodes.size()) return false;
		for (u32 parent = m_nodes[index].parentIndex; parent != 0xFFFFFFFFu && parent < m_nodes.size(); parent = m_nodes[parent].parentIndex)
			if (m_nodes[parent].scrollView && !m_layout.absRect(m_nodes[parent].layout).contains(point)) return false;
		return true;
	}

	bool UiRuntime::setScrollOffset(u32 index, Vec2 offset, bool smooth) {
		if (index >= m_nodes.size() || !m_nodes[index].scrollView) return false;
		Node& node = m_nodes[index];
		const Vec2 before = m_layout.scrollOffset(node.layout);
		if (smooth) {
			const Vec2 content = m_layout.contentSize(node.layout);
			const Rect rect = m_layout.rect(node.layout);
			node.scrollTarget = { std::clamp(offset.x, 0.f, std::max(0.f, content.x - rect.w)), std::clamp(offset.y, 0.f, std::max(0.f, content.y - rect.h)) };
			node.scrollAnimating = node.scrollTarget != before;
			if (node.scrollAnimating) addDamage(m_layout.absRect(node.layout));
			return node.scrollAnimating;
		}
		node.scrollAnimating = false;
		m_layout.setScrollOffset(node.layout, offset);
		const Vec2 after = m_layout.scrollOffset(node.layout);
		if (after == before) return false;
		addDamage(m_layout.absRect(node.layout));
		return true;
	}

	bool UiRuntime::ensureVisible(u32 index, bool smooth) {
		if (index >= m_nodes.size() || !isNodeVisible(index)) return false;
		bool changed = false;
		for (u32 parent = m_nodes[index].parentIndex; parent != 0xFFFFFFFFu && parent < m_nodes.size(); parent = m_nodes[parent].parentIndex) {
			if (!m_nodes[parent].scrollView) continue;
			const Rect viewport = m_layout.absRect(m_nodes[parent].layout);
			const Rect child = m_layout.absRect(m_nodes[index].layout);
			Vec2 offset = m_layout.scrollOffset(m_nodes[parent].layout);
			if (child.x < viewport.x) offset.x -= viewport.x - child.x;
			else if (child.right() > viewport.right()) offset.x += child.right() - viewport.right();
			if (child.y < viewport.y) offset.y -= viewport.y - child.y;
			else if (child.bottom() > viewport.bottom()) offset.y += child.bottom() - viewport.bottom();
			const bool moved = setScrollOffset(parent, offset, smooth);
			changed |= moved;
			if (moved && !smooth && m_nodes[parent].pressHandler.v && m_eventFn)
				m_eventFn(m_eventContext, m_strings->view(m_nodes[parent].pressHandler));
		}
		return changed;
	}

	Vec2 UiRuntime::scrollOffset(std::string_view id) const {
		for (const Node& node : m_nodes) if (node.scrollView && m_strings->view(node.id) == id) return m_layout.scrollOffset(node.layout);
		return {};
	}

	bool UiRuntime::scrollTo(std::string_view id, Vec2 offset, bool smooth) {
		for (u32 i = 0; i < m_nodes.size(); ++i) if (m_nodes[i].scrollView && m_strings->view(m_nodes[i].id) == id) {
			const bool changed = setScrollOffset(i, offset, smooth);
			if (changed && !smooth && m_nodes[i].pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(m_nodes[i].pressHandler));
			return changed;
		}
		return false;
	}

	bool UiRuntime::scrollIntoView(std::string_view id, bool smooth) {
		for (u32 i = 0; i < m_nodes.size(); ++i) if (m_strings->view(m_nodes[i].id) == id) return ensureVisible(i, smooth);
		return false;
	}

	void UiRuntime::addDamage(Rect rect) {
		if (rect.empty()) return;
		if (!m_hasDamage) { m_damage = rect; m_hasDamage = true; return; }
		const f32 left = std::min(m_damage.x, rect.x), top = std::min(m_damage.y, rect.y);
		const f32 right = std::max(m_damage.right(), rect.right()), bottom = std::max(m_damage.bottom(), rect.bottom());
		m_damage = { left, top, right - left, bottom - top };
	}

	void UiRuntime::applyLayoutStyle(Node& node) {
		LayoutStyle layoutStyle = m_layout.style(node.layout);
		auto setSize = [&](PropId property, Length& width, Length& height) {
			if (!node.style.has(property)) return;
			const StyleValue& value = node.style.get(property);
			width = length(value, 0);
			height = value.count > 1 ? length(value, 1) : width;
			};
		setSize(PropId::Size, layoutStyle.width, layoutStyle.height);
		if (node.style.has(PropId::MinSize)) {
			const StyleValue& v = node.style.get(PropId::MinSize);
			layoutStyle.minWidth = length(v, 0); layoutStyle.minHeight = length(v, 1);
		}
		if (node.style.has(PropId::MaxSize)) {
			const StyleValue& v = node.style.get(PropId::MaxSize);
			layoutStyle.maxWidth = length(v, 0); layoutStyle.maxHeight = length(v, 1);
		}
		if (node.style.has(PropId::MinWidth)) layoutStyle.minWidth = length(node.style.get(PropId::MinWidth), 0);
		if (node.style.has(PropId::MaxWidth)) layoutStyle.maxWidth = length(node.style.get(PropId::MaxWidth), 0);
		if (node.style.has(PropId::MinHeight)) layoutStyle.minHeight = length(node.style.get(PropId::MinHeight), 0);
		if (node.style.has(PropId::MaxHeight)) layoutStyle.maxHeight = length(node.style.get(PropId::MaxHeight), 0);
		if (node.style.has(PropId::Pad)) {
			const StyleValue& v = node.style.get(PropId::Pad);
			layoutStyle.padding = { number(v, 3), number(v, 0), number(v, 1), number(v, 2) };
		}
		if (node.style.has(PropId::Border)) {
			const StyleValue& v = node.style.get(PropId::Border);
			const f32 border = v.count && v.atoms[0].kind == AtomKind::Number ? std::max(0.f, v.atoms[0].num) : 0.f;
			layoutStyle.border = Edges::all(border);
		}
		if (node.style.has(PropId::Margin)) {
			const StyleValue& v = node.style.get(PropId::Margin);
			layoutStyle.margin = { number(v, 3), number(v, 0), number(v, 1), number(v, 2) };
		}
		if (node.style.has(PropId::Gap)) {
			const StyleValue& v = node.style.get(PropId::Gap);
			layoutStyle.gap = { number(v, 0), number(v, 1) };
		}
		if (node.style.has(PropId::Grow)) layoutStyle.grow = number(node.style.get(PropId::Grow), 0);
		if (node.style.has(PropId::Shrink)) layoutStyle.shrink = number(node.style.get(PropId::Shrink), 0, 1);
		if (node.style.has(PropId::Align)) layoutStyle.alignItems = align(node.style.get(PropId::Align).atoms[0]);
		if (node.style.has(PropId::Justify)) layoutStyle.justify = justify(node.style.get(PropId::Justify).atoms[0]);
		m_layout.setStyle(node.layout, layoutStyle);
	}

	void UiRuntime::paint(Painter& painter, f32 dpiScale) {
		if (!m_root.valid()) return;
		const auto now = std::chrono::steady_clock::now();
		if (m_fpsLastFrame.time_since_epoch().count() != 0) {
			m_fpsElapsed += std::chrono::duration<f64>(now - m_fpsLastFrame).count();
			++m_fpsFrames;
			if (m_fpsElapsed >= std::chrono::duration<f64>(kFpsRefreshInterval).count()) {
				m_fpsValue = static_cast<f32>(m_fpsFrames / m_fpsElapsed);
				m_frameTimeMs = m_fpsFrames ? static_cast<f32>(m_fpsElapsed * 1000.0 / m_fpsFrames) : 0.f;
				for (Node& fps : m_nodes) if (fps.fpsCounter) {
					char value[48];
					if (fps.showFrameTime) std::snprintf(value, sizeof(value), "FPS: %.0f | %.1f ms", double(m_fpsValue), double(m_frameTimeMs));
					else std::snprintf(value, sizeof(value), "FPS: %.0f", double(m_fpsValue));
					if (fps.text == value) continue;
					fps.text = value;
					m_layout.markContentChanged(fps.layout);
				}
				m_fpsElapsed = 0;
				m_fpsFrames = 0;
			}
		}
		m_fpsLastFrame = now;
		auto drawLabel = [&](std::string_view text, Rect rect, FontHandle fontHandle, const PropTable& style,
			Color fallback, f32 inset, bool wrap) {
				if (!m_text || !m_text->atlas()->ready() || !fontHandle.valid() || text.empty()) return;
				const f32 size = (style.has(PropId::FontSize) ? number(style.get(PropId::FontSize), 0, 14.f) : 14.f) * m_dpiScale;
				const TextLayout& run = m_text->layout(fontHandle, text, size, wrap ? std::max(0.f, rect.w - inset * 2.f) * m_dpiScale : 0.f);
				const Font* font = m_text->font(fontHandle); if (!font || run.glyphs.empty()) return;
				m_glyphScratch.clear();
				for (const ShapedGlyph& shaped : run.glyphs) {
					const AtlasGlyph glyph = m_text->atlas()->glyph(*font, shaped.glyph, static_cast<u16>(std::clamp(std::lround(size), 1l, 65535l)));
					const f32 scale = 1.f / m_dpiScale;
					m_glyphScratch.push_back({ {shaped.x * scale + glyph.offset.x * scale, shaped.y * scale + glyph.offset.y * scale, glyph.width * scale, glyph.height * scale}, glyph.uv });
				}
				const f32 width = run.width / m_dpiScale;
				const Align alignment = style.has(PropId::Align) ? align(style.get(PropId::Align).atoms[0]) : Align::Start;
				const f32 x = alignment == Align::Center ? rect.x + (rect.w - width) * 0.5f :
					alignment == Align::End ? rect.right() - inset - width : rect.x + inset;
				const f32 y = rect.y + std::max(0.f, (rect.h - run.height / m_dpiScale) * 0.5f);
				painter.pushClip(rect);
				painter.glyphRun(m_text->atlas()->texture(), m_glyphScratch.data(), static_cast<u32>(m_glyphScratch.size()),
					{ x, y + font->baselineAt(size) / m_dpiScale }, color(style, PropId::Fg, fallback));
				painter.popClip();
			};
		bool spinnerActive = false;
		for (u32 i = 0; i < m_nodes.size(); ++i)
			if ((m_nodes[i].spinner || (m_nodes[i].progressBar && m_nodes[i].indeterminate)) && !m_nodes[i].disabled && isNodeVisible(i)) { spinnerActive = true; break; }
		if (spinnerActive && now >= m_spinnerDeadline) {
			m_spinnerDeadline = now + std::chrono::milliseconds(80);
			for (u32 i = 0; i < m_nodes.size(); ++i)
				if ((m_nodes[i].spinner || (m_nodes[i].progressBar && m_nodes[i].indeterminate)) && !m_nodes[i].disabled && isNodeVisible(i)) {
					m_nodes[i].spinnerPhase = std::fmod(m_nodes[i].spinnerPhase + 0.6283185f, 6.2831853f);
					addDamage(m_layout.absRect(m_nodes[i].layout));
				}
		}
		else if (!spinnerActive) m_spinnerDeadline = {};
		// Caret blink: only tick while an editable field holds focus; other frames are untouched.
		const bool editableFocused = caretEditableFocused();
		if (editableFocused) {
			if (m_caretBlinkDeadline.time_since_epoch().count() == 0) {
				m_caretBlinkDeadline = now + kCaretBlinkInterval;
				m_caretVisible = true;
			}
			else if (now >= m_caretBlinkDeadline) {
				m_caretVisible = !m_caretVisible;
				m_caretBlinkDeadline = now + kCaretBlinkInterval;
				for (u32 i = 0; i < m_nodes.size(); ++i) {
					const Node& node = m_nodes[i];
					if ((node.textBox || node.textArea) && node.focused && !node.disabled && !node.readOnly && isNodeVisible(i))
						addDamage(node.pickerEditor ? pickerEditorRect(node) : m_layout.absRect(node.layout));
				}
			}
		}
		else if (m_caretBlinkDeadline.time_since_epoch().count() != 0) {
			m_caretBlinkDeadline = {};
			m_caretVisible = true;
		}
		m_dpiScale = std::max(0.25f, dpiScale);
		for (u32 nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex) {
			Node& node = m_nodes[nodeIndex];
			if ((node.fpsCounter && !m_fpsEnabled) || !isNodeVisible(nodeIndex)) continue;
			struct ScrollClipScope { Painter& painter; u32 count = 0; ~ScrollClipScope() { while (count--) painter.popClip(); } } scrollClips{ painter };
			for (u32 parent = node.parentIndex; parent != 0xFFFFFFFFu && parent < m_nodes.size(); parent = m_nodes[parent].parentIndex)
				if (m_nodes[parent].scrollView) { painter.pushClip(m_layout.absRect(m_nodes[parent].layout)); ++scrollClips.count; }
			const StateMask states = activeStates(node);
			if (!node.styleResolved || node.resolvedStates != states) {
				m_styles->resolve(node.styleSet, states, m_theme, node.style);
				node.inheritedTextMask = 0;
				node.resolvedStates = states;
				node.styleResolved = true;
			}
			if (node.parentIndex != 0xFFFFFFFFu && node.parentIndex < nodeIndex) {
				const PropId inherited[] = { PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing };
				const PropTable& parentStyle = m_nodes[node.parentIndex].style;
				for (u8 i = 0; i < std::size(inherited); ++i) {
					const PropId property = inherited[i];
					const u64 bit = u64{ 1 } << static_cast<int>(property);
					const bool wasInherited = (node.inheritedTextMask & u8(1u << i)) != 0;
					if (wasInherited) {
						if (parentStyle.has(property)) node.style.v[static_cast<int>(property)] = parentStyle.get(property);
						else { node.style.mask &= ~bit; node.inheritedTextMask &= u8(~(1u << i)); }
					}
					else if (!node.style.has(property) && parentStyle.has(property)) {
						node.style.v[static_cast<int>(property)] = parentStyle.get(property);
						node.style.mask |= bit;
						node.inheritedTextMask |= u8(1u << i);
					}
				}
			}
			node.font = selectFont(node.style);
			if (node.pickerEditor) continue; // picker editors paint in the popup overlay pass below
			const Rect rect = m_layout.absRect(node.layout);
			if (rect.empty()) continue;

			Color bg = color(node.style, PropId::Bg, Color{ 0, 0, 0, 0 });
			const f32 opacity = node.style.has(PropId::Opacity) ? std::clamp(number(node.style.get(PropId::Opacity), 0, 1), 0.f, 1.f) : 1.f;
			painter.pushOpacity(opacity);
			if (node.split) {
				const u32 count = m_layout.gutterCount(node.layout);
				const Color gutterColor = color(node.style, PropId::Fg, Color::fromRGBA8(0x475569FFu));
				for (u32 i = 0; i < count; ++i) {
					Rect gutter = m_layout.gutterRect(node.layout, i);
					gutter.x += rect.x; gutter.y += rect.y;
					painter.fillRect(gutter, gutterColor);
				}
			}
			if (bg.a > 0) {
				RectStyle shape;
				shape.fill = bg;
				shape.borderWidth = node.style.has(PropId::Border) ? number(node.style.get(PropId::Border), 0) : 0.f;
				shape.border = Color{ 0, 0, 0, 0 };
				if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
					const Atom& borderColor = node.style.get(PropId::Border).atoms[2];
					if (borderColor.kind == AtomKind::Color) shape.border = Color::fromRGBA8(borderColor.bits);
					const Atom& borderStyle = node.style.get(PropId::Border).atoms[1];
					shape.dashed = borderStyle.kind == AtomKind::Ident && m_strings->view(borderStyle.str()) == "dashed";
				}
				if (node.style.has(PropId::Shadow)) {
					const StyleValue& shadow = node.style.get(PropId::Shadow);
					ShadowStyle effect;
					effect.offset = { number(shadow, 0), number(shadow, 1) };
					effect.blur = number(shadow, 2, 8.f);
					effect.spread = number(shadow, 3);
					effect.color = shadow.count > 4 && shadow.atoms[4].kind == AtomKind::Color
						? Color::fromRGBA8(shadow.atoms[4].bits) : Color::fromRGBA8(0x00000055u);
					if (node.style.has(PropId::Radius)) effect.radius = { number(node.style.get(PropId::Radius), 0), number(node.style.get(PropId::Radius), 1), number(node.style.get(PropId::Radius), 2), number(node.style.get(PropId::Radius), 3) };
					painter.shadow(rect, effect);
				}
				if (node.style.has(PropId::Radius)) {
					const StyleValue& radius = node.style.get(PropId::Radius);
					shape.radius = { number(radius, 0), number(radius, 1), number(radius, 2), number(radius, 3) };
				}
				if (shape.fill.a > 0 || (shape.borderWidth > 0 && shape.border.a > 0)) painter.roundRect(rect, shape);
			}
			if (node.checkItem || node.checkBox) {
				const f32 boxSize = std::min(18.f, std::max(12.f, rect.h - 8.f));
				const Rect box{ rect.x + 2.f, rect.y + (rect.h - boxSize) * 0.5f, boxSize, boxSize };
				RectStyle check;
				check.fill = node.checked ? color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu)) : Color{ 0, 0, 0, 0 };
				const Color checkFg = color(node.style, PropId::Fg, Color::fromRGBA8(0x94A3B8FFu));
				check.border = checkFg.withAlpha(0.65f); check.borderWidth = 1.f;
				if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
					const Atom& borderColor = node.style.get(PropId::Border).atoms[2];
					if (borderColor.kind == AtomKind::Color) check.border = Color::fromRGBA8(borderColor.bits);
				}
				const f32 checkRadius = node.style.has(PropId::Radius) ? number(node.style.get(PropId::Radius), 0, 3.f) : 3.f;
				check.radius = CornerRadii::all(checkRadius);
				painter.roundRect(box, check);
				if (node.checked) {
					const Color mark = Color::fromRGBA8(0x0F172AFFu);
					painter.line({ box.x + 4.f, box.y + boxSize * 0.52f }, { box.x + boxSize * 0.43f, box.y + boxSize - 4.f }, 2.f, mark);
					painter.line({ box.x + boxSize * 0.43f, box.y + boxSize - 4.f }, { box.x + boxSize - 3.f, box.y + 4.f }, 2.f, mark);
				}
			}
			if (node.dropdown || node.datePicker) {
				const f32 cx = rect.right() - 15.f, cy = rect.y + rect.h * 0.5f;
				const Color mark = color(node.style, PropId::Fg, Color::fromRGBA8(0xF2F6FFFFu));
				painter.line({ cx - 4.f, cy - 2.f }, { cx, cy + 2.f }, 1.5f, mark);
				painter.line({ cx, cy + 2.f }, { cx + 4.f, cy - 2.f }, 1.5f, mark);
			}
			if (node.colorPicker) {
				const f32 side = std::min(22.f, std::max(12.f, rect.h - 8.f));
				const Rect swatch{ rect.right() - side - 5.f, rect.y + (rect.h - side) * 0.5f, side, side };
				painter.fillRect(swatch, node.pickerColor, CornerRadii::all(3.f));
			}
			if (node.progressBar || node.slider) {
				const Color track = bg.a > 0 ? bg : Color::fromRGBA8(0x334155FFu);
				const Color fill = color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu));
				const f32 fraction = node.maxValue > node.minValue
					? std::clamp((node.value - node.minValue) / (node.maxValue - node.minValue), 0.f, 1.f) : 0.f;
				if (node.progressBar) {
					const f32 thickness = std::max(4.f, rect.h * 0.35f);
					const Rect trackRect{ rect.x, rect.y + (rect.h - thickness) * 0.5f, rect.w, thickness };
					painter.fillRect(trackRect, track, CornerRadii::all(thickness * 0.5f));
					if (node.indeterminate) {
						// Sweep a short segment across the track; spinnerPhase is advanced by the idle timer.
						const f32 span = trackRect.w * 0.3f;
						const f32 head = std::fmod(node.spinnerPhase / 6.2831853f, 1.f) * (trackRect.w + span);
						const f32 left = std::clamp(head - span, 0.f, trackRect.w);
						const f32 right = std::clamp(head, 0.f, trackRect.w);
						if (right > left) painter.fillRect({ trackRect.x + left, trackRect.y, right - left, trackRect.h }, fill, CornerRadii::all(thickness * 0.5f));
					}
					else {
						painter.fillRect({ trackRect.x, trackRect.y, trackRect.w * fraction, trackRect.h }, fill, CornerRadii::all(thickness * 0.5f));
					}
				}
				else {
					const f32 thickness = std::max(3.f, rect.h * 0.16f);
					const f32 centerY = rect.y + rect.h * 0.5f;
					painter.fillRect({ rect.x, centerY - thickness * 0.5f, rect.w, thickness }, track, CornerRadii::all(thickness * 0.5f));
					painter.fillRect({ rect.x, centerY - thickness * 0.5f, rect.w * fraction, thickness }, fill, CornerRadii::all(thickness * 0.5f));
					const f32 diameter = std::min(rect.h, 22.f);
					painter.fillRect({ rect.x + (rect.w - diameter) * fraction, centerY - diameter * 0.5f, diameter, diameter }, fill, CornerRadii::all(diameter * 0.5f));
				}
			}
			if (node.progressBar || node.slider) { painter.popOpacity(); continue; }
			if (node.switchToggle) {
				const f32 trackHeight = std::clamp(rect.h - 8.f, 18.f, 26.f);
				const f32 trackWidth = std::clamp(trackHeight * 1.8f, 36.f, 58.f);
				const Rect track{ rect.x, rect.y + (rect.h - trackHeight) * 0.5f, trackWidth, trackHeight };
				RectStyle trackStyle;
				const Color switchFg = color(node.style, PropId::Fg, Color::fromRGBA8(0x2563EBFFu));
				trackStyle.fill = node.checked ? switchFg : switchFg.withAlpha(0.28f);
				const f32 switchRadius = node.style.has(PropId::Radius) ? number(node.style.get(PropId::Radius), 0, trackHeight * 0.5f) : trackHeight * 0.5f;
				trackStyle.radius = CornerRadii::all(switchRadius);
				if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
					const Atom& borderColor = node.style.get(PropId::Border).atoms[2];
					if (borderColor.kind == AtomKind::Color) { trackStyle.border = Color::fromRGBA8(borderColor.bits); trackStyle.borderWidth = number(node.style.get(PropId::Border), 0); }
				}
				painter.roundRect(track, trackStyle);
				const f32 knob = trackHeight - 4.f;
				const f32 knobX = track.x + 2.f + (node.checked ? track.w - knob - 2.f : 0.f);
				painter.fillRect({ knobX, track.y + 2.f, knob, knob }, Color::fromRGBA8(0xF8FAFCFFu), CornerRadii::all(knob * 0.5f));
			}
			if (node.separator) {
				const Color lineColor = color(node.style, PropId::Fg, Color::fromRGBA8(0x475569FFu));
				if (rect.w >= rect.h) painter.line({ rect.x, rect.y + rect.h * 0.5f }, { rect.right(), rect.y + rect.h * 0.5f }, 1.f, lineColor);
				else painter.line({ rect.x + rect.w * 0.5f, rect.y }, { rect.x + rect.w * 0.5f, rect.bottom() }, 1.f, lineColor);
				painter.popOpacity(); continue;
			}
			if (node.spinner) {
				const Color tick = color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu));
				const f32 radiusOuter = std::max(2.f, std::min(rect.w, rect.h) * 0.5f - 2.f);
				const f32 cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;
				constexpr u32 ticks = 8;
				for (u32 i = 0; i < ticks; ++i) {
					const f32 angle = node.spinnerPhase + f32(i) * (6.2831853f / f32(ticks));
					const f32 inner = radiusOuter * 0.45f;
					painter.line({ cx + std::cos(angle) * inner, cy + std::sin(angle) * inner },
						{ cx + std::cos(angle) * radiusOuter, cy + std::sin(angle) * radiusOuter },
						std::max(1.5f, radiusOuter * 0.18f),
						i == 0 ? tick : tick.withAlpha(0.85f * (1.f - f32(i) / f32(ticks))));
				}
				painter.popOpacity(); continue;
			}
			if (node.image) {
				if (!node.imageLoaded && !node.imageSource.empty()) {
					node.imageLoaded = true;
					if (readPng(node.imageSource, node.imagePixels) && !node.imagePixels.rgba.empty()) {
						node.imageW = node.imagePixels.width; node.imageH = node.imagePixels.height;
						if (m_renderer) {
							TextureDesc desc;
							desc.width = node.imageW; desc.height = node.imageH;
							desc.format = TextureFormat::RGBA8;
							desc.pixels = node.imagePixels.rgba.data();
							node.imageTexture = m_renderer->createTexture(desc);
						}
						m_layout.markContentChanged(node.layout);
						addDamage(rect);
					}
					else node.imageFailed = true;
				}
				if (node.imageTexture.valid()) {
					Rect dst = rect;
					Rect uv{ 0.f, 0.f, 1.f, 1.f };
					if (node.imageFit != 0 && node.imageW > 0 && node.imageH > 0 && rect.w > 0.f && rect.h > 0.f) {
						const f32 scale = node.imageFit == 2
							? std::max(rect.w / f32(node.imageW), rect.h / f32(node.imageH))
							: std::min(rect.w / f32(node.imageW), rect.h / f32(node.imageH));
						if (node.imageFit == 2) {
							// Cover: scale up to fill and crop the source so the destination stays the widget bounds.
							const f32 drawW = f32(node.imageW) * scale, drawH = f32(node.imageH) * scale;
							const f32 cropX = std::clamp((drawW - rect.w) * 0.5f / drawW, 0.f, 0.5f);
							const f32 cropY = std::clamp((drawH - rect.h) * 0.5f / drawH, 0.f, 0.5f);
							uv = { cropX, cropY, 1.f - cropX, 1.f - cropY };
							dst = rect;
						}
						else {
							const f32 drawW = f32(node.imageW) * scale, drawH = f32(node.imageH) * scale;
							dst = { rect.x + (rect.w - drawW) * 0.5f, rect.y + (rect.h - drawH) * 0.5f, drawW, drawH };
						}
					}
					const f32 radius = node.style.has(PropId::Radius) ? number(node.style.get(PropId::Radius), 0) : 0.f;
					painter.pushClip(rect);
					painter.image(node.imageTexture, dst, uv, Color{ 1, 1, 1, 1 }, CornerRadii::all(radius));
					painter.popClip();
				}
				else {
					// Placeholder while the source is missing, unreadable, or there is no renderer for textures.
					RectStyle frame;
					frame.border = color(node.style, PropId::Fg, Color::fromRGBA8(0x475569FFu)).withAlpha(0.75f);
					frame.borderWidth = node.style.has(PropId::Border) ? number(node.style.get(PropId::Border), 0, 1.f) : 1.f;
					if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3 && node.style.get(PropId::Border).atoms[2].kind == AtomKind::Color)
						frame.border = Color::fromRGBA8(node.style.get(PropId::Border).atoms[2].bits);
					if (node.style.has(PropId::Radius)) frame.radius = CornerRadii::all(number(node.style.get(PropId::Radius), 0));
					painter.roundRect(rect, frame);
					const Color cross = color(node.style, PropId::Fg, Color::fromRGBA8(0x475569FFu)).withAlpha(0.45f);
					painter.line({ rect.x + 2.f, rect.y + 2.f }, { rect.right() - 2.f, rect.bottom() - 2.f }, 1.f, cross);
					painter.line({ rect.right() - 2.f, rect.y + 2.f }, { rect.x + 2.f, rect.bottom() - 2.f }, 1.f, cross);
				}
				painter.popOpacity(); continue;
			}
			if (node.radioGroup) {
				const Color accent = color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu));
				const f32 rowHeight = node.options.empty() ? rect.h : rect.h / f32(node.options.size());
				for (u32 i = 0; i < node.options.size(); ++i) {
					// Row-local hover highlight; the control's own :hover style still applies to the whole group.
					if (static_cast<i32>(i) == node.hoveredOption && !node.disabled)
						painter.fillRect({ rect.x, rect.y + rowHeight * f32(i), rect.w, rowHeight }, accent.withAlpha(0.08f), CornerRadii::all(2.f));
					const f32 diameter = std::clamp(rowHeight - 12.f, 10.f, 16.f);
					const f32 cy = rect.y + rowHeight * (f32(i) + 0.5f);
					const Rect ring{ rect.x + 2.f, cy - diameter * 0.5f, diameter, diameter };
					RectStyle ringStyle;
					ringStyle.border = i == node.selectedOption ? accent : accent.withAlpha(0.55f);
					ringStyle.borderWidth = i == node.selectedOption ? 2.f : 1.f;
					ringStyle.radius = CornerRadii::all(diameter * 0.5f);
					painter.roundRect(ring, ringStyle);
					if (i == node.selectedOption) {
						const f32 dot = diameter * 0.45f;
						painter.fillRect({ ring.x + (diameter - dot) * 0.5f, cy - dot * 0.5f, dot, dot }, accent, CornerRadii::all(dot * 0.5f));
					}
					drawLabel(node.options[i], { rect.x, rect.y + rowHeight * f32(i), rect.w, rowHeight }, node.font, node.style, accent, 26.f, false);
				}
				painter.popOpacity(); continue;
			}
			if (node.segmented) {
				const Color accent = color(node.style, PropId::Fg, Color::fromRGBA8(0x4772B3FFu));
				const f32 segmentWidth = node.options.empty() ? rect.w : rect.w / f32(node.options.size());
				for (u32 i = 0; i < node.options.size(); ++i) {
					const Rect cell{ rect.x + segmentWidth * f32(i), rect.y, segmentWidth, rect.h };
					RectStyle cellStyle;
					cellStyle.radius = CornerRadii::all(4.f);
					if (i == node.selectedOption) { cellStyle.fill = accent; cellStyle.border = accent; cellStyle.borderWidth = 1.f; }
					else if (static_cast<i32>(i) == node.hoveredOption && !node.disabled) { cellStyle.fill = accent.withAlpha(0.2f); }
					if (cellStyle.fill.a > 0 || cellStyle.borderWidth > 0) painter.roundRect(cell, cellStyle);
					drawLabel(node.options[i], cell, node.font, node.style,
						i == node.selectedOption ? Color::fromRGBA8(0xFFFFFFFFu) : accent, 6.f, false);
				}
				painter.popOpacity(); continue;
			}
			if (node.stepper) {
				const Color mark = color(node.style, PropId::Fg, Color::fromRGBA8(0xD1D1D1FFu));
				const f32 button = std::min(20.f, rect.w * 0.25f);
				const Rect minus{ rect.right() - button * 2.f, rect.y, button, rect.h };
				const Rect plus{ rect.right() - button, rect.y, button, rect.h };
				RectStyle divider;
				divider.border = Color::fromRGBA8(0x3A3A3AFFu); divider.borderWidth = 1.f;
				painter.line({ minus.x, rect.y }, { minus.x, rect.bottom() }, 1.f, divider.border);
				painter.line({ plus.x, rect.y }, { plus.x, rect.bottom() }, 1.f, divider.border);
				if (node.stepperZone) {
					const Rect& hot = node.stepperZone == 1 ? minus : plus;
					painter.fillRect(hot, mark.withAlpha(0.12f));
				}
				painter.line({ minus.x + 5.f, minus.y + minus.h * 0.5f }, { minus.right() - 5.f, minus.y + minus.h * 0.5f }, 1.5f, mark);
				painter.line({ plus.x + plus.w * 0.5f, plus.y + 6.f }, { plus.x + plus.w * 0.5f, plus.bottom() - 6.f }, 1.5f, mark);
				painter.line({ plus.x + 5.f, plus.y + plus.h * 0.5f }, { plus.right() - 5.f, plus.y + plus.h * 0.5f }, 1.5f, mark);
				drawLabel(node.text, { rect.x, rect.y, rect.w - button * 2.f, rect.h }, node.font, node.style, mark, 8.f, false);
				painter.popOpacity(); continue;
			}
			if (node.rating) {
				const Color accent = color(node.style, PropId::Fg, Color::fromRGBA8(0x4772B3FFu));
				const i32 pips = static_cast<i32>(std::clamp(node.maxValue, 1.f, 10.f));
				const f32 spacing = rect.w / f32(pips);
				for (i32 i = 0; i < pips; ++i) {
					const f32 cx = rect.x + spacing * (f32(i) + 0.5f), cy = rect.y + rect.h * 0.5f;
					const f32 radius = std::min(7.f, spacing * 0.3f);
					const bool filled = f32(i) < node.value;
					const bool hovered = i == node.hoveredOption && !node.disabled;
					if (filled || hovered)
						painter.fillRect({ cx - radius, cy - radius, radius * 2.f, radius * 2.f },
							filled ? accent : accent.withAlpha(0.35f), CornerRadii::all(radius));
					else
						painter.roundRect({ cx - radius, cy - radius, radius * 2.f, radius * 2.f },
							RectStyle{ Color{0,0,0,0}, accent.withAlpha(0.45f), 1.f, CornerRadii::all(radius) });
				}
				painter.popOpacity(); continue;
			}
			if (node.breadcrumb) {
				const Color accent = color(node.style, PropId::Fg, Color::fromRGBA8(0xD1D1D1FFu));
				const f32 fontSize = node.style.has(PropId::FontSize) ? number(node.style.get(PropId::FontSize), 0, 14) : 14.f;
				f32 x = rect.x;
				for (u32 i = 0; i < node.options.size(); ++i) {
					const f32 width = m_text && node.font.valid() ? m_text->layout(node.font, node.options[i], fontSize, 0.f).width / m_dpiScale : 0.f;
					if (static_cast<i32>(i) == node.hoveredOption && !node.disabled)
						painter.fillRect({ x, rect.y, width + 8.f, rect.h }, accent.withAlpha(0.1f), CornerRadii::all(3.f));
					drawLabel(node.options[i], { x, rect.y, width + 12.f, rect.h }, node.font, node.style,
						i == node.selectedOption ? color(node.style, PropId::Border, accent) : accent, 0.f, false);
					x += width + 8.f;
					if (i + 1 < node.options.size()) {
						drawLabel("›", { x, rect.y, 12.f, rect.h }, node.font, node.style, accent.withAlpha(0.55f), 0.f, false);
						x += 14.f;
					}
				}
				painter.popOpacity(); continue;
			}
			if (node.keybindField) {
				if (node.focused && node.text.empty())
					drawLabel(node.placeholder.empty() ? "Press keys…" : node.placeholder, rect, node.font, node.style,
						color(node.style, PropId::Fg, Color::fromRGBA8(0x9A9A9AFFu)).withAlpha(0.6f), 8.f, false);
				else if (!node.text.empty())
					drawLabel(node.text, rect, node.font, node.style, color(node.style, PropId::Fg, Color::fromRGBA8(0xD1D1D1FFu)), 8.f, false);
				painter.popOpacity(); continue;
			}
			if (node.numberField && !node.disabled) {
				// Stepper buttons in the reserved right zone; clicks are routed in pointerDown().
				const Color mark = color(node.style, PropId::Fg, Color::fromRGBA8(0xD1D1D1FFu)).withAlpha(0.7f);
				const f32 button = std::min(20.f, rect.w * 0.22f);
				const Rect minus{ rect.right() - button * 2.f, rect.y, button, rect.h };
				const Rect plus{ rect.right() - button, rect.y, button, rect.h };
				painter.line({ minus.x, rect.y + 3.f }, { minus.x, rect.bottom() - 3.f }, 1.f, mark.withAlpha(0.35f));
				painter.line({ plus.x, rect.y + 3.f }, { plus.x, rect.bottom() - 3.f }, 1.f, mark.withAlpha(0.35f));
				painter.line({ minus.x + 5.f, minus.y + minus.h * 0.5f }, { minus.right() - 5.f, minus.y + minus.h * 0.5f }, 1.5f, mark);
				painter.line({ plus.x + plus.w * 0.5f, plus.y + 7.f }, { plus.x + plus.w * 0.5f, plus.bottom() - 7.f }, 1.5f, mark);
				painter.line({ plus.x + 5.f, plus.y + plus.h * 0.5f }, { plus.right() - 5.f, plus.y + plus.h * 0.5f }, 1.5f, mark);
			}
			const bool placeholderText = (node.textBox || node.textArea) && node.text.empty() && !node.placeholder.empty();
			std::string passwordMask;
			std::string_view displayText = node.dockPanel ? std::string_view{} :
				(placeholderText ? std::string_view(node.placeholder) : std::string_view(node.text));
			if (node.password && !node.text.empty()) {
				for (std::size_t i = 0; i < node.text.size(); i = nextUtf8(node.text, i)) appendUtf8(passwordMask, 0x2022u);
				displayText = passwordMask;
			}
			std::string visualText;
			if (m_showWhitespace && (node.textBox || node.textArea) && !placeholderText && !node.password) {
				visualText = visualizeWhitespace(node.text);
				displayText = visualText;
			}
			// Caret and selection prefixes must be measured through the same substitution as the
			// displayed glyphs, or the caret would drift from the drawn characters.
			auto caretPrefixText = [&](std::size_t byteOffset) {
				const std::string raw(node.text.data(), std::min(byteOffset, node.text.size()));
				return (m_showWhitespace && (node.textBox || node.textArea) && !node.password) ? visualizeWhitespace(raw) : raw;
				};
			if (displayText.empty()) { painter.popOpacity(); continue; }

			if (!node.font.valid()) { painter.popOpacity(); continue; }
			const Color fg = color(node.style, PropId::Fg, Color::fromRGBA8(0xF5F5F5FFu));
			const f32 fontSize = node.style.has(PropId::FontSize) ? number(node.style.get(PropId::FontSize), 0, 16) : 16.f;
			const f32 rasterSize = fontSize * m_dpiScale;
			const f32 letterSpacing = node.style.has(PropId::LetterSpacing) ? number(node.style.get(PropId::LetterSpacing), 0) : 0.f;
			const Edges textPadding = node.hasXuiPadding ? node.xuiPadding : node.style.has(PropId::Pad)
				? Edges{ number(node.style.get(PropId::Pad), 3), number(node.style.get(PropId::Pad), 0),
						number(node.style.get(PropId::Pad), 1), number(node.style.get(PropId::Pad), 2) } : Edges{};
			const f32 borderInset = node.style.has(PropId::Border) && node.style.get(PropId::Border).count
				? number(node.style.get(PropId::Border), 0) : 0.f;
			const f32 padTop = textPadding.t + borderInset;
			const f32 padBottom = textPadding.b + borderInset;
			const f32 textLeft = textPadding.l + borderInset;
			const f32 textRight = textPadding.r + borderInset + ((node.dropdown || node.colorPicker) ? 30.f : (node.numberField ? 44.f : (node.searchField && !node.text.empty() ? 18.f : 0.f)));
			const f32 switchTrackWidth = node.switchToggle ? std::clamp(std::clamp(rect.h - 8.f, 18.f, 26.f) * 1.8f, 36.f, 58.f) : 0.f;
			const f32 checkOffset = (node.checkItem || node.checkBox) ? 26.f : (node.switchToggle ? switchTrackWidth + 10.f : 0.f);
			const f32 contentWidth = std::max(0.f, rect.w - textLeft - textRight - checkOffset);
			const f32 wrapWidth = node.textBox ? 0.f : contentWidth * m_dpiScale;
			const TextLayout& run = m_text->layout(node.font, displayText, rasterSize, wrapWidth);
			const Font* font = m_text->font(node.font);
			GlyphAtlas* atlas = m_text->atlas();
			if (!font || !atlas->ready() || run.glyphs.empty()) { painter.popOpacity(); continue; }

			m_glyphScratch.clear();
			if (m_glyphScratch.capacity() < run.glyphs.size()) m_glyphScratch.reserve(run.glyphs.size());
			for (std::size_t glyphIndex = 0; glyphIndex < run.glyphs.size(); ++glyphIndex) {
				const ShapedGlyph& shaped = run.glyphs[glyphIndex];
				const AtlasGlyph glyph = atlas->glyph(*font, shaped.glyph, static_cast<u16>(std::clamp(std::lround(rasterSize), 1l, 65535l)));
				const f32 scale = 1.f / m_dpiScale;
				m_glyphScratch.push_back({ Rect{(shaped.x + glyph.offset.x) * scale + letterSpacing * static_cast<f32>(glyphIndex), (shaped.y + glyph.offset.y) * scale, glyph.width * scale, glyph.height * scale}, glyph.uv });
			}
			f32 x = rect.x;
			const f32 padLeft = textPadding.l;
			const f32 padRight = textPadding.r;
			const f32 runWidth = run.width / m_dpiScale + letterSpacing * static_cast<f32>(run.glyphs.size() > 0 ? run.glyphs.size() - 1 : 0);
			const f32 runHeight = run.height / m_dpiScale;
			const f32 baseline = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f) + font->baselineAt(rasterSize) / m_dpiScale;
			const Align textAlign = node.style.has(PropId::Align) ? align(node.style.get(PropId::Align).atoms[0]) : Align::Start;
			const bool textCenter = node.button || node.toggleButton || (!node.checkItem && textAlign == Align::Center);
			if (node.checkItem || node.checkBox || node.switchToggle) x += textLeft + checkOffset;
			else if (node.dropdown) x += padLeft;
			else if (node.colorPicker) x += padLeft;
			else if (textCenter) x += textPadding.l + std::max(0.f, (rect.w - textPadding.l - textPadding.r - runWidth) * 0.5f);
			else if (!node.checkItem && textAlign == Align::End) x += rect.w - runWidth - padRight;
			else x += padLeft;
			if (node.textBox && !placeholderText && node.focused) {
				const TextLayout& caretRun = m_text->layout(node.font, caretPrefixText(node.caret), rasterSize, 0.f);
				const f32 caretPosition = caretRun.width / m_dpiScale;
				x = rect.x + textLeft - std::clamp(caretPosition - contentWidth, 0.f, std::max(0.f, runWidth - contentWidth));
			}
			else if (node.textArea || node.textBox) x = rect.x + textLeft;
			else if (x + runWidth > rect.right() - padRight) x = rect.right() - padRight - runWidth;
			painter.pushClip({ rect.x + textLeft + checkOffset, rect.y + padTop, std::max(0.f, rect.w - textLeft - textRight - checkOffset), std::max(0.f, rect.h - padTop - padBottom) });
			if ((node.textBox || node.textArea) && node.focused && node.caret != node.selectionAnchor && !placeholderText) {
				const std::size_t begin = std::min(node.caret, node.selectionAnchor);
				const std::size_t end = std::max(node.caret, node.selectionAnchor);
				auto caretAt = [&](std::size_t byteOffset) {
					const std::string prefixText = caretPrefixText(byteOffset);
					const TextLayout& prefix = m_text->layout(node.font, prefixText, rasterSize, node.textArea ? contentWidth * m_dpiScale : 0.f);
					const u32 line = prefix.lineStarts.empty() ? 0u : static_cast<u32>(prefix.lineStarts.size() - 1);
					const u32 lineFirst = prefix.lineStarts.empty() ? 0u : prefix.lineStarts.back();
					const f32 cx = prefix.lineWidths.empty() ? 0.f : prefix.lineWidths.back() / m_dpiScale;
					(void)lineFirst;
					return std::pair<u32, f32>{line, cx};
					};
				const auto [firstLine, firstX] = caretAt(begin);
				const auto [lastLine, lastX] = caretAt(end);
				const f32 lineHeight = std::max(fontSize, font->lineHeight(rasterSize) / m_dpiScale);
				const f32 textTop = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f);
				for (u32 line = firstLine; line <= lastLine; ++line) {
					const f32 left = line == firstLine ? firstX : 0.f;
					const f32 right = line == lastLine ? lastX : contentWidth;
					painter.fillRect({ x + left, textTop + f32(line) * lineHeight, std::max(2.f, right - left), lineHeight }, fg.withAlpha(0.28f));
				}
			}
			painter.glyphRun(atlas->texture(), m_glyphScratch.data(), static_cast<u32>(m_glyphScratch.size()), { x, baseline }, fg);
			if (node.searchField && !node.text.empty()) {
				const Rect clearRect = searchClearRect(node);
				const Color mark = fg.withAlpha(0.7f);
				const f32 inset = clearRect.w * 0.3f;
				painter.line({ clearRect.x + inset, clearRect.y + inset }, { clearRect.right() - inset, clearRect.bottom() - inset }, 1.2f, mark);
				painter.line({ clearRect.right() - inset, clearRect.y + inset }, { clearRect.x + inset, clearRect.bottom() - inset }, 1.2f, mark);
			}
			if ((node.textBox || node.textArea) && node.focused && !node.disabled && !node.readOnly && !placeholderText && m_caretVisible) {
				const std::string prefix = caretPrefixText(node.caret);
				const TextLayout& caretRun = m_text->layout(node.font, prefix, rasterSize, node.textArea ? contentWidth * m_dpiScale : 0.f);
				f32 caretX = 0.f, caretY = 0.f;
				if (!caretRun.lineWidths.empty()) caretX = caretRun.lineWidths.back() / m_dpiScale;
				if (!caretRun.lineStarts.empty()) caretY = f32(caretRun.lineStarts.size() - 1) * caretRun.lineHeight / m_dpiScale;
				if (node.textBox) {
					const f32 caretPosition = caretRun.width / m_dpiScale;
					const f32 offset = std::clamp(caretPosition - contentWidth, 0.f, std::max(0.f, runWidth - contentWidth));
					caretX = caretPosition - offset;
				}
				const f32 y = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f) + caretY;
				painter.line({ x + caretX, y + 1.f }, { x + caretX, y + std::max(2.f, fontSize - 1.f) }, 1.f, fg);
			}
			painter.popClip();
			painter.popOpacity();
		}

		// Tabs and popups render after the retained tree, so their chrome stays above page content.
		painter.setLayer(1);
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			if (!node.scrollView || !isNodeVisible(i) || node.scrollBarVisibility == 2) continue;
			struct ScrollbarClipScope { Painter& painter; u32 count = 0; ~ScrollbarClipScope() { while (count--) painter.popClip(); } } scrollbarClips{ painter };
			for (u32 parent = node.parentIndex; parent != 0xFFFFFFFFu && parent < m_nodes.size(); parent = m_nodes[parent].parentIndex)
				if (m_nodes[parent].scrollView) { painter.pushClip(m_layout.absRect(m_nodes[parent].layout)); ++scrollbarClips.count; }
			const bool always = node.scrollBarVisibility == 1;
			const f32 radius = node.style.has(PropId::Radius) ? number(node.style.get(PropId::Radius), 0, node.scrollBarSize * 0.5f) : node.scrollBarSize * 0.5f;
			for (bool horizontal : { false, true }) {
				Rect track;
				const Rect thumb = scrollBarRect(i, horizontal, &track);
				if (thumb.empty()) continue;
				if (!always && !node.hovered && !node.focused) continue;
				const Color trackColor = color(node.style, PropId::ScrollTrack, Color::fromRGBA8(0x00000030u));
				const Color thumbColor = color(node.style, PropId::ScrollThumb, Color::fromRGBA8(0xA0A0A0C0u));
				painter.fillRect(track, trackColor, CornerRadii::all(radius));
				painter.fillRect(thumb, thumbColor, CornerRadii::all(radius));
			}
		}
		for (u32 tabsIndex = 0; tabsIndex < m_nodes.size(); ++tabsIndex) {
			Node& tabs = m_nodes[tabsIndex];
			if (!tabs.tabs || !isNodeVisible(tabsIndex)) continue;
			const Rect bounds = m_layout.absRect(tabs.layout);
			const u32 count = m_layout.childCount(tabs.layout);
			if (!count) continue;
			u32 ordinal = 0;
			for (NodeId child = m_layout.firstChild(tabs.layout); child.valid(); child = m_layout.nextSibling(child), ++ordinal) {
				Node& tab = m_nodes[child.index];
				const Rect header{ bounds.x + bounds.w * f32(ordinal) / f32(count), bounds.y,
								  bounds.w * f32(ordinal + 1) / f32(count) - bounds.w * f32(ordinal) / f32(count), 34.f };
				PropTable tabStyle;
				const StateMask state = stateBit(State::Resting) |
					(ordinal == tabs.selectedTab ? stateBit(State::Selected) : 0) |
					(header.contains(m_pointerPosition) ? stateBit(State::Hover) : 0) |
					(tabs.disabled ? stateBit(State::Disabled) : 0);
				m_styles->resolve(tab.styleSet, state, m_theme, tabStyle);
				const Color background = color(tabStyle, PropId::Bg,
					ordinal == tabs.selectedTab ? Color::fromRGBA8(0x24334FFFu) : Color::fromRGBA8(0x151E32FFu));
				painter.fillRect(header, background);
				if (tabStyle.has(PropId::Border) && tabStyle.get(PropId::Border).count >= 3) {
					const auto& border = tabStyle.get(PropId::Border);
					const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
					painter.roundRect(header, RectStyle{ Color{0,0,0,0}, edge, number(border, 0), CornerRadii{} });
				}
				drawLabel(tab.tabTitle, header, tab.font, tabStyle, Color::fromRGBA8(0xDDE7F8FFu), 8.f, false);
			}
		}
		// Dock chrome is a backend-neutral overlay. Every visual part resolves through .xss selectors.
		for (const DockHostState& host : m_dockHosts) {
			if (host.node >= m_nodes.size() || !isNodeVisible(host.node)) continue;
			const Node& hostNode = m_nodes[host.node];
			const Rect hostRect = m_layout.absRect(hostNode.layout);
			const LayoutStyle& hostLayout = m_layout.style(hostNode.layout);
			const Vec2 origin{ hostRect.x + hostLayout.border.l + hostLayout.padding.l,
				hostRect.y + hostLayout.border.t + hostLayout.padding.t };
			const auto resolvePart = [&](StyleSetId set, StateMask states, PropTable& table) {
				m_styles->resolve(set, states, m_theme, table);
			};
			const auto drawChrome = [&](const Node& owner, StyleSetId set, Rect bounds, StateMask states,
				Color fallbackBg, Color fallbackFg, std::string_view label, f32 inset) {
				PropTable style;
				resolvePart(set, states, style);
				const Color background = color(style, PropId::Bg, fallbackBg);
				const f32 radius = style.has(PropId::Radius) ? number(style.get(PropId::Radius), 0) : 0.f;
				const f32 opacity = style.has(PropId::Opacity) ? std::clamp(number(style.get(PropId::Opacity), 0, 1.f), 0.f, 1.f) : 1.f;
				const f32 textInset = style.has(PropId::Pad) ? number(style.get(PropId::Pad), 3, inset) : inset;
				painter.pushOpacity(opacity);
				if (background.a > 0.f) painter.fillRect(bounds, background, CornerRadii::all(radius));
				if (style.has(PropId::Border) && style.get(PropId::Border).count >= 3) {
					const StyleValue& border = style.get(PropId::Border);
					const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
					painter.roundRect(bounds, RectStyle{ Color{0,0,0,0}, edge, number(border, 0), CornerRadii::all(radius) });
				}
				const FontHandle styledFont = selectFont(style);
				drawLabel(label, bounds, styledFont.valid() ? styledFont : owner.font, style, fallbackFg, textInset, false);
				painter.popOpacity();
			};
			const std::span<const DockPanelLayout> panels = host.tree.panels();
			for (const DockPanelLayout& panelLayout : panels) {
				const u32 panelIndex = panelLayout.panel;
				if (!panelLayout.active || panelIndex >= m_nodes.size() || panelLayout.bounds.empty()) continue;
				const Node& panel = m_nodes[panelIndex];
				const Rect group = panelLayout.bounds;
				const u32 count = host.tree.panelCount(panelIndex);
				if (panelLayout.floating || count <= 1) {
					const Rect header{ origin.x + group.x, origin.y + group.y, group.w,
						std::min(group.h, panel.dockHeaderHeight) };
					StateMask states = stateBit(State::Resting) | (panel.disabled ? stateBit(State::Disabled) : stateBit(State::Enabled));
					if (!panel.disabled && header.contains(m_pointerPosition)) states |= stateBit(State::Hover);
					if (m_dockCandidate == panelIndex) states |= stateBit(State::Pressed);
					drawChrome(panel, panel.dockHeaderStyleSet, header, states,
						Color::fromRGBA8(0x263247FFu), Color::fromRGBA8(0xE6EDF7FFu), panel.text, 10.f);
					if (panel.dockClosable) {
						const f32 side = header.h;
						const Rect close{ header.right() - side, header.y, side, side };
						StateMask closeStates = stateBit(State::Resting) | (panel.disabled ? stateBit(State::Disabled) : stateBit(State::Enabled));
						if (!panel.disabled && close.contains(m_pointerPosition)) closeStates |= stateBit(State::Hover);
						drawChrome(panel, panel.dockCloseStyleSet, close, closeStates,
							Color{0,0,0,0}, Color::fromRGBA8(0xCBD5E1FFu), "×", 0.f);
					}
				}
				else {
					const f32 tabWidth = group.w / f32(count);
					for (u32 order = 0; order < count; ++order) {
						u32 tabNode = 0xFFFFFFFFu;
						for (u32 candidate : host.panelNodes)
							if (host.tree.groupOf(candidate) == panelLayout.group && host.tree.panelOrder(candidate) == order) { tabNode = candidate; break; }
						if (tabNode >= m_nodes.size()) continue;
						const Node& tab = m_nodes[tabNode];
						const Rect bounds{ origin.x + group.x + tabWidth * f32(order), origin.y + group.y,
							tabWidth, std::min(group.h, tab.dockHeaderHeight) };
						StateMask states = stateBit(State::Resting) | (tab.disabled ? stateBit(State::Disabled) : stateBit(State::Enabled));
						if (order == panelLayout.order) states |= stateBit(State::Selected);
						if (!tab.disabled && bounds.contains(m_pointerPosition)) states |= stateBit(State::Hover);
						if (m_dockCandidate == tabNode) states |= stateBit(State::Pressed);
						drawChrome(tab, tab.dockTabStyleSet, bounds, states,
							order == panelLayout.order ? Color::fromRGBA8(0x33445EFFu) : Color::fromRGBA8(0x202A3AFFu),
							Color::fromRGBA8(0xE6EDF7FFu), tab.text, tab.dockIcon.empty() ? 8.f : 28.f);
						if (!tab.dockIcon.empty()) {
							PropTable iconStyle;
							resolvePart(tab.dockTabStyleSet, states, iconStyle);
							const FontHandle iconFont = selectFont(iconStyle);
							drawLabel(tab.dockIcon, bounds, iconFont.valid() ? iconFont : tab.font, iconStyle,
								Color::fromRGBA8(0xE6EDF7FFu), 8.f, false);
						}
						if (tab.dockClosable) {
							const Rect close{ bounds.right() - bounds.h, bounds.y, bounds.h, bounds.h };
						StateMask closeStates = stateBit(State::Resting) | (tab.disabled ? stateBit(State::Disabled) : stateBit(State::Enabled));
						if (!tab.disabled && close.contains(m_pointerPosition)) closeStates |= stateBit(State::Hover);
						drawChrome(tab, tab.dockCloseStyleSet, close, closeStates,
							Color{0,0,0,0}, Color::fromRGBA8(0xCBD5E1FFu), "×", 0.f);
					}
					}
				}
			}
			for (u32 ordinal = 0; ordinal < host.tree.splitterCount(); ++ordinal) {
				const DockSplitterLayout split = host.tree.splitterAt(ordinal);
				Rect bounds = split.bounds;
				bounds.x += origin.x; bounds.y += origin.y;
				StateMask states = stateBit(State::Resting) | stateBit(State::Enabled);
				if (bounds.contains(m_pointerPosition)) states |= stateBit(State::Hover);
				if (m_dockSplitDragging && m_dockDragHost == static_cast<u32>(&host - m_dockHosts.data()) &&
					m_dockDragSplit == split.node) states |= stateBit(State::Pressed) | stateBit(State::Dragging);
				drawChrome(hostNode, hostNode.dockSplitterStyleSet, bounds, states,
					Color::fromRGBA8(0x46546AFFu), Color::fromRGBA8(0x46546AFFu), {}, 0.f);
			}
			if (m_dockPreview.valid && m_dockDragHost == static_cast<u32>(&host - m_dockHosts.data())) {
				Rect preview = m_dockPreview.bounds;
				preview.x += origin.x; preview.y += origin.y;
				PropTable style;
				resolvePart(hostNode.dockPreviewStyleSet, stateBit(State::Resting) | stateBit(State::Dragging), style);
				const f32 radius = style.has(PropId::Radius) ? number(style.get(PropId::Radius), 0, 4.f) : 4.f;
				const f32 opacity = style.has(PropId::Opacity) ? std::clamp(number(style.get(PropId::Opacity), 0, 1.f), 0.f, 1.f) : 1.f;
				painter.pushOpacity(opacity);
				painter.fillRect(preview, color(style, PropId::Bg, Color::fromRGBA8(0x5386D866u)), CornerRadii::all(radius));
				if (style.has(PropId::Border) && style.get(PropId::Border).count >= 3) {
					const StyleValue& border = style.get(PropId::Border);
					const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
					painter.roundRect(preview, RectStyle{ Color{0,0,0,0}, edge, number(border, 0, 1.f), CornerRadii::all(radius) });
				}
				painter.popOpacity();
			}
		}
		// Popups render after the retained tree, so they always remain on top of later siblings.
		for (Node& node : m_nodes) {
			if (!node.popupOpen || (!node.dropdown && !node.colorPicker && !node.datePicker)) continue;
			const Rect anchor = m_layout.absRect(node.layout);
			const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight,
				node.datePicker ? 224.f : node.pickerWidth, node.datePicker ? 208.f : node.pickerHeight);
			const Color panel = color(node.style, PropId::Bg, Color::fromRGBA8(0x151E32FFu));
			const Color foreground = color(node.style, PropId::Fg, Color::fromRGBA8(0xF2F6FFFFu));
			painter.fillRect(popup, panel, CornerRadii::all(5.f));
			if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
				const auto& border = node.style.get(PropId::Border);
				const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
				painter.roundRect(popup, RectStyle{ Color{0,0,0,0}, edge, number(border, 0), CornerRadii::all(5.f) });
			}
			if (node.dropdown) {
				for (u32 i = 0; i < node.options.size(); ++i) {
					const Rect row{ popup.x, popup.y + 28.f * i, popup.w, 28.f };
					if (i == node.selectedOption) painter.fillRect(row, foreground.withAlpha(0.12f), CornerRadii::all(2.f));
					if (static_cast<i32>(i) == node.hoveredOption) {
						// Popup rows are virtual option targets, not descendants of the header.
						// Use a row-local highlight so the header's :hover style cannot leak here.
						painter.fillRect(row, foreground.withAlpha(0.16f), CornerRadii::all(2.f));
					}
					drawLabel(node.options[i], row, node.font, node.style, foreground, 8.f, false);
				}
			}
			else if (node.datePicker) {
				static const char* const weekdays[7] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
				const Color accent = color(node.style, PropId::Fg, Color::fromRGBA8(0x4772B3FFu));
				const Rect header{ popup.x, popup.y, popup.w, 26.f };
				[[maybe_unused]] RectStyle headerDivider;
				painter.fillRect(header, foreground.withAlpha(0.06f));
				drawLabel("‹", { popup.x, popup.y, 26.f, 26.f }, node.font, node.style, foreground, 8.f, false);
				drawLabel("›", { popup.right() - 26.f, popup.y, 26.f, 26.f }, node.font, node.style, foreground, 8.f, false);
				drawLabel(std::string(kMonthNames[std::clamp<u32>(node.viewMonth, 1u, 12u) - 1u]) + " " + std::to_string(node.viewYear),
					header, node.font, node.style, foreground, 0.f, false);
				const f32 cellWidth = popup.w / 7.f;
				for (u32 column = 0; column < 7; ++column)
					drawLabel(weekdays[column], { popup.x + cellWidth * f32(column), popup.y + 28.f, cellWidth, 16.f },
						node.font, node.style, foreground.withAlpha(0.55f), 6.f, false);
				const u32 firstWeekday = weekdayMonday0(node.viewYear, node.viewMonth, 1u);
				const u32 monthDays = daysInMonth(node.viewYear, node.viewMonth);
				const bool viewingSelection = node.viewYear == node.dateYear && node.viewMonth == node.dateMonth;
				for (u32 cell = 0; cell < 42u; ++cell) {
					const i32 day = static_cast<i32>(cell) - static_cast<i32>(firstWeekday) + 1;
					if (day < 1 || day > static_cast<i32>(monthDays)) continue;
					const Rect cellRect{ popup.x + cellWidth * f32(cell % 7u), popup.y + 46.f + 22.f * f32(cell / 7u), cellWidth, 22.f };
					if (viewingSelection && static_cast<u32>(day) == node.dateDay)
						painter.fillRect(cellRect, accent, CornerRadii::all(3.f));
					drawLabel(std::to_string(day), cellRect, node.font, node.style,
						viewingSelection && static_cast<u32>(day) == node.dateDay ? Color::fromRGBA8(0xFFFFFFFFu) : foreground, 9.f, false);
				}
			}
			else {
				const PickerGeometry geometry = pickerGeometry(node);
				constexpr u32 columns = 48, rows = 22;
				f32 hue = 0.f;
				const f32 hi = std::max({ node.pickerColor.r,node.pickerColor.g,node.pickerColor.b }), lo = std::min({ node.pickerColor.r,node.pickerColor.g,node.pickerColor.b }), delta = hi - lo;
				f32 saturation = hi > 0.f ? delta / hi : 0.f, value = hi;
				if (delta > 0.f) {
					if (hi == node.pickerColor.r) hue = std::fmod((node.pickerColor.g - node.pickerColor.b) / delta, 6.f) / 6.f;
					else if (hi == node.pickerColor.g) hue = ((node.pickerColor.b - node.pickerColor.r) / delta + 2.f) / 6.f;
					else hue = ((node.pickerColor.r - node.pickerColor.g) / delta + 4.f) / 6.f;
					if (hue < 0.f) hue += 1.f;
				}
				if (!geometry.saturationValue.empty()) for (u32 row = 0; row < rows; ++row) for (u32 col = 0; col < columns; ++col) {
					const f32 svSaturation = f32(col) / f32(columns - 1), svValue = 1.f - f32(row) / f32(rows - 1);
					const Rect cell{ geometry.saturationValue.x + geometry.saturationValue.w * f32(col) / columns,
						geometry.saturationValue.y + geometry.saturationValue.h * f32(row) / rows,
						geometry.saturationValue.w / columns + 0.5f, geometry.saturationValue.h / rows + 0.5f };
					painter.fillRect(cell, hsv(hue, svSaturation, svValue));
				}
				const Color indicator = color(node.style, PropId::Fg, Color::fromRGBA8(0xFFFFFFFFu));
				const f32 markerX = geometry.saturationValue.x + std::clamp(saturation, 0.f, 1.f) * geometry.saturationValue.w;
				const f32 markerY = geometry.saturationValue.y + std::clamp(1.f - value, 0.f, 1.f) * geometry.saturationValue.h;
				if (!geometry.saturationValue.empty()) painter.roundRect({ markerX - 6.f,markerY - 6.f,12.f,12.f }, RectStyle{ Color{0,0,0,0},Color::fromRGBA8(0x101010FFu),3.f,CornerRadii::all(6.f) });
				if (!geometry.saturationValue.empty()) painter.roundRect({ markerX - 4.f,markerY - 4.f,8.f,8.f }, RectStyle{ Color{0,0,0,0},indicator,2.f,CornerRadii::all(4.f) });
				constexpr u32 hues = 48;
				if (!geometry.hue.empty()) for (u32 col = 0; col < hues; ++col) {
					const Rect cell{ geometry.hue.x + geometry.hue.w * f32(col) / hues, geometry.hue.y, geometry.hue.w / hues + 0.5f, geometry.hue.h };
					painter.fillRect(cell, hsv(f32(col) / hues, 1.f, 1.f));
				}
				const f32 hueX = geometry.hue.x + std::clamp(hue, 0.f, 1.f) * geometry.hue.w;
				if (!geometry.hue.empty()) painter.roundRect({ hueX - 3.f,geometry.hue.y - 2.f,6.f,geometry.hue.h + 4.f }, RectStyle{ Color::fromRGBA8(0xFFFFFFFFu),Color::fromRGBA8(0x151E32FFu),1.f,CornerRadii::all(3.f) });
				if (!geometry.alpha.empty()) {
					for (f32 cy = 0; cy < geometry.alpha.h; cy += 6.f) for (f32 cx = 0; cx < geometry.alpha.w; cx += 8.f) {
						const bool alternate = (static_cast<u32>(cx / 8.f) + static_cast<u32>(cy / 6.f)) % 2u != 0;
						painter.fillRect({ geometry.alpha.x + cx, geometry.alpha.y + cy, 8.f, 6.f }, alternate ? Color::fromRGBA8(0x697386FFu) : Color::fromRGBA8(0x374151FFu));
					}
					constexpr u32 alphaSteps = 48;
					for (u32 step = 0;step < alphaSteps;++step) {
						Color sample = node.pickerColor; sample.a = f32(step) / f32(alphaSteps - 1);
						painter.fillRect({ geometry.alpha.x + f32(step) * geometry.alpha.w / alphaSteps,geometry.alpha.y,
							geometry.alpha.w / alphaSteps + 0.5f,geometry.alpha.h }, sample);
					}
					const f32 alphaX = geometry.alpha.x + node.pickerColor.a * geometry.alpha.w;
					painter.roundRect({ alphaX - 2.f,geometry.alpha.y - 2.f,4.f,geometry.alpha.h + 4.f }, RectStyle{ Color::fromRGBA8(0xFFFFFFFFu),Color::fromRGBA8(0x151E32FFu),1.f,CornerRadii::all(2.f) });
				}
				painter.fillRect({ geometry.format.x,geometry.format.y + 3.f,24.f,22.f }, node.pickerColor, CornerRadii::all(4.f));
				if (node.pickerShowFormatSwitcher) {
					const Rect switcher{ geometry.format.x + 34.f,geometry.format.y,110.f,geometry.format.h };
					painter.fillRect(switcher, foreground.withAlpha(0.08f), CornerRadii::all(5.f));
					drawLabel(std::string(node.pickerFormat).append("  ▾"), switcher, node.font, node.style, foreground, 12.f, false);
				} else drawLabel(node.pickerFormat, geometry.format, node.font, node.style, foreground, 0.f, false);
				if (node.pickerShowInputs) {
					const char* labels = (node.pickerFormat == "rgb" || node.pickerFormat == "hex") ? "RGBA" :
						node.pickerFormat == "hsv" ? "HSVA" : "HSLA";
					for (u32 channel = 0; channel < (node.pickerAlpha ? 4u : 3u); ++channel)
						drawLabel(std::string_view(labels + channel, 1), geometry.channels[channel], node.font, node.style, foreground.withAlpha(0.72f), 0.f, false);
					drawLabel("HEX", geometry.hexLabel, node.font, node.style, foreground.withAlpha(0.72f), 0.f, false);
				}
			}
		}
		// Actual TextBox runtime nodes for channel values and HEX. Popup placement is an overlay rect;
		// editing/focus/selection/clipboard are the same code paths used by regular authored TextBoxes.
		for (Node& editor : m_nodes) {
			if (!editor.pickerEditor || !isNodeVisible(static_cast<u32>(&editor - m_nodes.data()))) continue;
			const Rect rect = pickerEditorRect(editor);
			const Color background = color(editor.style, PropId::Bg, Color::fromRGBA8(0x101827FFu));
			const Color foreground = color(editor.style, PropId::Fg, Color::fromRGBA8(0xF2F6FFFFu));
			const f32 radius = editor.style.has(PropId::Radius) ? number(editor.style.get(PropId::Radius), 0, 4.f) : 4.f;
			painter.fillRect(rect, background, CornerRadii::all(radius));
			if (editor.style.has(PropId::Border) && editor.style.get(PropId::Border).count >= 3) {
				const StyleValue& border = editor.style.get(PropId::Border);
				const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
				painter.roundRect(rect, RectStyle{ Color{0,0,0,0}, edge, number(border,0), CornerRadii::all(radius) });
			}
			const f32 fontSize = editor.style.has(PropId::FontSize) ? number(editor.style.get(PropId::FontSize), 0, 12.f) : 12.f;
			if (editor.focused && editor.caret != editor.selectionAnchor && m_text && editor.font.valid()) {
				const std::size_t begin = std::min(editor.caret, editor.selectionAnchor), end = std::max(editor.caret, editor.selectionAnchor);
				const f32 x1 = m_text->layout(editor.font, std::string_view(editor.text).substr(0, begin), fontSize * m_dpiScale).width / m_dpiScale;
				const f32 x2 = m_text->layout(editor.font, std::string_view(editor.text).substr(0, end), fontSize * m_dpiScale).width / m_dpiScale;
				painter.fillRect({ rect.x + 5.f + x1,rect.y + 3.f,std::max(1.f,x2 - x1),rect.h - 6.f }, foreground.withAlpha(0.28f));
			}
			drawLabel(editor.text, rect, editor.font, editor.style, foreground, 5.f, false);
			if (editor.focused && !editor.disabled && !editor.readOnly && m_caretVisible && m_text && editor.font.valid()) {
				const f32 caretX = m_text->layout(editor.font, std::string_view(editor.text).substr(0, std::min(editor.caret, editor.text.size())), fontSize * m_dpiScale).width / m_dpiScale;
				painter.line({ rect.x + 5.f + caretX,rect.y + 5.f }, { rect.x + 5.f + caretX,rect.bottom() - 5.f }, 1.f, foreground);
			}
		}
		const bool popupOpen = m_tooltipVisible && std::any_of(m_nodes.begin(), m_nodes.end(), [](const Node& node) { return node.popupOpen; });
		if (m_tooltipVisible && !popupOpen && m_tooltipOwner < m_nodes.size()) {
			const Node& owner = m_nodes[m_tooltipOwner];
			PropTable tooltipStyle;
			m_styles->resolve(m_tooltipStyleSet, stateBit(State::Resting), m_theme, tooltipStyle);
			const FontHandle selectedTooltipFont = selectFont(tooltipStyle);
			const FontHandle tooltipFont = selectedTooltipFont.valid() ? selectedTooltipFont : owner.font;
			const f32 fontSize = tooltipStyle.has(PropId::FontSize) ? number(tooltipStyle.get(PropId::FontSize), 0, 13.f) : 13.f;
			const f32 maxWidth = std::min(360.f, std::max(80.f, m_lastLayoutWidth - 16.f));
			const TextLayout& measured = m_text->layout(tooltipFont, owner.tooltipText, fontSize * m_dpiScale, (maxWidth - 20.f) * m_dpiScale);
			const f32 padding = tooltipStyle.has(PropId::Pad) ? number(tooltipStyle.get(PropId::Pad), 0, 7.f) : 7.f;
			const f32 width = std::min(maxWidth, measured.width / m_dpiScale + padding * 2.f);
			const f32 height = measured.height / m_dpiScale + padding * 2.f;
			const Rect anchor = m_layout.absRect(owner.layout);
			Rect bubble{ anchor.x, anchor.bottom() + 6.f, width, height };
			if (bubble.bottom() > m_lastLayoutHeight) bubble.y = anchor.y - height - 6.f;
			bubble.x = std::clamp(bubble.x, 4.f, std::max(4.f, m_lastLayoutWidth - width - 4.f));
			bubble.y = std::clamp(bubble.y, 4.f, std::max(4.f, m_lastLayoutHeight - height - 4.f));
			painter.fillRect(bubble, color(tooltipStyle, PropId::Bg, Color::fromRGBA8(0x202A40F5u)),
				CornerRadii::all(tooltipStyle.has(PropId::Radius) ? number(tooltipStyle.get(PropId::Radius), 0, 5.f) : 5.f));
			drawLabel(owner.tooltipText, { bubble.x + padding, bubble.y + padding, bubble.w - padding * 2.f, bubble.h - padding * 2.f },
				tooltipFont, tooltipStyle, Color::fromRGBA8(0xF2F6FFFFu), 0.f, true);
		}
	}

	bool UiRuntime::pointerMove(Vec2 point) {
		m_pointerPosition = point;
		bool changed = dockPointerMove(point);
		if (m_dragScroll < m_nodes.size()) {
			const u32 index = m_dragScroll;
			Rect track;
			const Rect thumb = scrollBarRect(index, m_dragScrollX, &track);
			const Vec2 content = m_layout.contentSize(m_nodes[index].layout);
			const Rect view = m_layout.absRect(m_nodes[index].layout);
			const f32 thumbLength = m_dragScrollX ? thumb.w : thumb.h;
			const f32 trackLength = m_dragScrollX ? track.w : track.h;
			const f32 range = m_dragScrollX ? content.x - view.w : content.y - view.h;
			const f32 travel = std::max(1.f, trackLength - thumbLength);
			const f32 pointer = m_dragScrollX ? point.x : point.y;
			Vec2 offset = m_layout.scrollOffset(m_nodes[index].layout);
			const f32 value = m_dragScrollStart + (pointer - m_dragPointerStart) * range / travel;
			if (m_dragScrollX) offset.x = value; else offset.y = value;
			const bool moved = setScrollOffset(index, offset, false);
			changed |= moved;
			if (moved && m_nodes[index].pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(m_nodes[index].pressHandler));
			return changed;
		}
		const u32 target = hitTestNode(point);
		u32 popupTarget = 0xFFFFFFFFu;
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			const Node& node = m_nodes[index];
			if (!node.popupOpen || (!node.dropdown && !node.colorPicker && !node.datePicker) || !isNodeVisible(index)) continue;
			if (popupBounds(m_layout.absRect(node.layout), node.dropdown, node.options.size(), m_lastLayoutHeight,
				node.datePicker ? 224.f : node.pickerWidth, node.datePicker ? 208.f : node.pickerHeight).contains(point)) {
				popupTarget = index;
				break;
			}
		}
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			const bool visible = isNodeVisible(i);
			const bool editorHover = node.pickerEditor && visible && pickerEditorRect(node).contains(point);
			const bool hover = visible && !node.disabled && (editorHover || (popupTarget == 0xFFFFFFFFu && target == i));
			if (hover != node.hovered) {
				node.hovered = hover;
				if (!node.hovered && !node.focused) node.tooltipDismissed = false;
				changed = true;
				addDamage(node.pickerEditor ? pickerEditorRect(node) : m_layout.absRect(node.layout));
			}
		}
		for (u32 index : m_interactiveNodes) {
			Node& node = m_nodes[index];
			if (node.slider && node.pressed && !node.disabled && isNodeVisible(index)) {
				const f32 before = node.value;
				updateSlider(node, point);
				if (node.value != before) {
					changed = true; addDamage(m_layout.absRect(node.layout));
					if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				}
			}
			if (node.colorPicker && node.pressed && node.popupOpen && !node.disabled && isNodeVisible(index))
				changed |= updateColorPicker(node, point);
		}
		refreshTooltipTarget();
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			if (!node.popupOpen || !node.dropdown) continue;
			const Rect popup = popupBounds(m_layout.absRect(node.layout), true, node.options.size(), m_lastLayoutHeight, node.pickerWidth, node.pickerHeight);
			const i32 over = i == popupTarget && popup.contains(point) && !node.options.empty()
				? std::clamp<i32>(static_cast<i32>((point.y - popup.y) / 28.f), 0, static_cast<i32>(node.options.size()) - 1)
				: -1;
			if (over != node.hoveredOption) { node.hoveredOption = over; changed = true; addDamage(popup); }
		}
		if (m_splitDrag.active) {
			const Rect splitBounds = m_layout.absRect(m_dragSplit);
			const bool horizontal = m_layout.style(m_dragSplit).splitDir == SplitDir::Horizontal;
			m_layout.splitDragUpdate(m_splitDrag, (horizontal ? point.x - splitBounds.x : point.y - splitBounds.y) - m_dragStart);
			changed = true;
			addDamage(splitBounds);
		}
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			if (!(node.radioGroup || node.segmented || node.rating || node.breadcrumb) || node.disabled || !isNodeVisible(i) || node.options.empty()) continue;
			const Rect rect = m_layout.absRect(node.layout);
			i32 over = -1;
			if (rect.contains(point)) {
				if (node.radioGroup) {
					const f32 rowHeight = rect.h / f32(node.options.size());
					over = std::clamp<i32>(static_cast<i32>((point.y - rect.y) / std::max(1.f, rowHeight)), 0, static_cast<i32>(node.options.size()) - 1);
				}
				else if (node.segmented) {
					over = std::clamp<i32>(static_cast<i32>((point.x - rect.x) / std::max(1.f, rect.w / f32(node.options.size()))), 0, static_cast<i32>(node.options.size()) - 1);
				}
				else if (node.rating) {
					const i32 pips = static_cast<i32>(std::clamp(node.maxValue, 1.f, 10.f));
					over = std::clamp<i32>(static_cast<i32>((point.x - rect.x) / std::max(1.f, rect.w / f32(pips))), 0, pips - 1);
				}
				else {   // breadcrumb: segments have per-label widths
					const f32 fontSize = node.style.has(PropId::FontSize) ? number(node.style.get(PropId::FontSize), 0, 14) : 14.f;
					f32 x = rect.x;
					for (u32 segment = 0; segment < node.options.size(); ++segment) {
						const f32 width = m_text && node.font.valid() ? m_text->layout(node.font, node.options[segment], fontSize, 0.f).width / m_dpiScale : 0.f;
						if (point.x < x + width + 10.f) { over = static_cast<i32>(segment); break; }
						x += width + 22.f;
					}
				}
			}
			if (over != node.hoveredOption) { node.hoveredOption = over; changed = true; addDamage(rect); }
		}
		return changed;
	}

	CursorShape UiRuntime::cursorAt(Vec2 point) const {
		for (auto it = m_dockHosts.rbegin(); it != m_dockHosts.rend(); ++it) {
			const DockHostState& host = *it;
			if (host.node >= m_nodes.size() || !isNodeVisible(host.node)) continue;
			const Rect rect = m_layout.absRect(m_nodes[host.node].layout);
			const LayoutStyle& style = m_layout.style(m_nodes[host.node].layout);
			const Vec2 origin{ rect.x + style.border.l + style.padding.l, rect.y + style.border.t + style.padding.t };
			const Vec2 local{ point.x - origin.x, point.y - origin.y };
			bool horizontal = true;
			if (host.tree.splitAt(local, &horizontal) != 0xFFFFFFFFu)
				return horizontal ? CursorShape::ResizeEW : CursorShape::ResizeNS;
			const u32 panel = host.tree.panelAt(local);
			if (panel < m_nodes.size() && host.tree.panelBounds(panel).contains(local) &&
				local.y < host.tree.panelBounds(panel).y + m_nodes[panel].dockHeaderHeight) return CursorShape::Hand;
		}
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			if (m_nodes[index].scrollView && pointInsideScrollAncestors(index, point)) {
				if (scrollBarRect(index, true).contains(point)) return CursorShape::ResizeEW;
				if (scrollBarRect(index, false).contains(point)) return CursorShape::ResizeNS;
			}
		}
		const u32 target = hitTestNode(point);
		if (target < m_nodes.size()) {
			const Node& node = m_nodes[target];
			// Disabled controls never advertise interaction; the plain arrow keeps that honest.
			if (node.disabled) return CursorShape::Arrow;
			// Split gutters resize along the drag axis, matching the drag handling in pointerDown().
			if (node.split) {
				const Rect bounds = m_layout.absRect(node.layout);
				const u32 gutters = m_layout.gutterCount(node.layout);
				for (u32 gutter = 0; gutter < gutters; ++gutter) {
					Rect rect = m_layout.gutterRect(node.layout, gutter);
					rect.x += bounds.x; rect.y += bounds.y;
					if (rect.contains(point))
						return m_layout.style(node.layout).splitDir == SplitDir::Horizontal ? CursorShape::ResizeEW : CursorShape::ResizeNS;
				}
			}
			if (node.slider) return CursorShape::ResizeEW;
			if ((node.textBox || node.textArea) && !node.readOnly) {
				if (node.searchField && !node.text.empty() && searchClearRect(node).contains(point)) return CursorShape::Hand;
				return CursorShape::IBeam;
			}
			const bool clickable = node.button || node.checkItem || node.checkBox || node.switchToggle || node.toggleButton ||
				node.dropdown || node.colorPicker || node.datePicker || node.segmented || node.rating || node.breadcrumb ||
				node.stepper || node.keybindField || node.radioGroup || node.tabs;
			return clickable ? CursorShape::Hand : CursorShape::Arrow;
		}
		return CursorShape::Arrow;
	}

	bool UiRuntime::scrollWheel(Vec2 point, Vec2 delta) {
		if (delta.x == 0.f && delta.y == 0.f) return false;
		const u32 target = hitTestNode(point);
		for (u32 index = target; index != 0xFFFFFFFFu && index < m_nodes.size(); index = m_nodes[index].parentIndex) {
			Node& node = m_nodes[index];
			if (!node.scrollView || node.disabled) continue;
			const LayoutStyle& style = m_layout.style(node.layout);
			Vec2 before = m_layout.scrollOffset(node.layout), next = before;
			if (style.scrollY && delta.y != 0.f) next.y -= delta.y * node.scrollSpeed;
			if (style.scrollX && delta.x != 0.f) next.x -= delta.x * node.scrollSpeed;
			if (next != before && setScrollOffset(index, next, node.scrollSmooth)) {
				if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				return true;
			}
		}
		return false;
	}

	bool UiRuntime::pointerDown(Vec2 point) {
		m_pointerPosition = point;
		bool changed = false;
		bool popupCoversPoint = false;
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			const Node& node = m_nodes[i];
			if (node.popupOpen && (node.dropdown || node.colorPicker || node.datePicker) && isNodeVisible(i) &&
				popupBounds(m_layout.absRect(node.layout), node.dropdown, node.options.size(), m_lastLayoutHeight,
					node.datePicker ? 224.f : node.pickerWidth, node.datePicker ? 208.f : node.pickerHeight).contains(point)) {
				popupCoversPoint = true; break;
			}
		}
		if (!popupCoversPoint && dockPointerDown(point)) return true;
		for (std::size_t i = popupCoversPoint ? 0 : m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			if (!m_nodes[index].scrollView || m_nodes[index].disabled || !pointInsideScrollAncestors(index, point)) continue;
			for (bool horizontal : { false, true }) {
				Rect track;
				const Rect thumb = scrollBarRect(index, horizontal, &track);
				if (!thumb.contains(point)) continue;
				m_dragScroll = index; m_dragScrollX = horizontal;
				m_nodes[index].pressed = true;
				m_dragPointerStart = horizontal ? point.x : point.y;
				const Vec2 offset = m_layout.scrollOffset(m_nodes[index].layout);
				m_dragScrollStart = horizontal ? offset.x : offset.y;
				addDamage(m_layout.absRect(m_nodes[index].layout));
				return true;
			}
		}
		// Any click dismisses an already visible tip. The dismissed owner stays suppressed
		// until it loses both hover and focus, preventing the tip from reopening over a popup.
		if (m_tooltipOwner < m_nodes.size() && m_nodes[m_tooltipOwner].tooltipHideOnClick) {
			Node& owner = m_nodes[m_tooltipOwner];
			owner.tooltipDismissed = true;
			addDamage({ 0, 0, m_lastLayoutWidth, m_lastLayoutHeight });
			m_tooltipOwner = 0xFFFFFFFFu;
			m_tooltipVisible = false;
			m_tooltipDeadline = m_tooltipHideDeadline = {};
			changed = true;
		}
		// Popup surfaces are painted above the tree; the last painted popup gets the event.
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			Node& node = m_nodes[index];
			if (!node.popupOpen || (!node.dropdown && !node.colorPicker && !node.datePicker) || !isNodeVisible(index)) continue;
			const Rect anchor = m_layout.absRect(node.layout);
			const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight,
				node.datePicker ? 224.f : node.pickerWidth, node.datePicker ? 208.f : node.pickerHeight);
			if (!popup.contains(point)) continue;
			if (node.datePicker) {
				const f32 localY = point.y - popup.y;
				if (localY < 26.f) {
					// Header arrows step the viewed month; the selection follows so it stays visible.
					if (point.x < popup.x + 30.f) stepViewMonth(node, -1);
					else if (point.x > popup.right() - 30.f) stepViewMonth(node, 1);
				}
				else if (localY >= 44.f) {
					const f32 cellWidth = popup.w / 7.f;
					const u32 cell = std::min<u32>(static_cast<u32>((localY - 46.f) / 22.f), 5u) * 7u
						+ std::min<u32>(static_cast<u32>((point.x - popup.x) / cellWidth), 6u);
					const i32 day = static_cast<i32>(cell) - static_cast<i32>(weekdayMonday0(node.viewYear, node.viewMonth, 1u)) + 1;
					const i32 monthDays = static_cast<i32>(daysInMonth(node.viewYear, node.viewMonth));
					if (day >= 1 && day <= monthDays) {
						node.dateYear = node.viewYear;
						node.dateMonth = node.viewMonth;
						node.dateDay = static_cast<u32>(day);
						node.text = formatDateText(node.dateYear, node.dateMonth, node.dateDay);
						m_layout.markContentChanged(node.layout);
						node.popupOpen = false;
						if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
					}
				}
				addDamage(anchor); addDamage(popup);
				return true;
			}
			if (node.dropdown) {
				if (node.options.empty()) return true;
				const u32 selected = std::min<u32>(static_cast<u32>((point.y - popup.y) / 28.f), static_cast<u32>(node.options.size() - 1));
				node.selectedOption = selected;
				node.text = node.options[selected];
				node.popupOpen = false;
				m_layout.markContentChanged(node.layout);
				addDamage(anchor); addDamage(popup);
				if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
			}
			else {
				const PickerGeometry geometry = pickerGeometry(node);
				if (geometry.format.contains(point)) {
					if (node.pickerShowFormatSwitcher && !node.pickerFormats.empty()) {
						auto format = std::find(node.pickerFormats.begin(), node.pickerFormats.end(), node.pickerFormat);
						const std::size_t next = format == node.pickerFormats.end() ? 0 : (static_cast<std::size_t>(format - node.pickerFormats.begin()) + 1) % node.pickerFormats.size();
						node.pickerFormat = node.pickerFormats[next]; node.text = colorText(node.pickerColor, node.pickerFormat, node.pickerAlpha, node.pickerPrecision);
						syncPickerEditors(index); addDamage(popup); addDamage(anchor);
					}
				}
				if (!geometry.format.contains(point) && node.pickerShowInputs) {
					u32 editorIndex = 0xFFFFFFFFu;
					for (u32 candidate = 0; candidate < m_nodes.size(); ++candidate) {
						const Node& editor = m_nodes[candidate];
						if (editor.pickerEditor && editor.pickerOwner == index && pickerEditorRect(editor).contains(point)) {
							editorIndex = candidate; break;
						}
					}
					if (editorIndex != 0xFFFFFFFFu) {
						Node& editor = m_nodes[editorIndex];
						for (u32 candidate : m_interactiveNodes) {
							Node& field = m_nodes[candidate];
							const bool focus = candidate == editorIndex && !field.disabled;
							if (field.focused != focus) addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout));
							field.focused = focus;
							if (focus) {
								const Rect editorRect = pickerEditorRect(field);
								const f32 fontSize = field.style.has(PropId::FontSize) ? number(field.style.get(PropId::FontSize), 0, 13.f) : 13.f;
								f32 bestDistance = std::numeric_limits<f32>::max();
								for (std::size_t byte = 0; byte <= field.text.size();) {
									const f32 width = m_text && field.font.valid() ? m_text->layout(field.font, std::string_view(field.text).substr(0, byte), fontSize * m_dpiScale).width / m_dpiScale : 0.f;
									const f32 distance = std::fabs((point.x - editorRect.x - 5.f) - width);
									if (distance < bestDistance) { bestDistance = distance; field.caret = byte; }
									if (byte == field.text.size()) break;
									byte = nextUtf8(field.text, byte);
								}
								field.selectionAnchor = field.caret;
							}
						}
						node.pressed = false;
						refreshTooltipTarget();
						addDamage(pickerEditorRect(editor));
						return true;
					}
				}
				if (geometry.saturationValue.contains(point) || geometry.hue.contains(point) || geometry.alpha.contains(point)) {
					node.pressed = true;
					(void)updateColorPicker(node, point);
				}
			}
			return true;
		}
		// An outside click dismisses the popup, then may activate the newly exposed target.
		for (Node& node : m_nodes) if (node.popupOpen && (node.dropdown || node.colorPicker || node.datePicker)) {
			const Rect anchor = m_layout.absRect(node.layout);
			const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight, node.pickerWidth, node.pickerHeight);
			node.popupOpen = false;
			node.pressed = false;
			node.hoveredOption = -1;
			changed = true;
			addDamage(anchor); addDamage(popup);
		}
		// Tab headers are a dedicated layer above normal widget content.
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			Node& tabs = m_nodes[index];
			if (!tabs.tabs || tabs.disabled || !isNodeVisible(index)) continue;
			const Rect rect = m_layout.absRect(tabs.layout);
			if (!rect.contains(point) || point.y >= rect.y + 34.f) continue;
			const u32 count = m_layout.childCount(tabs.layout);
			if (!count || rect.w <= 0.f) return true;
			const u32 ordinal = std::min<u32>(static_cast<u32>((point.x - rect.x) / rect.w * count), count - 1);
			if (tabs.selectedTab != ordinal) {
				tabs.selectedTab = ordinal;
				addDamage(rect);
				changed = true;
				for (u32 j = 0; j < m_nodes.size(); ++j) if (m_nodes[j].focused && !isNodeVisible(j)) m_nodes[j].focused = false;
				if (tabs.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(tabs.pressHandler));
			}
			for (u32 j = 0; j < m_nodes.size(); ++j) {
				const bool focus = j == index;
				if (m_nodes[j].focused != focus) addDamage(m_layout.absRect(m_nodes[j].layout));
				m_nodes[j].focused = focus;
			}
			refreshTooltipTarget();
			return true;
		}

		const u32 targetIndex = hitTestNode(point);
		Node* target = targetIndex < m_nodes.size() ? &m_nodes[targetIndex] : nullptr;
		if (target && target->tooltipHideOnClick) target->tooltipDismissed = true;
		for (u32 index : m_interactiveNodes) {
			Node& node = m_nodes[index];
			const bool focus = target == &node && !node.disabled;
			changed |= node.focused != focus;
			if (node.focused != focus) addDamage(m_layout.absRect(node.layout));
			if (!focus && node.focused && node.numberField && !node.disabled) commitNumberText(node);
			node.focused = focus;
			if (focus && (node.textBox || node.textArea)) resetCaretBlink();
			if (!node.hovered && !node.focused) node.tooltipDismissed = false;
			if ((node.textBox || node.textArea) && focus) node.caret = node.text.size();
			if ((node.textBox || node.textArea) && focus) node.selectionAnchor = node.caret;
		}
		refreshTooltipTarget();
		if (target && target->searchField && !target->text.empty() && !target->disabled && !target->readOnly) {
			const Rect clearRect = searchClearRect(*target);
			if (clearRect.contains(point)) {
				target->text.clear();
				target->caret = target->selectionAnchor = 0;
				m_layout.markContentChanged(target->layout);
				addDamage(m_layout.absRect(target->layout));
				if (target->pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
				return true;
			}
		}
		if (target && target->radioGroup && !target->disabled && !target->options.empty()) {
			const Rect rect = m_layout.absRect(target->layout);
			const f32 rowHeight = rect.h / f32(target->options.size());
			const u32 row = std::clamp<u32>(static_cast<u32>((point.y - rect.y) / std::max(1.f, rowHeight)), 0, static_cast<u32>(target->options.size() - 1));
			if (row != target->selectedOption) {
				target->selectedOption = row;
				changed = true;
				addDamage(rect);
				if (target->pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
			}
			return true;
		}
		if (target && !target->disabled) {
			const Rect rect = m_layout.absRect(target->layout);
			if (target->segmented && !target->options.empty()) {
				const u32 segment = std::clamp<u32>(static_cast<u32>((point.x - rect.x) / std::max(1.f, rect.w / f32(target->options.size()))), 0, static_cast<u32>(target->options.size()) - 1);
				if (selectSegment(*target, segment, false) && target->pressHandler.v && m_eventFn)
					m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
				return true;
			}
			if (target->breadcrumb && !target->options.empty()) {
				const f32 fontSize = target->style.has(PropId::FontSize) ? number(target->style.get(PropId::FontSize), 0, 14) : 14.f;
				f32 x = rect.x;
				u32 chosen = 0xFFFFFFFFu;
				for (u32 segment = 0; segment < target->options.size(); ++segment) {
					const f32 width = m_text && target->font.valid() ? m_text->layout(target->font, target->options[segment], fontSize, 0.f).width / m_dpiScale : 0.f;
					if (point.x < x + width + 10.f) { chosen = segment; break; }
					x += width + 22.f;
				}
				if (chosen != 0xFFFFFFFFu && selectSegment(*target, chosen, false) && target->pressHandler.v && m_eventFn)
					m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
				return true;
			}
			if (target->rating) {
				const i32 pips = static_cast<i32>(std::clamp(target->maxValue, 1.f, 10.f));
				const i32 pip = std::clamp<i32>(static_cast<i32>((point.x - rect.x) / std::max(1.f, rect.w / f32(pips))), 0, pips - 1);
				const f32 next = f32(pip + 1) == target->value ? 0.f : f32(pip + 1);   // clicking the last lit pip clears
				if (next != target->value) {
					target->value = next;
					addDamage(rect);
					if (target->pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
				}
				return true;
			}
			if ((target->stepper || target->numberField) && rect.w > 24.f) {
				const f32 button = std::min(20.f, rect.w * 0.22f);
				if (point.x >= rect.right() - button * 2.f) {
					const bool minus = point.x < rect.right() - button;
					target->stepperZone = minus ? 1u : 2u;
					applyNumberStep(*target, minus ? -1.f : 1.f);
					addDamage(rect);
					return true;
				}
			}
		}

		// Split gutters are painted by the Split node before its children. They only
		// receive input when no later-painted node covers that point.
		for (u32 index : m_splitNodes) {
			Node& node = m_nodes[index];
			if (target != &node) continue;
			const u32 gutters = m_layout.gutterCount(node.layout);
			const Rect splitBounds = m_layout.absRect(node.layout);
			for (u32 i = 0; i < gutters; ++i) {
				Rect gutter = m_layout.gutterRect(node.layout, i);
				gutter.x += splitBounds.x; gutter.y += splitBounds.y;
				if (!gutter.contains(point)) continue;
				m_dragSplit = node.layout;
				m_splitDrag = m_layout.splitDragBegin(node.layout, i);
				m_dragStart = m_layout.style(node.layout).splitDir == SplitDir::Horizontal ? point.x - splitBounds.x : point.y - splitBounds.y;
				addDamage(splitBounds);
				return true;
			}
		}
		if (target && !target->disabled) {
			const bool shouldPress = target->button || target->checkItem || target->checkBox || target->slider ||
				target->dropdown || target->colorPicker || target->textBox || target->textArea || target->switchToggle || target->toggleButton || target->datePicker;
			for (Node& node : m_nodes) if (&node != target && node.pressed) {
				node.pressed = false;
				addDamage(m_layout.absRect(node.layout));
				changed = true;
			}
			if (target->pressed != shouldPress) changed = true;
			target->pressed = shouldPress;
			if (target->pressed) {
				changed = true;
				addDamage(m_layout.absRect(target->layout));
				if (target->dropdown || target->colorPicker || target->datePicker) {
					for (Node& node : m_nodes) if (&node != target && node.popupOpen) node.popupOpen = false;
					target->popupOpen = true;
					if (target->datePicker) { target->viewYear = target->dateYear; target->viewMonth = target->dateMonth; }
					addDamage(popupBounds(m_layout.absRect(target->layout), target->dropdown, target->options.size(), m_lastLayoutHeight,
						target->datePicker ? 224.f : target->pickerWidth, target->datePicker ? 208.f : target->pickerHeight));
				}
				if (target->slider) {
					const f32 before = target->value;
					updateSlider(*target, point);
					if (before != target->value) {
						addDamage(m_layout.absRect(target->layout));
						if (target->pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(target->pressHandler));
					}
				}
			}
		}
		return changed;
	}

	bool UiRuntime::pointerUp(Vec2 point) {
		if (m_dockCandidate != 0xFFFFFFFFu || m_dockSplitDragging) return dockPointerUp(point);
		if (m_dragScroll < m_nodes.size()) {
			const u32 index = m_dragScroll;
			m_dragScroll = 0xFFFFFFFFu;
			m_nodes[index].pressed = false;
			addDamage(m_layout.absRect(m_nodes[index].layout));
			return true;
		}
		Node* activated = nullptr;
		const u32 frontmost = hitTestNode(point);
		for (auto it = m_interactiveNodes.rbegin(); it != m_interactiveNodes.rend(); ++it) {
			Node& node = m_nodes[*it];
			if (*it == frontmost && node.pressed && !node.disabled && isNodeVisible(*it) && m_layout.absRect(node.layout).contains(point)) {
				activated = &node;
				break;
			}
		}
		bool changed = false;
		for (u32 index : m_interactiveNodes) {
			Node& node = m_nodes[index];
			changed |= node.pressed;
			if (node.pressed) addDamage(m_layout.absRect(node.layout));
			node.pressed = false;
			if (node.stepperZone) { node.stepperZone = 0; addDamage(m_layout.absRect(node.layout)); changed = true; }
		}
		if (activated && (activated->checkItem || activated->checkBox || activated->switchToggle || activated->toggleButton)) {
			activated->checked = !activated->checked;
			changed = true;
			addDamage(m_layout.absRect(activated->layout));
		}
		if (activated && (activated->button || activated->checkItem || activated->checkBox || activated->switchToggle || activated->toggleButton) &&
			activated->pressHandler.v && m_eventFn)
			m_eventFn(m_eventContext, m_strings->view(activated->pressHandler));
		m_splitDrag = {};
		m_dragSplit = {};
		return changed || activated != nullptr;
	}

	bool UiRuntime::keyDown(Key key) { return keyDown(key, 0, nullptr); }

	bool UiRuntime::keyDown(Key key, u8 mods, Platform* platform) {
		if (key == Key::Escape && (m_dockCandidate != 0xFFFFFFFFu || m_dockSplitDragging)) {
			if (m_dockDragHost < m_dockHosts.size()) addDockDamage(m_dockHosts[m_dockDragHost]);
			m_dockCandidate = m_dockDragPanel = m_dockDragHost = m_dockDragSplit = 0xFFFFFFFFu;
			m_dockDragging = m_dockSplitDragging = false;
			m_dockPreview = {};
			return true;
		}
		if (key == Key::F3) {
			m_fpsEnabled = !m_fpsEnabled;
			m_fpsElapsed = 0;
			m_fpsFrames = 0;
			m_fpsLastFrame = {};
			m_fpsDeadline = m_fpsEnabled ? std::chrono::steady_clock::now() + kFpsRefreshInterval
				: std::chrono::steady_clock::time_point{};
			for (const Node& node : m_nodes) if (node.fpsCounter) addDamage(m_layout.absRect(node.layout));
			return true;
		}
		const bool control = (mods & static_cast<u8>(Mod::Ctrl)) != 0;
		for (u32 index : m_interactiveNodes) {
			Node& field = m_nodes[index];
			if (!(field.textBox || field.textArea) || !field.focused || field.disabled) continue;
			const std::size_t selectionBegin = std::min(field.caret, field.selectionAnchor);
			const std::size_t selectionEnd = std::max(field.caret, field.selectionAnchor);
			const bool hasSelection = selectionBegin != selectionEnd;
			if (control && key == Key::A) {
				field.selectionAnchor = 0;
				field.caret = field.text.size();
				addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout));
				return true;
			}
			if (control && (key == static_cast<Key>('C') || key == static_cast<Key>('X'))) {
				if (hasSelection && platform) platform->setClipboardText(std::string_view(field.text).substr(selectionBegin, selectionEnd - selectionBegin));
				if (key == static_cast<Key>('C') || field.readOnly || !hasSelection) return hasSelection;
			}
			if (control && key == static_cast<Key>('V') && platform && !field.readOnly) return textInput(platform->clipboardText());
			if (control && key != static_cast<Key>('X')) break;
			const std::size_t oldCaret = field.caret;
			const bool shift = (mods & static_cast<u8>(Mod::Shift)) != 0;
			bool edited = false;
			if (!field.readOnly && ((key == Key::Backspace || key == Key::Delete) || (control && key == static_cast<Key>('X')))) {
				if (hasSelection) {
					field.text.erase(selectionBegin, selectionEnd - selectionBegin);
					field.caret = field.selectionAnchor = selectionBegin;
					edited = true;
				}
				else if (key == Key::Backspace && field.caret) {
					const std::size_t start = previousUtf8(field.text, field.caret);
					field.text.erase(start, field.caret - start); field.caret = field.selectionAnchor = start; edited = true;
				}
				else if ((key == Key::Delete || (control && key == static_cast<Key>('X'))) && field.caret < field.text.size()) {
					const std::size_t end = nextUtf8(field.text, field.caret);
					field.text.erase(field.caret, end - field.caret); field.selectionAnchor = field.caret; edited = true;
				}
			}
			else if (key == Key::Left && field.caret) {
				field.caret = previousUtf8(field.text, field.caret);
				if (!shift) field.selectionAnchor = field.caret;
			}
			else if (key == Key::Right && field.caret < field.text.size()) {
				field.caret = nextUtf8(field.text, field.caret);
				if (!shift) field.selectionAnchor = field.caret;
			}
			else if (key == Key::Home) {
				const std::size_t lineBreak = field.textArea && field.caret ? field.text.rfind('\n', field.caret - 1) : std::string::npos;
				field.caret = lineBreak == std::string::npos ? 0 : lineBreak + 1;
				if (!shift) field.selectionAnchor = field.caret;
			}
			else if (key == Key::End) {
				const std::size_t end = field.text.find('\n', field.caret);
				field.caret = end == std::string::npos ? field.text.size() : end;
				if (!shift) field.selectionAnchor = field.caret;
			}
			else if (!field.readOnly && field.textArea && key == Key::Tab) {
				const u32 currentCount = codepointCount(field.text);
				const u32 selectedCount = hasSelection
					? codepointCount(std::string_view(field.text).substr(selectionBegin, selectionEnd - selectionBegin)) : 0u;
				const u32 room = field.maxLength ? field.maxLength - std::min(field.maxLength, currentCount - selectedCount) : field.tabSize;
				const u32 spaces = std::min(field.tabSize, room);
				if (spaces) {
					if (hasSelection) field.text.erase(selectionBegin, selectionEnd - selectionBegin);
					field.text.insert(selectionBegin, spaces, ' ');
					field.caret = field.selectionAnchor = selectionBegin + spaces;
					edited = true;
				}
			}
			else if (!field.readOnly && field.textArea && key == Key::Enter) {
				const std::size_t countAfterSelection = codepointCount(field.text) - (hasSelection ? codepointCount(std::string_view(field.text).substr(selectionBegin, selectionEnd - selectionBegin)) : 0u);
				if (!field.maxLength || countAfterSelection < field.maxLength) {
					if (hasSelection) field.text.erase(selectionBegin, selectionEnd - selectionBegin);
					field.text.insert(selectionBegin, 1, '\n');
					field.caret = field.selectionAnchor = selectionBegin + 1;
					edited = true;
				}
			}
			else break;
			if (edited) {
				field.text = truncateUtf8(field.text, field.maxLength);
				field.caret = std::min(field.caret, field.text.size());
				field.selectionAnchor = std::min(field.selectionAnchor, field.text.size());
				m_layout.markContentChanged(field.layout);
				if (field.pickerEditor) updatePickerFromEditor(field);
				else if (field.numberField) commitNumberText(field);
				else if (field.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(field.pressHandler));
			}
			if (edited || oldCaret != field.caret) { resetCaretBlink(); addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout)); return true; }
			if (key == Key::Backspace || key == Key::Delete) return false;
			break;
		}
		if (key == Key::Tab) {
			std::size_t focusableCount = 0, current = static_cast<std::size_t>(-1);
			for (u32 index : m_interactiveNodes) if (!m_nodes[index].disabled && isNodeVisible(index)) {
				if (m_nodes[index].focused) current = focusableCount;
				++focusableCount;
			}
			if (!focusableCount) return false;
			const std::size_t next = current == static_cast<std::size_t>(-1) ? 0 : (current + 1) % focusableCount;
			std::size_t ordinal = 0;
			for (u32 index : m_interactiveNodes) if (!m_nodes[index].disabled && isNodeVisible(index)) {
				Node& node = m_nodes[index]; const bool focus = ordinal++ == next;
				if (node.focused != focus) addDamage(m_layout.absRect(node.layout));
				node.focused = focus;
				if (focus) ensureVisible(index, false);
			}
			refreshTooltipTarget();
			return true;
		}
		// A focused KeybindField captures the next non-modifier key combination. Tab stays reserved
		// for focus traversal and F3 stays the global FPS toggle, so neither can be bound.
		for (u32 index : m_interactiveNodes) {
			Node& field = m_nodes[index];
			if (!field.keybindField || !field.focused || field.disabled) continue;
			if (key == Key::F3 || key == Key::Tab) break;
			const u16 code = static_cast<u16>(key);
			if (code >= 0xE060 && code <= 0xE067) return false;   // bare modifier press: keep waiting
			if (key == Key::Escape) {
				if (field.text.empty()) return false;
				field.text.clear();
			}
			else {
				field.text = keyLabel(key, mods);
			}
			m_layout.markContentChanged(field.layout);
			addDamage(m_layout.absRect(field.layout));
			if (field.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(field.pressHandler));
			return true;
		}
		if (key == Key::Left || key == Key::Right || key == Key::Up || key == Key::Down || key == Key::PageUp || key == Key::PageDown || key == Key::Home || key == Key::End) {
			for (u32 focused : m_interactiveNodes) {
				const Node& focusedControl = m_nodes[focused];
				if (!focusedControl.focused || focusedControl.disabled || focusedControl.textBox || focusedControl.textArea || focusedControl.numberField || focusedControl.searchField || focusedControl.keybindField) continue;
				if ((key == Key::Left || key == Key::Right || key == Key::Up || key == Key::Down) && !focusedControl.scrollView) continue;
				for (u32 parent = focusedControl.scrollView ? focused : focusedControl.parentIndex; parent != 0xFFFFFFFFu && parent < m_nodes.size(); parent = m_nodes[parent].parentIndex) {
					Node& view = m_nodes[parent]; if (!view.scrollView) continue;
					const Rect rect = m_layout.rect(view.layout);
					Vec2 offset = m_layout.scrollOffset(view.layout);
					if (key == Key::Home) offset.y = 0.f;
					else if (key == Key::End) offset.y = m_layout.contentSize(view.layout).y;
					else if (key == Key::PageUp) offset.y -= rect.h * 0.9f;
					else if (key == Key::PageDown) offset.y += rect.h * 0.9f;
					else if (key == Key::Up) offset.y -= view.scrollSpeed;
					else if (key == Key::Down) offset.y += view.scrollSpeed;
					else if (key == Key::Left) offset.x -= view.scrollSpeed;
					else if (key == Key::Right) offset.x += view.scrollSpeed;
					if (setScrollOffset(parent, offset, view.scrollSmooth)) return true;
					break;
				}
			}
		}
		if (key == Key::Left || key == Key::Right || key == Key::Up || key == Key::Down || key == Key::PageUp || key == Key::PageDown) {
			for (u32 index : m_interactiveNodes) {
				Node& node = m_nodes[index];
				if (!node.datePicker || !node.focused || node.disabled || !node.popupOpen) continue;
				i32 delta = 0;
				if (key == Key::Left) delta = -1;
				else if (key == Key::Right) delta = 1;
				else if (key == Key::Up) delta = -7;
				else if (key == Key::Down) delta = 7;
				else if (key == Key::PageUp) delta = -30;
				else delta = 30;
				shiftDate(node.dateYear, node.dateMonth, node.dateDay, delta);
				node.viewYear = node.dateYear;
				node.viewMonth = node.dateMonth;
				node.text = formatDateText(node.dateYear, node.dateMonth, node.dateDay);
				m_layout.markContentChanged(node.layout);
				addDamage(m_layout.absRect(node.layout));
				addDamage(popupBounds(m_layout.absRect(node.layout), false, 0, m_lastLayoutHeight, 224.f, 208.f));
				if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				return true;
			}
		}
		if (key == Key::Left || key == Key::Right) {
			for (u32 index : m_interactiveNodes) {
				Node& node = m_nodes[index];
				if (node.disabled || !node.focused) continue;
				if (node.segmented && node.options.size() > 1) {
					const i32 count = static_cast<i32>(node.options.size());
					const i32 direction = key == Key::Right ? 1 : -1;
					const i32 next = (static_cast<i32>(node.selectedOption) + count + direction) % count;
					selectSegment(node, static_cast<u32>(next), true);
					return true;
				}
				if (node.rating) {
					const f32 step = key == Key::Right ? 1.f : -1.f;
					const f32 next = std::clamp(node.value + step, node.minValue, node.maxValue);
					if (next == node.value) return false;
					node.value = next;
					addDamage(m_layout.absRect(node.layout));
					if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
					return true;
				}
			}
		}
		if (key == Key::Left || key == Key::Right) {
			for (Node& tabs : m_nodes) if (tabs.tabs && tabs.focused && !tabs.disabled) {
				const u32 count = m_layout.childCount(tabs.layout);
				if (count < 2) return false;
				const u32 next = key == Key::Right ? (tabs.selectedTab + 1) % count : (tabs.selectedTab + count - 1) % count;
				if (next == tabs.selectedTab) return false;
				tabs.selectedTab = next; addDamage(m_layout.absRect(tabs.layout));
				if (tabs.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(tabs.pressHandler));
				refreshTooltipTarget(); return true;
			}
		}
		if (key == Key::Left || key == Key::Right) {
			for (u32 index : m_interactiveNodes) {
				Node& node = m_nodes[index];
				if (!node.slider || !node.focused || node.disabled || node.maxValue <= node.minValue) continue;
				const f32 direction = key == Key::Right ? 1.f : -1.f;
				const f32 step = (node.maxValue - node.minValue) * 0.05f;
				const f32 before = node.value;
				node.value = std::clamp(node.value + direction * step, node.minValue, node.maxValue);
				if (node.value == before) return false;
				addDamage(m_layout.absRect(node.layout));
				if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				return true;
			}
		}
		if (key == Key::Escape) {
			for (u32 index : m_interactiveNodes) {
				Node& field = m_nodes[index];
				if (field.searchField && field.focused && !field.disabled && !field.text.empty()) {
					field.text.clear();
					field.caret = field.selectionAnchor = 0;
					m_layout.markContentChanged(field.layout);
					addDamage(m_layout.absRect(field.layout));
					if (field.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(field.pressHandler));
					return true;
				}
			}
			for (Node& node : m_nodes) if (node.popupOpen) { node.popupOpen = false; addDamage(m_layout.absRect(node.layout)); return true; }
			return false;
		}
		if (key == Key::Down || key == Key::Up) {
			for (Node& node : m_nodes) if (node.dropdown && node.focused && !node.options.empty()) {
				const i32 direction = key == Key::Down ? 1 : -1;
				node.selectedOption = static_cast<u32>(std::clamp<i32>(static_cast<i32>(node.selectedOption) + direction, 0, static_cast<i32>(node.options.size()) - 1));
				node.text = node.options[node.selectedOption]; m_layout.markContentChanged(node.layout); addDamage(m_layout.absRect(node.layout));
				if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				return true;
			}
		}
		if (key == Key::Down || key == Key::Up) {
			for (u32 index : m_interactiveNodes) {
				Node& node = m_nodes[index];
				if (node.radioGroup && node.focused && !node.disabled && !node.options.empty()) {
					const i32 direction = key == Key::Down ? 1 : -1;
					const u32 next = static_cast<u32>(std::clamp<i32>(static_cast<i32>(node.selectedOption) + direction, 0, static_cast<i32>(node.options.size()) - 1));
					if (next == node.selectedOption) return false;
					node.selectedOption = next;
					addDamage(m_layout.absRect(node.layout));
					if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
					return true;
				}
				if ((node.numberField || node.stepper) && node.focused && !node.disabled) {
					applyNumberStep(node, key == Key::Up ? 1.f : -1.f);
					addDamage(m_layout.absRect(node.layout));
					return true;
				}
			}
		}
		if (key != Key::Enter && key != Key::Space) return false;
		for (u32 index : m_interactiveNodes) {
			Node& node = m_nodes[index];
			if (!node.focused || node.disabled) continue;
			if (node.checkItem || node.checkBox || node.switchToggle || node.toggleButton) node.checked = !node.checked;
			if (node.dropdown || node.colorPicker) node.popupOpen = !node.popupOpen;
			addDamage(m_layout.absRect(node.layout));
			if (!node.dropdown && !node.colorPicker && node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
			return true;
		}
		return false;
	}

	bool UiRuntime::textInput(u32 codepoint) {
		// Control characters are editing/navigation commands, never printable glyphs. Tab is
		// handled by keyDown (which inserts the TextArea's configured indentation).
		if (codepoint < 0x20u || codepoint == 0x7Fu) return false;
		for (u32 index : m_interactiveNodes) {
			Node& field = m_nodes[index];
			if (!(field.textBox || field.textArea) || !field.focused || field.disabled || field.readOnly || !codepoint || codepoint == '\r' || codepoint == '\n') continue;
			if (field.numberField) {
				const bool numeric = (codepoint >= '0' && codepoint <= '9') || codepoint == '.' || codepoint == '-' ||
					codepoint == '+' || codepoint == ' ' || codepoint == 'e' || codepoint == 'E';
				if (!numeric) return false;
			}
			const std::size_t begin = std::min(field.caret, field.selectionAnchor);
			const std::size_t end = std::max(field.caret, field.selectionAnchor);
			const u32 selectedCount = codepointCount(std::string_view(field.text).substr(begin, end - begin));
			if (field.maxLength && codepointCount(field.text) - selectedCount >= field.maxLength) return false;
			std::string encoded; appendUtf8(encoded, codepoint);
			if (end > begin) field.text.erase(begin, end - begin);
			field.text.insert(begin, encoded);
			field.caret = field.selectionAnchor = begin + encoded.size();
			resetCaretBlink();
			m_layout.markContentChanged(field.layout); addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout));
			if (field.pickerEditor) updatePickerFromEditor(field);
			else if (field.numberField) commitNumberText(field);
			else if (field.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(field.pressHandler));
			if (field.pickerEditor) addDamage(pickerEditorRect(field));
			return true;
		}
		return false;
	}

	bool UiRuntime::textInput(std::string_view utf8) {
		bool changed = false;
		for (std::size_t i = 0; i < utf8.size();) {
			const u8 lead = static_cast<u8>(utf8[i]);
			u32 codepoint = 0xFFFDu;
			std::size_t length = 1;
			if (lead < 0x80u) codepoint = lead;
			else if ((lead & 0xE0u) == 0xC0u) { codepoint = lead & 0x1Fu; length = 2; }
			else if ((lead & 0xF0u) == 0xE0u) { codepoint = lead & 0x0Fu; length = 3; }
			else if ((lead & 0xF8u) == 0xF0u) { codepoint = lead & 0x07u; length = 4; }
			if (i + length > utf8.size()) { codepoint = 0xFFFDu; length = 1; }
			else if (length > 1) {
				bool valid = true;
				for (std::size_t byte = 1; byte < length; ++byte) {
					const u8 continuation = static_cast<u8>(utf8[i + byte]);
					if ((continuation & 0xC0u) != 0x80u) { valid = false; break; }
					codepoint = (codepoint << 6) | (continuation & 0x3Fu);
				}
				const bool overlong = (length == 2 && codepoint < 0x80u) ||
					(length == 3 && codepoint < 0x800u) ||
					(length == 4 && codepoint < 0x10000u);
				if (!valid || overlong || codepoint > 0x10FFFFu || (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
					codepoint = 0xFFFDu; length = 1;
				}
			}
			changed |= textInput(codepoint);
			i += length;
		}
		return changed;
	}

	void UiRuntime::updateSlider(Node& node, Vec2 point) {
		const Rect rect = m_layout.absRect(node.layout);
		if (rect.w <= 0 || node.maxValue <= node.minValue) return;
		const f32 fraction = std::clamp((point.x - rect.x) / rect.w, 0.f, 1.f);
		node.value = node.minValue + fraction * (node.maxValue - node.minValue);
	}

	bool UiRuntime::updateColorPicker(Node& node, Vec2 point) {
		const Rect anchor = m_layout.absRect(node.layout);
		const PickerGeometry geometry = pickerGeometry(node);
		const Rect popup = geometry.popup;
		if (!popup.contains(point)) return false;
		f32 hue = 0.f, saturation = 0.f, value = 0.f;
		const f32 hi = std::max({ node.pickerColor.r, node.pickerColor.g, node.pickerColor.b });
		const f32 lo = std::min({ node.pickerColor.r, node.pickerColor.g, node.pickerColor.b });
		const f32 delta = hi - lo; value = hi; saturation = hi > 0.f ? delta / hi : 0.f;
		if (delta > 0.f) {
			if (hi == node.pickerColor.r) hue = std::fmod((node.pickerColor.g - node.pickerColor.b) / delta, 6.f) / 6.f;
			else if (hi == node.pickerColor.g) hue = ((node.pickerColor.b - node.pickerColor.r) / delta + 2.f) / 6.f;
			else hue = ((node.pickerColor.r - node.pickerColor.g) / delta + 4.f) / 6.f;
			if (hue < 0.f) hue += 1.f;
		}
		Color selected = node.pickerColor;
		if (geometry.alpha.contains(point))
			selected.a = std::clamp((point.x - geometry.alpha.x) / geometry.alpha.w, 0.f, 1.f);
		else if (geometry.hue.contains(point)) {
			hue = std::clamp((point.x - geometry.hue.x) / geometry.hue.w, 0.f, 1.f);
			selected = hsv(hue, saturation, value).withAlpha(node.pickerAlpha ? node.pickerColor.a : 1.f);
		}
		else if (geometry.saturationValue.contains(point)) {
			saturation = std::clamp((point.x - geometry.saturationValue.x) / geometry.saturationValue.w, 0.f, 1.f);
			value = 1.f - std::clamp((point.y - geometry.saturationValue.y) / geometry.saturationValue.h, 0.f, 1.f);
			selected = hsv(hue, saturation, value).withAlpha(node.pickerAlpha ? node.pickerColor.a : 1.f);
		} else return false;
		if (selected == node.pickerColor) return false;
		node.pickerColor = selected;
		node.text = colorText(selected, node.pickerFormat, node.pickerAlpha, node.pickerPrecision);
		const u32 ownerIndex = static_cast<u32>(&node - m_nodes.data());
		syncPickerEditors(ownerIndex);
		m_layout.markContentChanged(node.layout);
		addDamage(anchor); addDamage(popup);
		if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
		return true;
	}

	Rect UiRuntime::pickerEditorRect(const Node& editor) const {
		if (!editor.pickerEditor || editor.pickerOwner >= m_nodes.size()) return {};
		const Node& owner = m_nodes[editor.pickerOwner];
		const PickerGeometry geometry = pickerGeometry(owner);
		if (editor.pickerSlot < 4) return geometry.channels[editor.pickerSlot];
		return geometry.hexValue;
	}

	UiRuntime::PickerGeometry UiRuntime::pickerGeometry(const Node& node) const {
		PickerGeometry g;
		g.popup = popupBounds(m_layout.absRect(node.layout), false, 0, m_lastLayoutHeight, node.pickerWidth, node.pickerHeight);
		const f32 inset = 12.f, width = std::max(0.f, g.popup.w - inset * 2.f);
		f32 top = g.popup.y + inset;
		if (node.pickerShowSaturationValue) {
			g.saturationValue = { g.popup.x + inset, top, width, 148.f };
			top = g.saturationValue.bottom() + 10.f;
		}
		if (node.pickerShowHue) {
			g.hue = { g.popup.x + inset, top, width, 14.f };
			top = g.hue.bottom() + 8.f;
		}
		if (node.pickerShowAlpha && node.pickerAlpha) {
			g.alpha = { g.popup.x + inset, top, width, 12.f };
			top = g.alpha.bottom() + 10.f;
		}
		g.format = { g.popup.x + inset, top, width, 28.f };
		top = g.format.bottom() + 10.f;
		if (node.pickerShowInputs) {
			g.channelLabels = { g.popup.x + inset, top, width, 14.f };
			const f32 count = node.pickerAlpha ? 4.f : 3.f;
			const f32 gap = 6.f, fieldWidth = std::max(1.f, (width - gap * (count - 1.f)) / count);
			const f32 fieldsY = g.channelLabels.bottom() + 3.f;
			for (u32 i = 0; i < static_cast<u32>(count); ++i)
				g.channels[i] = { g.popup.x + inset + i * (fieldWidth + gap), fieldsY, fieldWidth, 28.f };
			const f32 hexY = fieldsY + 36.f;
			g.hexLabel = { g.popup.x + inset, hexY, 36.f, 28.f };
			g.hexValue = { g.hexLabel.right() + 8.f, hexY, std::max(1.f, width - 44.f), 28.f };
		}
		return g;
	}

	void UiRuntime::syncPickerEditors(u32 ownerIndex, u32 preserveIndex) {
		if (ownerIndex >= m_nodes.size()) return;
		const Node& owner = m_nodes[ownerIndex];
		const auto values = pickerEditorValues(owner.pickerColor, owner.pickerFormat, owner.pickerAlpha, owner.pickerPrecision);
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& editor = m_nodes[i];
			if (!editor.pickerEditor || editor.pickerOwner != ownerIndex || i == preserveIndex) continue;
			editor.text = values[editor.pickerSlot];
			editor.caret = editor.text.size();
			editor.selectionAnchor = editor.caret;
			m_layout.markContentChanged(editor.layout);
		}
	}

	bool UiRuntime::updatePickerFromEditor(Node& editor) {
		if (!editor.pickerEditor || editor.pickerOwner >= m_nodes.size()) return false;
		Node& owner = m_nodes[editor.pickerOwner];
		Color next = owner.pickerColor;
		if (editor.pickerSlot == 4) {
			if (!parseColorText(editor.text, next)) return false;
		}
		else {
			std::array<f32, 4> channel{};
			const auto current = pickerEditorValues(owner.pickerColor, owner.pickerFormat, owner.pickerAlpha, owner.pickerPrecision);
			for (u32 i = 0; i < 4; ++i) {
				const std::string& input = i == editor.pickerSlot ? editor.text : current[i];
				char* end = nullptr;
				channel[i] = std::strtof(input.c_str(), &end);
				if (end == input.c_str() || *end != '\0' || !std::isfinite(channel[i])) return false;
			}
			std::string expression;
			char text[128]{};
			if (owner.pickerFormat == "rgb" || owner.pickerFormat == "hex")
				std::snprintf(text, sizeof(text), "rgba(%.0f, %.0f, %.0f, %.4f)", channel[0], channel[1], channel[2], double(channel[3]));
			else
				std::snprintf(text, sizeof(text), "%s(%.4f, %.4f%%, %.4f%%, %.4f)", owner.pickerFormat == "hsv" ? "hsva" : "hsla",
					double(channel[0]), double(channel[1]), double(channel[2]), double(channel[3]));
			expression = text;
			if (!parseColorText(expression, next)) return false;
		}
		if (!owner.pickerAlpha) next.a = 1.f;
		const bool changed = next != owner.pickerColor;
		if (!changed) return false;
		owner.pickerColor = next;
		owner.text = colorText(next, owner.pickerFormat, owner.pickerAlpha, owner.pickerPrecision);
		syncPickerEditors(editor.pickerOwner, static_cast<u32>(&editor - m_nodes.data()));
		m_layout.markContentChanged(owner.layout);
		const Rect anchor = m_layout.absRect(owner.layout);
		addDamage(anchor);
		addDamage(popupBounds(anchor, false, 0, m_lastLayoutHeight, owner.pickerWidth, owner.pickerHeight));
		if (owner.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(owner.pressHandler));
		return true;
	}

	void UiRuntime::resetCaretBlink() {
		m_caretVisible = true;
		m_caretBlinkDeadline = std::chrono::steady_clock::now() + kCaretBlinkInterval;
	}

	Rect UiRuntime::searchClearRect(const Node& node) const {
		const Rect rect = m_layout.absRect(node.layout);
		const f32 side = std::clamp(rect.h - 10.f, 12.f, 20.f);
		return { rect.right() - side - 4.f, rect.y + (rect.h - side) * 0.5f, side, side };
	}

	bool UiRuntime::selectSegment(Node& node, u32 index, bool notify) {
		if (index >= node.options.size()) return false;
		const bool changed = node.selectedOption != index;
		node.selectedOption = index;
		if (changed) {
			addDamage(m_layout.absRect(node.layout));
			if (notify && node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
		}
		return changed;
	}

	void UiRuntime::stepViewMonth(Node& node, i32 direction) {
		i32 month = static_cast<i32>(node.viewMonth) - 1 + direction;   // 0-based
		node.viewYear += (month >= 0 ? month : month - 11) / 12;        // floor division
		month = ((month % 12) + 12) % 12;
		node.viewMonth = static_cast<u32>(month) + 1u;
	}

	void UiRuntime::applyNumberStep(Node& node, f32 direction) {
		const f32 stepped = node.value + direction * node.stepValue;
		const f32 clamped = node.numberBounded ? std::clamp(stepped, std::min(node.minValue, node.maxValue), std::max(node.minValue, node.maxValue)) : stepped;
		if (clamped == node.value) return;
		node.value = clamped;
		node.text = formatNumberText(clamped, node.numberPrecision) + (node.unit.empty() ? std::string{} : node.unit);
		node.caret = node.selectionAnchor = node.text.size();
		m_layout.markContentChanged(node.layout);
		if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
	}

	bool UiRuntime::commitNumberText(Node& node) {
		const char* text = node.text.c_str();
		char* end = nullptr;
		const f32 parsed = std::strtof(text, &end);
		if (end == text || !std::isfinite(parsed)) return false;
		while (end && *end == ' ') ++end;
		if (end && *end != '\0' && (node.unit.empty() || std::string_view(end) != std::string_view(node.unit))) return false;
		const f32 clamped = node.numberBounded ? std::clamp(parsed, std::min(node.minValue, node.maxValue), std::max(node.minValue, node.maxValue)) : parsed;
		const bool changed = clamped != node.value;
		node.value = clamped;
		if (changed && node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
		return true;
	}

	u32 UiRuntime::hitTestNode(Vec2 point) const {
		if (point.x < 0.f || point.y < 0.f || point.x >= m_lastLayoutWidth || point.y >= m_lastLayoutHeight)
			return 0xFFFFFFFFu;
		// Dock tabs and panel headers are chrome above the retained panel contents.
		for (auto it = m_dockHosts.rbegin(); it != m_dockHosts.rend(); ++it) {
			const DockHostState& host = *it;
			if (host.node >= m_nodes.size() || !isNodeVisible(host.node)) continue;
			const Rect hostRect = m_layout.absRect(m_nodes[host.node].layout);
			const LayoutStyle& hostStyle = m_layout.style(m_nodes[host.node].layout);
			const Vec2 origin{ hostRect.x + hostStyle.border.l + hostStyle.padding.l, hostRect.y + hostStyle.border.t + hostStyle.padding.t };
			const Vec2 local{ point.x - origin.x, point.y - origin.y };
			const u32 active = host.tree.panelAt(local);
			if (active < m_nodes.size()) {
				const Rect panel = host.tree.panelBounds(active);
				if (point.y >= origin.y + panel.y && point.y < origin.y + panel.y + m_nodes[active].dockHeaderHeight)
					return active;
			}
		}
		// Tab headers are painted in the overlay layer above the retained node tree.
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			const Node& tabs = m_nodes[index];
			if (!tabs.tabs || tabs.disabled || !isNodeVisible(index)) continue;
			const Rect rect = m_layout.absRect(tabs.layout);
			if (rect.contains(point) && point.y < rect.y + 34.f) return index;
		}
		// Picker editors are popup overlay controls and therefore sit above every authored node.
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			const Node& editor = m_nodes[index];
			if (editor.pickerEditor && !editor.disabled && isNodeVisible(index) && pickerEditorRect(editor).contains(point)) return index;
		}
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			const Node& node = m_nodes[index];
			if (!node.scrollView || !isNodeVisible(index) || !pointInsideScrollAncestors(index, point)) continue;
			if (scrollBarRect(index, false).contains(point) || scrollBarRect(index, true).contains(point)) return index;
		}
		for (std::size_t i = m_nodes.size(); i > 0; --i) {
			const u32 index = static_cast<u32>(i - 1);
			const Node& node = m_nodes[index];
			if (!isNodeVisible(index) || !pointInsideScrollAncestors(index, point)) continue;
			if (node.fpsCounter && !m_fpsEnabled) continue;
			if (node.pickerEditor) continue;
			const Rect rect = m_layout.absRect(node.layout);
			if (rect.empty() || !rect.contains(point)) continue;

			const bool interactive = node.button || node.checkItem || node.checkBox || node.slider || node.dropdown ||
				node.colorPicker || node.textBox || node.textArea || node.tabs ||
				node.switchToggle || node.toggleButton || node.radioGroup ||
				node.segmented || node.stepper || node.rating || node.breadcrumb || node.keybindField || node.datePicker;
			const bool structuralBlocker = typeIs(node.type, *m_strings, "Window") || typeIs(node.type, *m_strings, "Text") ||
				typeIs(node.type, *m_strings, "Panel") || typeIs(node.type, *m_strings, "Split") ||
				typeIs(node.type, *m_strings, "CheckList") || typeIs(node.type, *m_strings, "Tab") ||
				node.progressBar || node.fpsCounter || node.label || node.badge || node.spinner || node.image || node.dockHost || node.dockPanel;
			const bool styledBlocker = node.style.has(PropId::Bg) && color(node.style, PropId::Bg, Color{}).a > 0.f;
			// Node storage follows paint order: descendants and later siblings are on top.
			// Transparent Row/Column containers pass input through; painted containers block
			// controls behind them. Tooltip bubbles are deliberately pointer-transparent.
			if (interactive || structuralBlocker || styledBlocker) return index;
		}
		return 0xFFFFFFFFu;
	}

	UiRuntime::Node* UiRuntime::findNode(NodeId layoutNode) {
		if (layoutNode.index >= m_nodes.size() || m_nodes[layoutNode.index].layout != layoutNode) return nullptr;
		return &m_nodes[layoutNode.index];
	}

	const UiRuntime::Node* UiRuntime::findNode(NodeId layoutNode) const {
		if (layoutNode.index >= m_nodes.size() || m_nodes[layoutNode.index].layout != layoutNode) return nullptr;
		return &m_nodes[layoutNode.index];
	}

	StateMask UiRuntime::activeStates(const Node& node) const {
		StateMask states = stateBit(State::Resting);
		if (node.hovered) states |= stateBit(State::Hover);
		if (node.pressed) states |= stateBit(State::Pressed);
		if (node.focused) states |= stateBit(State::Focus);
		if (node.checked) states |= stateBit(State::Checked) | stateBit(State::Selected);
		if (node.disabled) states |= stateBit(State::Disabled);
		else states |= stateBit(State::Enabled);
		if ((node.textBox || node.textArea) && node.readOnly) states |= stateBit(State::ReadOnly);
		if (node.popupOpen) states |= stateBit(State::Open);
		if (node.tab && node.parentIndex < m_nodes.size() && m_nodes[node.parentIndex].tabs) {
			const Node& tabs = m_nodes[node.parentIndex];
			u32 ordinal = 0;
			for (NodeId child = m_layout.firstChild(tabs.layout); child.valid(); child = m_layout.nextSibling(child)) {
				if (child.index == node.layout.index) { if (ordinal == tabs.selectedTab) states |= stateBit(State::Selected); break; }
				++ordinal;
			}
		}
		if ((node.textBox || node.textArea) && node.text.empty() && !node.placeholder.empty()) states |= stateBit(State::Placeholder);
		return states;
	}

	bool UiRuntime::caretEditableFocused() const {
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			const Node& node = m_nodes[i];
			if ((node.textBox || node.textArea) && node.focused && !node.disabled && !node.readOnly && isNodeVisible(i)) return true;
		}
		return false;
	}

	bool UiRuntime::isNodeVisible(u32 index) const {
		if (index >= m_nodes.size()) return false;
		if (m_nodes[index].dockPanel) {
			if (m_nodes[index].dockClosed) return false;
			const DockHostState* host = dockHostForPanel(index);
			if (!host || !host->tree.contains(index)) return false;
			if (!host->tree.floating(index) && host->tree.activePanel(index) != index) return false;
		}
		if (m_nodes[index].pickerEditor) {
			const u32 owner = m_nodes[index].pickerOwner;
			return owner < m_nodes.size() && m_nodes[owner].popupOpen && isNodeVisible(owner) &&
				(m_nodes[index].pickerSlot != 3 || m_nodes[owner].pickerAlpha);
		}
		u32 child = index;
		while (m_nodes[child].parentIndex != 0xFFFFFFFFu) {
			const u32 parent = m_nodes[child].parentIndex;
			if (m_nodes[parent].dockPanel && !isNodeVisible(parent)) return false;
			if (m_nodes[parent].tabs && m_nodes[child].tab) {
				u32 ordinal = 0;
				for (NodeId page = m_layout.firstChild(m_nodes[parent].layout); page.valid(); page = m_layout.nextSibling(page), ++ordinal)
					if (page.index == child) return ordinal == m_nodes[parent].selectedTab;
			}
			child = parent;
		}
		return true;
	}

	void UiRuntime::refreshTooltipTarget(std::chrono::steady_clock::time_point now) {
		u32 next = 0xFFFFFFFFu;
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			const Node& node = m_nodes[i];
			if (node.disabled || node.tooltipDisabled || node.tooltipDismissed || !isNodeVisible(i) || node.tooltipText.empty()) continue;
			const bool trigger = (node.tooltipTrigger & 1u && node.hovered) || (node.tooltipTrigger & 2u && node.focused);
			if (trigger) { next = i; break; }
		}
		if (next == m_tooltipOwner) {
			if (next != 0xFFFFFFFFu) m_tooltipHideDeadline = {};
			return;
		}
		if (next == 0xFFFFFFFFu && m_tooltipOwner < m_nodes.size() && m_tooltipVisible) {
			if (m_tooltipHideDeadline.time_since_epoch().count() == 0) {
				const u32 delay = m_nodes[m_tooltipOwner].tooltipHideDelayMs;
				if (delay) { m_tooltipHideDeadline = now + std::chrono::milliseconds(delay); return; }
			}
			else return;
		}
		if (m_tooltipOwner != 0xFFFFFFFFu) addDamage({ 0, 0, m_lastLayoutWidth, m_lastLayoutHeight });
		m_tooltipOwner = next;
		m_tooltipVisible = false;
		m_tooltipHideDeadline = {};
		m_tooltipDeadline = next == 0xFFFFFFFFu ? std::chrono::steady_clock::time_point{} : now + std::chrono::milliseconds(m_nodes[next].tooltipShowDelayMs);
	}

	bool UiRuntime::updateTimers(std::chrono::steady_clock::time_point now) {
		bool redraw = false;
		for (u32 i = 0; i < m_nodes.size(); ++i) {
			Node& node = m_nodes[i];
			if (!node.scrollView || !node.scrollAnimating) continue;
			const Vec2 current = m_layout.scrollOffset(node.layout);
			Vec2 next{ current.x + (node.scrollTarget.x - current.x) * 0.28f, current.y + (node.scrollTarget.y - current.y) * 0.28f };
			if (std::fabs(node.scrollTarget.x - next.x) < 0.5f && std::fabs(node.scrollTarget.y - next.y) < 0.5f) { next = node.scrollTarget; node.scrollAnimating = false; }
			m_layout.setScrollOffset(node.layout, next);
			addDamage(m_layout.absRect(node.layout));
			if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
			redraw = true;
		}
		if (m_tooltipOwner != 0xFFFFFFFFu && !m_tooltipVisible && now >= m_tooltipDeadline) {
			m_tooltipVisible = true;
			addDamage({ 0, 0, m_lastLayoutWidth, m_lastLayoutHeight });
			redraw = true;
		}
		if (m_tooltipVisible && m_tooltipHideDeadline.time_since_epoch().count() != 0 && now >= m_tooltipHideDeadline) {
			addDamage({ 0, 0, m_lastLayoutWidth, m_lastLayoutHeight });
			m_tooltipOwner = 0xFFFFFFFFu;
			m_tooltipVisible = false;
			m_tooltipDeadline = m_tooltipHideDeadline = {};
			redraw = true;
		}
		if (m_fpsEnabled && m_fpsDeadline.time_since_epoch().count() != 0 && now >= m_fpsDeadline) {
			m_fpsDeadline = now + kFpsRefreshInterval;
			for (u32 i = 0; i < m_nodes.size(); ++i)
				if (m_nodes[i].fpsCounter && isNodeVisible(i)) addDamage(m_layout.absRect(m_nodes[i].layout));
			redraw = true;
		}
		bool spinnerVisible = false;
		for (u32 i = 0; i < m_nodes.size(); ++i)
			if ((m_nodes[i].spinner || (m_nodes[i].progressBar && m_nodes[i].indeterminate)) && !m_nodes[i].disabled && isNodeVisible(i)) { spinnerVisible = true; break; }
		if (spinnerVisible && (m_spinnerDeadline.time_since_epoch().count() == 0 || now >= m_spinnerDeadline)) {
			for (u32 i = 0; i < m_nodes.size(); ++i)
				if ((m_nodes[i].spinner || (m_nodes[i].progressBar && m_nodes[i].indeterminate)) && !m_nodes[i].disabled && isNodeVisible(i)) addDamage(m_layout.absRect(m_nodes[i].layout));
			redraw = true;
		}
		else if (!spinnerVisible && m_spinnerDeadline.time_since_epoch().count() != 0) {
			m_spinnerDeadline = {};
		}
		// The blink is pending only while an editable field is actually focused; a stale deadline
		// from a field that lost focus must not keep the app waking (or report a timer delay).
		if (m_caretBlinkDeadline.time_since_epoch().count() != 0) {
			if (!caretEditableFocused()) m_caretBlinkDeadline = {};
			else if (now >= m_caretBlinkDeadline) {
				for (u32 i = 0; i < m_nodes.size(); ++i) {
					const Node& node = m_nodes[i];
					if ((node.textBox || node.textArea) && node.focused && !node.disabled && !node.readOnly && isNodeVisible(i))
						addDamage(node.pickerEditor ? pickerEditorRect(node) : m_layout.absRect(node.layout));
				}
				redraw = true;
			}
		}
		return redraw;
	}

	f64 UiRuntime::nextTimerDelay(std::chrono::steady_clock::time_point now) const {
		f64 delay = -1.0;
		if (m_tooltipOwner != 0xFFFFFFFFu && !m_tooltipVisible)
			delay = std::max(0.0, std::chrono::duration<f64>(m_tooltipDeadline - now).count());
		if (m_tooltipVisible && m_tooltipHideDeadline.time_since_epoch().count() != 0) {
			const f64 hideDelay = std::max(0.0, std::chrono::duration<f64>(m_tooltipHideDeadline - now).count());
			delay = delay < 0.0 ? hideDelay : std::min(delay, hideDelay);
		}
		if (m_fpsEnabled && m_fpsDeadline.time_since_epoch().count() != 0) {
			const f64 fpsDelay = std::max(0.0, std::chrono::duration<f64>(m_fpsDeadline - now).count());
			delay = delay < 0.0 ? fpsDelay : std::min(delay, fpsDelay);
		}
		if (m_spinnerDeadline.time_since_epoch().count() != 0) {
			const f64 spinnerDelay = std::max(0.0, std::chrono::duration<f64>(m_spinnerDeadline - now).count());
			delay = delay < 0.0 ? spinnerDelay : std::min(delay, spinnerDelay);
		}
		for (const Node& node : m_nodes) if (node.scrollAnimating) {
			const f64 scrollDelay = 1.0 / 60.0;
			delay = delay < 0.0 ? scrollDelay : std::min(delay, scrollDelay);
			break;
		}
		if (m_caretBlinkDeadline.time_since_epoch().count() != 0 && caretEditableFocused()) {
			const f64 caretDelay = std::max(0.0, std::chrono::duration<f64>(m_caretBlinkDeadline - now).count());
			delay = delay < 0.0 ? caretDelay : std::min(delay, caretDelay);
		}
		return delay;
	}

	Color UiRuntime::controlColor(std::string_view id) const {
		for (const Node& node : m_nodes) if (node.colorPicker && node.id.v && m_strings->view(node.id) == id) return node.pickerColor;
		return {};
	}

	std::string_view UiRuntime::controlText(std::string_view id) const {
		for (const Node& node : m_nodes) if ((node.dropdown || node.colorPicker || node.textBox || node.textArea || typeIs(node.type, *m_strings, "Text") || node.label || node.badge || node.stepper || node.datePicker || node.keybindField) && node.id.v && m_strings->view(node.id) == id) return node.text;
		for (const Node& node : m_nodes) if ((node.radioGroup || node.segmented || node.breadcrumb) && node.id.v && m_strings->view(node.id) == id && node.selectedOption < node.options.size()) return node.options[node.selectedOption];
		return {};
	}

	std::optional<UiValue> UiRuntime::value(std::string_view id) const {
		for (const Node& node : m_nodes) {
			if (!node.id.v || m_strings->view(node.id) != id) continue;
			if (node.dockPanel) {
				const DockHostState* host = dockHostForPanel(static_cast<u32>(&node - m_nodes.data()));
				if (!host || !host->tree.contains(static_cast<u32>(&node - m_nodes.data()))) return std::nullopt;
				const u32 panelIndex = static_cast<u32>(&node - m_nodes.data());
				DockPanelValue value;
				value.region = host->tree.zone(panelIndex);
				value.floating = host->tree.floating(panelIndex);
				value.bounds = host->tree.panelBounds(panelIndex);
				value.order = host->tree.panelOrder(panelIndex);
				value.active = host->tree.activePanel(panelIndex) == panelIndex;
				value.host = m_nodes[host->node].id.v ? std::string(m_strings->view(m_nodes[host->node].id)) : std::string{};
				const u32 active = host->tree.activePanel(panelIndex);
				if (active < m_nodes.size() && m_nodes[active].id.v) value.activeTab.assign(m_strings->view(m_nodes[active].id));
				return UiValue{ std::move(value) };
			}
			if (node.checkItem || node.checkBox || node.switchToggle || node.toggleButton) return UiValue{ node.checked };
			if (node.slider || node.rating || node.progressBar) return UiValue{ static_cast<f64>(node.value) };
			if (node.numberField || node.stepper) return UiValue{ static_cast<f64>(node.value) };
			if (node.dropdown || node.textBox || node.textArea || typeIs(node.type, *m_strings, "Text") || node.label || node.badge || node.datePicker || node.keybindField) return UiValue{ node.text };
			if (node.radioGroup || node.segmented || node.breadcrumb) return node.selectedOption < node.options.size() ? UiValue{ node.options[node.selectedOption] } : UiValue{ std::string{} };
			if (node.colorPicker) return UiValue{ node.pickerColor };
			if (node.tabs) return UiValue{ node.selectedTab };
			if (node.scrollView) return UiValue{ m_layout.scrollOffset(node.layout) };
		}
		return std::nullopt;
	}

	bool UiRuntime::setValue(std::string_view id, const UiValue& valueToSet, bool notify) {
		for (Node& node : m_nodes) {
			if (!node.id.v || m_strings->view(node.id) != id) continue;
			bool changed = false;
			if (node.dockPanel) {
				const DockPanelValue* requested = std::get_if<DockPanelValue>(&valueToSet);
				if (!requested) return false;
				const u32 panelIndex = static_cast<u32>(&node - m_nodes.data());
				DockHostState* host = dockHostForPanel(panelIndex);
				if (!host || !host->tree.contains(panelIndex)) return false;
				const bool wasFloating = host->tree.floating(panelIndex);
				if (requested->floating) {
					Rect floating = requested->bounds;
					if (floating.w <= 0.f) floating.w = std::max(1.f, node.dockPreferredSize.x);
					if (floating.h <= 0.f) floating.h = std::max(1.f, node.dockPreferredSize.y);
					changed = wasFloating ? host->tree.moveFloating(panelIndex, floating) : host->tree.undock(panelIndex, floating);
				}
				else if (!requested->targetPanel.empty()) {
					u32 target = 0xFFFFFFFFu;
					for (u32 candidate : host->panelNodes)
						if (candidate != panelIndex && m_nodes[candidate].id.v && m_strings->view(m_nodes[candidate].id) == requested->targetPanel) { target = candidate; break; }
					if (target == 0xFFFFFFFFu) return false;
					changed = host->tree.dock(panelIndex, target, requested->region);
				}
				else if (requested->active && !wasFloating) changed = host->tree.activate(panelIndex);
				if (!requested->floating && !host->tree.floating(panelIndex)) {
					changed |= host->tree.reorder(panelIndex, requested->order);
					if (requested->active) changed |= host->tree.activate(panelIndex);
				}
				if (!changed) return true;
				updateDockHostLayout(*host, true);
				addDockDamage(*host);
				if (notify) {
					fireDockHandler(panelIndex, requested->floating ? node.undockHandler : node.dockHandler);
					if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
				}
				return true;
			}
			if (node.checkItem || node.checkBox) {
				const bool* value = std::get_if<bool>(&valueToSet); if (!value) return false;
				changed = node.checked != *value; node.checked = *value;
			}
			else if (node.slider || node.progressBar) {
				const f64* value = std::get_if<f64>(&valueToSet); if (!value) return false;
				const f32 clamped = std::clamp(static_cast<f32>(*value), node.minValue, node.maxValue);
				changed = node.value != clamped; node.value = clamped;
			}
			else if (node.switchToggle || node.toggleButton) {
				const bool* value = std::get_if<bool>(&valueToSet); if (!value) return false;
				changed = node.checked != *value; node.checked = *value;
			}
			else if (node.radioGroup || node.segmented || node.breadcrumb) {
				const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
				const auto found = std::find(node.options.begin(), node.options.end(), *value); if (found == node.options.end()) return false;
				const u32 selected = static_cast<u32>(found - node.options.begin());
				changed = node.selectedOption != selected; node.selectedOption = selected;
			}
			else if (node.datePicker) {
				const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
				i32 year = 0; u32 month = 0, day = 0;
				if (!(std::sscanf(value->data(), "%4d-%2u-%2u", &year, &month, &day) == 3 && month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month))) return false;
				changed = !(year == node.dateYear && month == node.dateMonth && day == node.dateDay);
				node.dateYear = year; node.dateMonth = month; node.dateDay = day;
				node.viewYear = year; node.viewMonth = month;
				node.text = formatDateText(year, month, day);
				if (changed) m_layout.markContentChanged(node.layout);
			}
			else if (node.numberField || node.stepper) {
				if (const f64* number = std::get_if<f64>(&valueToSet)) {
					const f32 clamped = node.numberBounded ? std::clamp(static_cast<f32>(*number), std::min(node.minValue, node.maxValue), std::max(node.minValue, node.maxValue)) : static_cast<f32>(*number);
					changed = node.value != clamped;
					node.value = clamped;
					node.text = formatNumberText(clamped, node.numberPrecision) + (node.unit.empty() ? std::string{} : node.unit);
					node.caret = node.selectionAnchor = node.text.size();
					if (changed) m_layout.markContentChanged(node.layout);
				}
				else if (const std::string* string = std::get_if<std::string>(&valueToSet)) {
					node.text = truncateUtf8(*string, node.maxLength);
					node.caret = node.selectionAnchor = node.text.size();
					m_layout.markContentChanged(node.layout);
					changed = commitNumberText(node);
				}
				else return false;
			}
			else if (node.dropdown) {
				const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
				const auto found = std::find(node.options.begin(), node.options.end(), *value); if (found == node.options.end()) return false;
				const u32 selected = static_cast<u32>(found - node.options.begin());
				changed = node.selectedOption != selected; node.selectedOption = selected; node.text = *value;
				if (changed) m_layout.markContentChanged(node.layout);
			}
			else if (node.textBox || node.textArea || typeIs(node.type, *m_strings, "Text") || node.label || node.badge || node.keybindField) {
				const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
				std::string next = truncateUtf8(*value, node.maxLength);
				changed = node.text != next; node.text = std::move(next); node.caret = node.selectionAnchor = node.text.size();
				if (changed) m_layout.markContentChanged(node.layout);
			}
			else if (node.colorPicker) {
				Color next;
				if (const Color* colorValue = std::get_if<Color>(&valueToSet)) next = *colorValue;
				else if (const std::string* stringValue = std::get_if<std::string>(&valueToSet)) { if (!parseColorText(*stringValue, next)) return false; }
				else return false;
				if (!node.pickerAlpha) next.a = 1.f;
				changed = node.pickerColor != next; node.pickerColor = next; node.text = colorText(next, node.pickerFormat, node.pickerAlpha, node.pickerPrecision);
				if (changed) syncPickerEditors(static_cast<u32>(&node - m_nodes.data()));
				if (changed) m_layout.markContentChanged(node.layout);
			}
			else if (node.tabs) {
				const u32* value = std::get_if<u32>(&valueToSet); if (!value) return false;
				const u32 count = m_layout.childCount(node.layout); if (!count) return false;
				const u32 selected = std::min(*value, count - 1); changed = node.selectedTab != selected; node.selectedTab = selected;
				if (changed) for (u32 i = 0; i < m_nodes.size(); ++i) if (m_nodes[i].focused && !isNodeVisible(i)) m_nodes[i].focused = false;
			}
			else if (node.scrollView) {
				const Vec2* offset = std::get_if<Vec2>(&valueToSet); if (!offset) return false;
				changed = setScrollOffset(static_cast<u32>(&node - m_nodes.data()), *offset, false);
			}
			else return false;
			if (changed) {
				addDamage(m_layout.absRect(node.layout));
				if (notify && node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
			}
			return true;
		}
		return false;
	}

	Size UiRuntime::measureLeaf(void* context, NodeId node, Constraint constraint) {
		return static_cast<UiRuntime*>(context)->measure(node, constraint);
	}

	Size UiRuntime::measure(NodeId node, Constraint constraint) const {
		const Node* runtimeNode = findNode(node);
		if (!runtimeNode) return {};
		// Non-text widgets keep usable intrinsic sizes when .xui/.xss leave them auto-sized.
		if (runtimeNode->switchToggle) {
			if (runtimeNode->text.empty() || !m_text || !runtimeNode->font.valid()) return { 44.f, 24.f };
			const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16.f) : 16.f;
			const TextLayout& label = m_text->layout(runtimeNode->font, runtimeNode->text, fontSize, 0.f);
			return { std::max(44.f, label.width + 46.f), std::max(24.f, label.height) };
		}
		if (runtimeNode->checkItem || runtimeNode->checkBox) {
			const Size emptySize = runtimeNode->checkItem ? Size{ 160.f, 28.f } : Size{ 20.f, 20.f };
			if (runtimeNode->text.empty() || !m_text || !runtimeNode->font.valid()) return emptySize;
			const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16.f) : 16.f;
			const TextLayout& label = m_text->layout(runtimeNode->font, runtimeNode->text, fontSize, 0.f);
			return { std::max(emptySize.w, label.width + 26.f), std::max(emptySize.h, label.height) };
		}
		if (runtimeNode->button || runtimeNode->toggleButton) {
			if (runtimeNode->text.empty() || !m_text || !runtimeNode->font.valid()) return { 96.f, 36.f };
		}
		if (runtimeNode->fpsCounter) return { 168.f, 26.f };
		if (runtimeNode->separator) return { 12.f, 1.f };
		if (runtimeNode->spinner) return { 18.f, 18.f };
		if (runtimeNode->image) return runtimeNode->imageW ? Size{ f32(runtimeNode->imageW), f32(runtimeNode->imageH) } : Size{ 96.f, 64.f };
		// Empty controls still need a useful preferred size. Without these defaults, an auto-sized
		// Slider/ProgressBar or an empty placeholder field measured to 0x0 until the app authored
		// explicit dimensions. Keep these as content-box sizes; LayoutTree adds XUI/XSS padding and borders.
		if (runtimeNode->slider) return { 160.f, 24.f };
		if (runtimeNode->progressBar) return { 160.f, 16.f };
		if (runtimeNode->colorPicker) return { 156.f, 34.f };
		if (runtimeNode->dropdown) {
			Size preferred{ 156.f, 34.f };
			if (!runtimeNode->text.empty() && m_text && runtimeNode->font.valid()) {
				const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16.f) : 16.f;
				const TextLayout& label = m_text->layout(runtimeNode->font, runtimeNode->text, fontSize, 0.f);
				preferred.w = std::max(preferred.w, label.width + 32.f);
				preferred.h = std::max(preferred.h, label.height + 12.f);
			}
			return preferred;
		}
		if (runtimeNode->textBox || runtimeNode->textArea) {
			const f32 minWidth = runtimeNode->textArea ? 240.f : (runtimeNode->numberField || runtimeNode->searchField ? 180.f : 160.f);
			const f32 minHeight = runtimeNode->textArea ? 96.f : 28.f;
			const std::string& visibleText = runtimeNode->text.empty() ? runtimeNode->placeholder : runtimeNode->text;
			if (!visibleText.empty() && m_text && runtimeNode->font.valid()) {
				const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16.f) : 16.f;
				const f32 wrapWidth = constraint.fw ? constraint.w : 0.f;
				const TextLayout& content = m_text->layout(runtimeNode->font, visibleText, fontSize, wrapWidth);
				const f32 controlInset = runtimeNode->numberField ? 60.f : runtimeNode->searchField ? 24.f : 0.f;
				return { std::max(minWidth, content.width + controlInset), std::max(minHeight, content.height) };
			}
			return { minWidth, minHeight };
		}
		if (runtimeNode->radioGroup) {
			if (!m_text || !runtimeNode->font.valid() || runtimeNode->options.empty()) return { 160.f, 28.f };
			const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16) : 16.f;
			f32 width = 0.f;
			for (const std::string& option : runtimeNode->options)
				width = std::max(width, m_text->layout(runtimeNode->font, option, fontSize, 0.f).width + 30.f);
			return { width, f32(runtimeNode->options.size()) * 28.f };
		}
		if (runtimeNode->segmented || runtimeNode->breadcrumb) {
			if (!m_text || !runtimeNode->font.valid() || runtimeNode->options.empty())
				return runtimeNode->segmented ? Size{ 240.f, 28.f } : Size{ 220.f, 22.f };
			const f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16) : 16.f;
			f32 total = 0.f, widest = 0.f;
			for (const std::string& option : runtimeNode->options) {
				const f32 width = m_text->layout(runtimeNode->font, option, fontSize, 0.f).width;
				total += width;
				widest = std::max(widest, width);
			}
			if (runtimeNode->segmented) return { widest * f32(runtimeNode->options.size()) + 16.f, 28.f };
			return { total + f32(runtimeNode->options.size()) * 22.f, std::max(22.f, fontSize * 1.4f) };
		}
		if (runtimeNode->stepper || runtimeNode->keybindField || runtimeNode->datePicker) return { runtimeNode->datePicker ? 170.f : 180.f, 36.f };
		if (runtimeNode->rating) return { std::clamp(runtimeNode->maxValue, 1.f, 10.f) * 26.f, 24.f };
		if (runtimeNode->text.empty() || !m_text || !runtimeNode->font.valid()) return {};
		f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16) : 16.f;
		const f32 wrapWidth = constraint.fw ? constraint.w : 0.f;
		const TextLayout& run = m_text->layout(runtimeNode->font, runtimeNode->text, fontSize, wrapWidth);
		const f32 spacing = runtimeNode->style.has(PropId::LetterSpacing) ? number(runtimeNode->style.get(PropId::LetterSpacing), 0) : 0.f;
		const f32 contentWidth = run.width + spacing * static_cast<f32>(run.glyphs.size() > 0 ? run.glyphs.size() - 1 : 0);
		if (runtimeNode->button || runtimeNode->toggleButton) return { std::max(96.f, contentWidth), std::max(36.f, run.height) };
		return { contentWidth, run.height };
	}

	f32 UiRuntime::number(const StyleValue& value, std::size_t index, f32 fallback) {
		if (index >= value.count || value.atoms[index].kind != AtomKind::Number) return fallback;
		return value.atoms[index].num;
	}

	Length UiRuntime::length(const StyleValue& value, std::size_t index) {
		if (index >= value.count || value.atoms[index].kind != AtomKind::Number) return Length::autoLen();
		const Atom& atom = value.atoms[index];
		return atom.unit == Unit::Percent ? Length::pct(atom.num) : Length::px(atom.num);
	}

	Align UiRuntime::align(const Atom& atom) const {
		const std::string_view value = m_strings->view(atom.str());
		if (value == "Start") return Align::Start;
		if (value == "Center") return Align::Center;
		if (value == "End") return Align::End;
		if (value == "Stretch") return Align::Stretch;
		return Align::Auto;
	}

	Justify UiRuntime::justify(const Atom& atom) const {
		const std::string_view value = m_strings->view(atom.str());
		if (value == "Center") return Justify::Center;
		if (value == "End") return Justify::End;
		if (value == "SpaceBetween") return Justify::SpaceBetween;
		if (value == "SpaceAround") return Justify::SpaceAround;
		if (value == "SpaceEvenly") return Justify::SpaceEvenly;
		return Justify::Start;
	}

	Color UiRuntime::color(const PropTable& style, PropId property, Color fallback) {
		if (!style.has(property) || style.get(property).count == 0) return fallback;
		const Atom& atom = style.get(property).atoms[0];
		return atom.kind == AtomKind::Color ? Color::fromRGBA8(atom.bits) : fallback;
	}

	FontHandle UiRuntime::selectFont(const PropTable& style) const {
		if (!style.has(PropId::Font) || !m_strings) return m_font;
		const Atom& familyAtom = style.get(PropId::Font).atoms[0];
		if (familyAtom.kind != AtomKind::String && familyAtom.kind != AtomKind::Ident) return m_font;
		const StrId familyId = familyAtom.str();
		const u16 requested = style.has(PropId::FontWeight)
			? static_cast<u16>(std::clamp(number(style.get(PropId::FontWeight), 0, 400.f), 100.f, 900.f)) : 400;
		const FontFace* best = nullptr;
		u16 bestDistance = 0xFFFF;
		for (const FontFace& face : m_fontFaces) if (face.family == familyId) {
			const u16 distance = static_cast<u16>(std::abs(static_cast<int>(face.weight) - static_cast<int>(requested)));
			if (distance < bestDistance) { best = &face; bestDistance = distance; }
		}
		return best ? best->font : m_font;
	}

} // namespace xui
