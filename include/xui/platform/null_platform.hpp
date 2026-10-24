#pragma once
/// @file null_platform.hpp  Headless Platform backend for tests, CI and golden-image runs.
///
/// No real window exists. Tests script input with pushEvent()/helper factories and drive time explicitly
/// (setTime/advanceTime) so animation and debounce logic is fully deterministic. requestRedraw() queues a
/// single coalesced Tick, mirroring what real window systems do.
#include <xui/platform/platform.hpp>
#include <deque>
#include <string>
#include <vector>

namespace xui {

	class NullPlatform final : public Platform {
	public:
		bool init(const PlatformDesc&) override;
		void shutdown() override;

		WindowHandle createWindow(const WindowDesc&) override;
		void destroyWindow(WindowHandle) override;
		void setWindowTitle(WindowHandle, std::string_view) override;
		std::string_view windowTitle(WindowHandle h) const;
		Vec2 windowSizeLogical(WindowHandle h) const override { return sizeLogical(h); }
		Vec2i windowSizePixels(WindowHandle h) const override { return sizePixels(h); }
		f32 dpiScale(WindowHandle) const override;

		bool pumpEvents(Event& out) override;
		void waitEvents(f64) override {}               // headless: nothing to wait for
		f64 time() const override { return m_time; }

		void* nativeWindowHandle(WindowHandle) override { return nullptr; }
		void* nativeGlContext(WindowHandle) override { return nullptr; }
		void swapBuffers(WindowHandle) override {}
		void makeGlCurrent(WindowHandle) override {}
		void setCursor(CursorShape s) override { m_lastCursor = s; m_cursorCount++; }
		void setClipboardText(std::string_view s) override { m_clipboard.assign(s); m_clipboardWrites++; }
		std::string clipboardText() const override { return m_clipboard; }
		void requestRedraw(WindowHandle w) override;

		// ---- test scripting ------------------------------------------------------------------------
		void pushEvent(const Event& e) { m_queue.push_back(e); }
		/// Deterministic clock: advance and return the new time. Does NOT auto-emit Tick events.
		f64 setTime(f64 t) { return m_time = t; }
		f64 advanceTime(f64 dt) { m_time += dt; return m_time; }

		CursorShape lastCursor() const { return m_lastCursor; }
		u32 cursorSetCount() const { return m_cursorCount; }
		u32 clipboardWriteCount() const { return m_clipboardWrites; }
		u32 pendingTicks() const { return m_pendingTicks; }
		void setDpiScale(f32 s) { m_dpi = s; }

		// ---- custom chrome recording (TitleBar support) --------------------------------------------
		bool setDecorated(WindowHandle, bool decorated) override { m_lastDecorated = decorated; ++m_setDecoratedCount; return true; }
		bool beginSystemMove(WindowHandle) override { ++m_systemMoveCount; return true; }
		bool beginSystemResize(WindowHandle, WindowEdge edge) override { m_lastResizeEdge = edge; ++m_systemResizeCount; return true; }
		void minimizeWindow(WindowHandle) override { ++m_minimizeCount; }
		bool toggleMaximize(WindowHandle) override { m_maximized = !m_maximized; ++m_maximizeToggleCount; return m_maximized; }
		bool windowMaximized(WindowHandle) const override { return m_maximized; }
		void closeWindow(WindowHandle) override { ++m_closeCount; }
		void setResizeInset(WindowHandle, f32 pixels) override { m_resizeInset = pixels; }

		f32 lastResizeInset() const { return m_resizeInset; }
		f32 lastCornerRadius() const { return m_cornerRadius; }

		bool lastDecorated() const { return m_lastDecorated; }
		u32 setDecoratedCount() const { return m_setDecoratedCount; }
		u32 systemMoveCount() const { return m_systemMoveCount; }
		u32 systemResizeCount() const { return m_systemResizeCount; }
		WindowEdge lastResizeEdge() const { return m_lastResizeEdge; }
		u32 minimizeCount() const { return m_minimizeCount; }
		u32 maximizeToggleCount() const { return m_maximizeToggleCount; }
		u32 closeCount() const { return m_closeCount; }
		void resetChromeCounts() {
			m_setDecoratedCount = m_systemMoveCount = m_systemResizeCount = m_minimizeCount = m_maximizeToggleCount = m_closeCount = 0;
		}
		/// Simulate a resize: stores the new size and queues Resize + DpiChange events.
		void simulateResize(WindowHandle w, u32 logicalW, u32 logicalH, f32 newDpi = 0.f);
		/// Simulate a full click (down + up at the same position) with given modifiers.
		void click(MouseButton b, f32 x, f32 y, u8 mods = 0);
		void keyTap(Key k, u8 mods = 0);
		void typeText(std::string_view utf8);

	private:
		struct Win { std::string title; u32 w = 0, h = 0; bool alive = false; u32 gen = 1; };
		Win* find(WindowHandle h);
		const Win* find(WindowHandle h) const { return const_cast<NullPlatform*>(this)->find(h); }
		Vec2 sizeLogical(WindowHandle h) const;
		Vec2i sizePixels(WindowHandle h) const;

		std::vector<Win> m_wins;
		std::deque<Event> m_queue;
		std::string m_clipboard;
		CursorShape m_lastCursor = CursorShape::Arrow;
		f32 m_dpi = 1.f;
		f64 m_time = 0;
		u32 m_cursorCount = 0, m_clipboardWrites = 0, m_pendingTicks = 0;
		// custom chrome recording
		bool m_lastDecorated = true;
		WindowEdge m_lastResizeEdge = WindowEdge::Left;
		bool m_maximized = false;
		f32 m_resizeInset = 0.f;
		f32 m_cornerRadius = 0.f;
		u32 m_setDecoratedCount = 0, m_systemMoveCount = 0, m_systemResizeCount = 0;
		u32 m_minimizeCount = 0, m_maximizeToggleCount = 0, m_closeCount = 0;
	};

} // namespace xui
