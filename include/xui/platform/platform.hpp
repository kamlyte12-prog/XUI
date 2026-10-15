#pragma once
/// @file platform.hpp  The platform-agnostic windowing/input interface. Widgets and application code
/// never see a backend type (GLFW, Win32, headless null) — only this interface.
///
/// Event model: platforms queue events; the app drains them with pumpEvents() until it returns false.
/// Event storage owned by the platform is valid until the next pumpEvents() call (IME strings, file paths).
/// requestRedraw() is the only repaint trigger: nothing redraws unless asked (idle CPU ~ 0).
#include <xui/core/handle.hpp>
#include <xui/core/math.hpp>
#include <xui/core/types.hpp>
#include <string>
#include <string_view>

namespace xui {

using WindowHandle = Handle<struct WindowTag>;

/// Keyboard keys. Printable keys use their ASCII uppercase value ('A'..'Z', '0'..'9');
/// named keys below start at 0xE000 to stay clear of ASCII.
enum class Key : u16 {
    None = 0,
    Space = 32,
    Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Num0 = 48, Num9 = 57,
    Semicolon = 59, Equal = 61,
    A = 65, Z = 90,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, Grave = 96,
    Escape = 0xE000, Enter, Tab, Backspace, Insert, Delete,
    Right = 0xE010, Left, Down, Up, PageUp, PageDown, Home, End,
    CapsLock = 0xE020, ScrollLock, NumLock, PrintScreen, Pause,
    F1 = 0xE030, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Kp0 = 0xE040, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal, KpDivide, KpMultiply, KpSubtract, KpAdd, KpEnter, KpEqual,
    LeftShift = 0xE060, LeftCtrl, LeftAlt, LeftSuper, RightShift, RightCtrl, RightAlt, RightSuper,
    Menu = 0xE070
};

enum class Mod : u8 { Shift = 1, Ctrl = 2, Alt = 4, Super = 8 };
enum class MouseButton : u8 { Left = 0, Right, Middle, X1, X2 };
enum class CursorShape : u8 {
    Arrow, IBeam, Crosshair, Hand, ResizeEW, ResizeNS, ResizeNWSE, ResizeNESW, ResizeAll, NotAllowed, Hidden
};

enum class EventType : u8 {
    None = 0,
    MouseMove, MouseEnter, MouseLeave, MouseDown, MouseUp, Wheel,
    KeyDown, KeyUp, TextInput, ImeCompose, ImeCommit,
    FocusGained, FocusLost, Resize, DpiChange, CloseRequest, FileDrop, Tick,
    Quit
};

/// One input/window event. Common fields first; per-type fields follow. POD, cheap to copy.
struct Event {
    EventType type = EventType::None;
    u8 mods = 0;                       ///< Mod bits (keyboard + the state during mouse events)
    MouseButton button = MouseButton::Left;
    Key key = Key::None;               ///< KeyDown/KeyUp (None if the platform cannot identify it)
    bool repeat = false;               ///< KeyDown auto-repeat
    WindowHandle window;               ///< originating window
    f32 x = 0, y = 0;                  ///< mouse: logical px, origin top-left of the client area
    f32 dx = 0, dy = 0;                ///< MouseMove deltas / Wheel scroll deltas (notch units, dy>0 = away)
    f32 w = 0, h = 0;                  ///< Resize: new logical size. DpiChange: old dpi scale
    f32 scale = 1;                     ///< DpiChange: new dpi scale
    u32 codepoint = 0;                 ///< TextInput: single Unicode scalar (0 if platform only sends UTF-8)
    const char* text = "";             ///< TextInput/ImeCommit: UTF-8, platform-owned. ImeCompose: pre-edit string
    i32 selStart = -1, selEnd = -1;    ///< ImeCompose: pre-edit selection range in UTF-8 bytes (-1 = none)
    const char* const* paths = nullptr;///< FileDrop: UTF-8 paths, platform-owned until the next pumpEvents()
    u32 pathCount = 0;
};

struct WindowDesc {
    std::string_view title;
    u32 width = 1280, height = 720;    ///< logical px
    u32 minWidth = 128, minHeight = 96;
    bool resizable = true;
    bool visible = true;
    bool glContext = false;            ///< ask the platform for an OpenGL context on this window (WGL/GLFW)
    bool alwaysOnTop = false;
};

struct PlatformDesc {
    std::string_view appName = "xui-app";
    /// Block in waitEvents() when the queue is empty (true: ~0% idle CPU, the default) instead of polling.
    bool blockOnWait = true;
};

class Platform {
public:
    virtual ~Platform() = default;
    virtual bool init(const PlatformDesc&) = 0;
    virtual void shutdown() = 0;

    virtual WindowHandle createWindow(const WindowDesc&) = 0;
    virtual void destroyWindow(WindowHandle) = 0;
    virtual void setWindowTitle(WindowHandle, std::string_view) = 0;
    virtual Vec2 windowSizeLogical(WindowHandle) const = 0;  ///< logical px
    virtual Vec2i windowSizePixels(WindowHandle) const = 0;  ///< physical px (framebuffer)
    virtual f32 dpiScale(WindowHandle) const = 0;

    /// Pop the next queued event. Returns false when the queue is empty.
    virtual bool pumpEvents(Event& out) = 0;
    /// Sleep until an event arrives or @p timeoutSec passes (0 = wait indefinitely). No busy waiting.
    virtual void waitEvents(f64 timeoutSec) = 0;
    virtual f64 time() const = 0;      ///< seconds on a monotonic clock

    virtual void* nativeWindowHandle(WindowHandle) = 0;   ///< HWND / GLFWwindow* / nullptr
    virtual void* nativeGlContext(WindowHandle) = 0;      ///< HGLRC / nullptr
    virtual void swapBuffers(WindowHandle) = 0;           ///< present a GL context (no-op without one)
    virtual void makeGlCurrent(WindowHandle) = 0;
    virtual void setCursor(CursorShape) = 0;
    virtual void setClipboardText(std::string_view) = 0;
    virtual std::string clipboardText() const = 0;
    /// Ask for a redraw (delivered as an EventType::Tick on the app loop, coalesced per frame).
    virtual void requestRedraw(WindowHandle) = 0;
    /// Optional: backends whose interactive resize runs a modal loop (Win32 sizing via GLFW)
    /// never return from pumpEvents/waitEvents mid-drag, so the app loop cannot redraw.
    /// Such backends invoke this hook from their size callback to render each live step.
    /// Default no-op keeps headless and non-modal backends unchanged.
    virtual void setLiveResizeHook(WindowHandle, void (*)(void*), void*) {}
};

} // namespace xui
