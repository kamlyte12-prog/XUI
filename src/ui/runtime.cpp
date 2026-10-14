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
        output = {expression.num, expression.num2, expression.num, expression.num2};
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
    if (h < 1.f) out = {c, x, 0, 1}; else if (h < 2.f) out = {x, c, 0, 1};
    else if (h < 3.f) out = {0, c, x, 1}; else if (h < 4.f) out = {0, x, c, 1};
    else if (h < 5.f) out = {x, 0, c, 1}; else out = {c, 0, x, 1};
    out.r += m; out.g += m; out.b += m; return out;
}
std::string colorHex(Color c) { char text[10]; std::snprintf(text, sizeof(text), "#%08x", c.toRGBA8()); return text; }
std::string colorText(Color c, std::string_view format, bool alpha, u32 precision) {
    precision = std::min<u32>(precision, 4);
    char text[96]{};
    const f32 scale = std::pow(10.f, static_cast<f32>(precision));
    auto rounded = [scale](f32 v) { return std::round(v * scale) / scale; };
    if (format == "hex") {
        const u32 rgba = c.toRGBA8();
        if (alpha) std::snprintf(text, sizeof(text), "#%08X", rgba);
        else std::snprintf(text, sizeof(text), "#%06X", rgba >> 8);
    } else if (format == "rgb") {
        if (alpha) std::snprintf(text, sizeof(text), "rgba(%d, %d, %d, %.*f)", int(c.r*255.f+0.5f), int(c.g*255.f+0.5f), int(c.b*255.f+0.5f), int(precision), rounded(c.a));
        else std::snprintf(text, sizeof(text), "rgb(%d, %d, %d)", int(c.r*255.f+0.5f), int(c.g*255.f+0.5f), int(c.b*255.f+0.5f));
    } else {
        const f32 hi = std::max({c.r,c.g,c.b}), lo = std::min({c.r,c.g,c.b}), delta = hi-lo;
        f32 h=0.f, s=0.f, l=(hi+lo)*0.5f;
        if (delta > 0.f) {
            if (hi==c.r) h=std::fmod((c.g-c.b)/delta,6.f);
            else if (hi==c.g) h=(c.b-c.r)/delta+2.f;
            else h=(c.r-c.g)/delta+4.f;
            h*=60.f; if(h<0.f) h+=360.f;
            s = format=="hsv" ? delta/hi : delta/(1.f-std::fabs(2.f*l-1.f));
        }
        if(format=="hsv") {
            if(alpha) std::snprintf(text,sizeof(text),"hsva(%.*f, %.*f%%, %.*f%%, %.*f)",int(precision),rounded(h),int(precision),rounded(s*100),int(precision),rounded(hi*100),int(precision),rounded(c.a));
            else std::snprintf(text,sizeof(text),"hsv(%.*f, %.*f%%, %.*f%%)",int(precision),rounded(h),int(precision),rounded(s*100),int(precision),rounded(hi*100));
        } else if(alpha) std::snprintf(text,sizeof(text),"hsla(%.*f, %.*f%%, %.*f%%, %.*f)",int(precision),rounded(h),int(precision),rounded(s*100),int(precision),rounded(l*100),int(precision),rounded(c.a));
        else std::snprintf(text,sizeof(text),"hsl(%.*f, %.*f%%, %.*f%%)",int(precision),rounded(h),int(precision),rounded(s*100),int(precision),rounded(l*100));
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
    } else {
        if (format == "rgb") {
            std::snprintf(text, sizeof(text), "%u", u32(c.r * 255.f + 0.5f)); values[0] = text;
            std::snprintf(text, sizeof(text), "%u", u32(c.g * 255.f + 0.5f)); values[1] = text;
            std::snprintf(text, sizeof(text), "%u", u32(c.b * 255.f + 0.5f)); values[2] = text;
        } else {
            const f32 hi = std::max({c.r, c.g, c.b}), lo = std::min({c.r, c.g, c.b}), delta = hi - lo;
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
    while(!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
    while(!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);
    if(Color::parseHex(input,out)) return true;
    const auto open=input.find('('), close=input.rfind(')');
    if(open==std::string_view::npos || close!=input.size()-1 || close<=open) return false;
    const auto kind=input.substr(0,open);
    const bool rgb=kind=="rgb"||kind=="rgba", hsl=kind=="hsl"||kind=="hsla", hsvFormat=kind=="hsv"||kind=="hsva";
    if(!rgb&&!hsl&&!hsvFormat) return false;
    std::array<f32,4> v{0,0,0,1}; std::array<bool,4> percent{}; u32 n=0;
    std::string_view body=input.substr(open+1,close-open-1);
    while(!body.empty()) {
        const auto comma=body.find(','); auto part=body.substr(0,comma);
        if(n>=4) return false;
        while(!part.empty()&&std::isspace(static_cast<unsigned char>(part.front()))) part.remove_prefix(1);
        while(!part.empty()&&(std::isspace(static_cast<unsigned char>(part.back()))||part.back()=='%')) { if(part.back()=='%') percent[n]=true; part.remove_suffix(1); }
        if(n>=4||part.empty()) return false;
        std::string copy(part); char* end=nullptr; const float x=std::strtof(copy.c_str(),&end);
        if(end==copy.c_str()||*end!='\0'||!std::isfinite(x)) return false;
        v[n++]=x; if(comma==std::string_view::npos) break; body.remove_prefix(comma+1);
    }
    if(n!=3&&n!=4) return false;
    if(rgb) { for(u32 i=0;i<3;++i) v[i]=std::clamp(percent[i]?v[i]/100.f:v[i]/255.f,0.f,1.f); out={v[0],v[1],v[2],std::clamp(v[3],0.f,1.f)}; return true; }
    const f32 h=std::fmod(v[0],360.f)/360.f, s=std::clamp(percent[1]?v[1]/100.f:v[1],0.f,1.f), third=std::clamp(percent[2]?v[2]/100.f:v[2],0.f,1.f);
    if(hsvFormat) out=hsv(h,s,third);
    else {
        const f32 chroma=(1.f-std::fabs(2.f*third-1.f))*s, x=chroma*(1.f-std::fabs(std::fmod(h*6.f,2.f)-1.f)), m=third-chroma*0.5f, segment=h*6.f;
        Color c; if(segment<1)c={chroma,x,0,1}; else if(segment<2)c={x,chroma,0,1}; else if(segment<3)c={0,chroma,x,1}; else if(segment<4)c={0,x,chroma,1}; else if(segment<5)c={x,0,chroma,1}; else c={chroma,0,x,1};
        out={c.r+m,c.g+m,c.b+m,1};
    }
    out.a=std::clamp(v[3],0.f,1.f); return true;
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
Rect popupBounds(Rect anchor, bool dropdown, std::size_t optionCount, f32 windowHeight, f32 pickerWidth=196.f, f32 pickerHeight=204.f) {
    const f32 height = dropdown ? 28.f * static_cast<f32>(optionCount) : pickerHeight;
    const f32 y = anchor.y + anchor.h + height <= windowHeight ? anchor.y + anchor.h : std::max(0.f, anchor.y - height);
    return {anchor.x, y, dropdown ? std::max(anchor.w, 150.f) : std::max(196.f,pickerWidth), height};
}
} // namespace

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
        } else if (name == "size" && arg.expr < document.exprs.size()) {
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
            m_styles->resolve(editor.styleSet, stateBit(State::Resting), 0, editor.style);
            const PropId inherited[] = {PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing};
            for (u8 i = 0; i < std::size(inherited); ++i) {
                const PropId property = inherited[i];
                if (!editor.style.has(property) && owner.style.has(property)) {
                    editor.style.v[static_cast<int>(property)] = owner.style.get(property);
                    editor.style.mask |= u64{1} << static_cast<int>(property);
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
    if (it == m_fontFaces.end()) m_fontFaces.push_back({familyId, weight, font}); else it->font = font;
    for (Node& node : m_nodes) {
        node.font = selectFont(node.style);
        if (!node.text.empty()) m_layout.markContentChanged(node.layout);
    }
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
    node.slider = typeIs(source.type, *m_strings, "Slider");
    node.progressBar = typeIs(source.type, *m_strings, "ProgressBar");
    node.dropdown = typeIs(source.type, *m_strings, "Dropdown");
    node.colorPicker = typeIs(source.type, *m_strings, "ColorPicker");
    node.textBox = typeIs(source.type, *m_strings, "TextBox");
    node.textArea = typeIs(source.type, *m_strings, "TextArea");
    node.tabs = typeIs(source.type, *m_strings, "Tabs");
    node.tab = typeIs(source.type, *m_strings, "Tab");
    node.fpsCounter = typeIs(source.type, *m_strings, "FpsCounter");

    if (typeIs(source.type, *m_strings, "Window") || typeIs(source.type, *m_strings, "Column") ||
        typeIs(source.type, *m_strings, "Panel") || typeIs(source.type, *m_strings, "CheckList")) {
        node.layout = m_layout.create(LayoutKind::Column);
    } else if (typeIs(source.type, *m_strings, "Row")) {
        node.layout = m_layout.create(LayoutKind::Row);
    } else if (node.split) {
        node.layout = m_layout.create(LayoutKind::Split);
    } else if (node.tabs) {
        node.layout = m_layout.create(LayoutKind::Stack);
    } else if (node.tab) {
        node.layout = m_layout.create(LayoutKind::Column);
    } else if (typeIs(source.type, *m_strings, "Text") || node.button || node.checkItem || node.checkBox || node.slider || node.progressBar || node.fpsCounter || node.dropdown || node.colorPicker || node.textBox || node.textArea) {
        node.layout = m_layout.create(LayoutKind::Leaf);
    } else {
        error = "unsupported runtime widget '" + std::string(m_strings->view(source.type)) + "'";
        return false;
    }

    if (node.layout.index != m_nodes.size()) { error = "runtime node storage lost its layout index invariant"; return false; }
    if (parentIndex != 0xFFFFFFFFu && !m_layout.append(m_nodes[parentIndex].layout, node.layout)) {
        error = "could not attach .xui child to its parent";
        return false;
    }

    if ((typeIs(source.type, *m_strings, "Text") || node.button || node.checkItem || node.checkBox || node.fpsCounter) && source.argCount) {
        node.text.assign(literal(document, *m_strings, document.args[source.argFirst].expr));
    }
    if (node.tab && source.argCount) node.tabTitle.assign(literal(document, *m_strings, document.args[source.argFirst].expr));
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
        if (name == "label" && (node.button || node.checkItem)) node.text.assign(value);
        else if (name == "label" && node.checkBox) node.text.assign(value);
        else if (name == "tooltip" && prop.role == XPropRole::Value) node.tooltipText.assign(value);
        else if (name == "placeholder" && prop.role == XPropRole::Value) node.placeholder.assign(value);
        else if (node.colorPicker && name == "format" && prop.role == XPropRole::Value) node.pickerFormat.assign(value);
        else if (node.colorPicker && name == "formats" && prop.role == XPropRole::Value) {
            node.pickerFormats.clear(); std::size_t begin=0;
            while(begin<=value.size()) { const std::size_t end=value.find('|',begin); std::string item(value.substr(begin,end==std::string_view::npos?value.size()-begin:end-begin));
                if(item=="hex"||item=="rgb"||item=="hsl"||item=="hsv") node.pickerFormats.push_back(std::move(item));
                if(end==std::string_view::npos) break;
                begin=end+1;
            }
        }
        else if (node.colorPicker && name == "precision" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
            node.pickerPrecision = static_cast<u32>(std::clamp(document.exprs[prop.expr].num,0.f,4.f));
        else if (node.colorPicker && name == "popupSize" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Size) {
            const XExpr& size=document.exprs[prop.expr];
            node.pickerWidth=std::max(220.f,size.num); node.pickerHeight=std::max(264.f,size.num2);
        }
        else if (node.colorPicker && (name == "alpha" || name == "showAlpha" || name == "showInputs" || name == "showFormatSwitcher" || name == "showHue" || name == "showSaturationValue") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
            const bool enabled = document.exprs[prop.expr].kind != XExprKind::Bool || document.exprs[prop.expr].bval;
            if(name=="alpha") node.pickerAlpha=enabled;
            if(name=="showAlpha") node.pickerShowAlpha=enabled;
            if(name=="showInputs") node.pickerShowInputs=enabled;
            if(name=="showFormatSwitcher") node.pickerShowFormatSwitcher=enabled;
            if(name=="showHue") node.pickerShowHue=enabled;
            if(name=="showSaturationValue") node.pickerShowSaturationValue=enabled;
        }
        else if (((name == "value" && (node.textBox || node.textArea || node.colorPicker || node.dropdown)) || (name == "selected" && node.tabs)) && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
            const XExpr& expression = document.exprs[prop.expr];
            if ((node.textBox || node.textArea) && name == "value") node.text = value;
            else if (node.tabs && name == "selected" && expression.kind == XExprKind::Number) node.selectedTab = static_cast<u32>(std::max(0.f, expression.num));
            else if (node.colorPicker && name == "value" && expression.kind == XExprKind::Color) node.pickerColor = Color::fromRGBA8(expression.color);
            else if (node.colorPicker && name == "value" && expression.kind == XExprKind::String) { Color parsed; if (parseColorText(value, parsed)) node.pickerColor = parsed; }
            else if (node.dropdown && name == "value") for (u32 i = 0; i < node.options.size(); ++i) if (node.options[i] == value) node.selectedOption = i;
        }
        else if (name == "maxLength" && prop.role == XPropRole::Value && prop.expr < document.exprs.size() && document.exprs[prop.expr].kind == XExprKind::Number)
            node.maxLength = static_cast<u32>(std::max(0.f, document.exprs[prop.expr].num));
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
        } else if (name == "press" && prop.role == XPropRole::Event && prop.expr < document.exprs.size()) {
            const XExpr& handler = document.exprs[prop.expr];
            if (handler.kind == XExprKind::Name) node.pressHandler = handler.name;
        } else if (name == "change" && prop.role == XPropRole::Event && prop.expr < document.exprs.size()) {
            const XExpr& handler = document.exprs[prop.expr];
            if (handler.kind == XExprKind::Name) node.pressHandler = handler.name;
        } else if (name == "disabled" && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
            node.disabled = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
        } else if (name == "options" && prop.role == XPropRole::Value) {
            std::size_t begin = 0;
            while (begin <= value.size()) {
                const std::size_t end = value.find('|', begin);
                std::string option(value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin));
                if (!option.empty()) node.options.push_back(std::move(option));
                if (end == std::string_view::npos) break;
                begin = end + 1;
            }
        } else if ((name == "value" || name == "min" || name == "max") && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
            const XExpr& expression = document.exprs[prop.expr];
            if (expression.kind == XExprKind::Number) {
                if (name == "value") node.value = expression.num;
                else if (name == "min") node.minValue = expression.num;
                else node.maxValue = expression.num;
            } else if (name == "value" && node.colorPicker && expression.kind == XExprKind::Color)
                node.pickerColor = Color::fromRGBA8(expression.color);
            else if (name == "value" && node.dropdown)
                for (u32 i = 0; i < node.options.size(); ++i) if (node.options[i] == value) node.selectedOption = i;
        } else if (name == "checked" && prop.role == XPropRole::Value && prop.expr < document.exprs.size()) {
            node.checked = document.exprs[prop.expr].kind == XExprKind::Bool && document.exprs[prop.expr].bval;
        } else if (prop.role == XPropRole::If || prop.role == XPropRole::For) {
            error = "runtime .xui subset does not yet evaluate if/for control-flow properties";
            return false;
        }
    }

    if (node.colorPicker) {
        if (node.pickerFormats.empty()) node.pickerFormats = {"hex"};
        if (std::find(node.pickerFormats.begin(), node.pickerFormats.end(), node.pickerFormat) == node.pickerFormats.end()) node.pickerFormat = node.pickerFormats.front();
        if (!node.pickerAlpha) node.pickerColor.a = 1.f;
        node.text = colorText(node.pickerColor, node.pickerFormat, node.pickerAlpha, node.pickerPrecision);
    }
    MatchInput match;
    match.type = node.type;
    match.id = node.id;
    match.classes = node.classes;
    node.styleSet = m_styles->match(match);
    m_styles->resolve(node.styleSet, stateBit(State::Resting), 0, node.style);
    if (parentIndex != 0xFFFFFFFFu && parentIndex < m_nodes.size()) {
        const PropId inherited[] = {PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing};
        const PropTable& parentStyle = m_nodes[parentIndex].style;
        for (u8 i = 0; i < std::size(inherited); ++i) {
            const PropId property = inherited[i];
            if (!node.style.has(property) && parentStyle.has(property)) {
                node.style.v[static_cast<int>(property)] = parentStyle.get(property);
                node.style.mask |= u64{1} << static_cast<int>(property);
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
            authoredLayout.gap = {std::max(0.f, expression.num), std::max(0.f, expression.num2)};
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
        } else if (name == "size" || name == "minSize" || name == "maxSize") {
            f32 width = 0, height = 0;
            if (expression.kind == XExprKind::Size) { width = expression.num; height = expression.num2; }
            else if (!layoutPixels(expression, width)) return failLayout(name);
            else height = width;
            if (name == "size") { authoredLayout.width = Length::px(width); authoredLayout.height = Length::px(height); }
            else if (name == "minSize") { authoredLayout.minW = std::max(0.f, width); authoredLayout.minH = std::max(0.f, height); }
            else { authoredLayout.maxW = std::max(0.f, width); authoredLayout.maxH = std::max(0.f, height); }
        } else if (name == "width" || name == "height") {
            Length value;
            if (!layoutLength(expression, value)) return failLayout(name);
            if (name == "width") authoredLayout.width = value; else authoredLayout.height = value;
        } else if (name == "pad" || name == "margin") {
            Edges value;
            if (!layoutEdges(expression, value)) return failLayout(name);
            if (name == "pad") { authoredLayout.padding = value; node.hasXuiPadding = true; node.xuiPadding = value; }
            else authoredLayout.margin = value;
        } else if (name == "align") {
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
                typeIs(source.type, *m_strings, "CheckList") || typeIs(source.type, *m_strings, "Tab");
            if (container) authoredLayout.alignItems = alignValue;
            else authoredLayout.alignSelf = alignValue;
        } else if (name == "justify") {
            if (expression.kind != XExprKind::Name) return failLayout(name);
            const std::string_view value = m_strings->view(expression.name);
            if (value == "Start") authoredLayout.justify = Justify::Start;
            else if (value == "Center") authoredLayout.justify = Justify::Center;
            else if (value == "End") authoredLayout.justify = Justify::End;
            else if (value == "SpaceBetween") authoredLayout.justify = Justify::SpaceBetween;
            else if (value == "SpaceAround") authoredLayout.justify = Justify::SpaceAround;
            else if (value == "SpaceEvenly") authoredLayout.justify = Justify::SpaceEvenly;
            else return failLayout(name);
        } else if (name == "gap" && typeIs(source.type, *m_strings, "CheckList")) {
            if (!parseGap(expression, name)) return false;
        }
    }
    m_layout.setStyle(node.layout, authoredLayout);
    m_nodes.push_back(std::move(node));
    if (m_nodes[runtimeIndex].dropdown || m_nodes[runtimeIndex].colorPicker) {
        Node& control = m_nodes[runtimeIndex];
        if (control.dropdown) {
            if (control.options.empty()) control.options = {"Option 1", "Option 2", "Option 3"};
            if (control.selectedOption < control.options.size()) control.text = control.options[control.selectedOption];
        } else { control.text = colorText(control.pickerColor,control.pickerFormat,control.pickerAlpha,control.pickerPrecision); }
    }
    if (m_nodes[runtimeIndex].textBox || m_nodes[runtimeIndex].textArea) {
        Node& field = m_nodes[runtimeIndex];
        field.text = truncateUtf8(field.text, field.maxLength);
        field.caret = field.text.size();
    }
    if (m_nodes[runtimeIndex].button || m_nodes[runtimeIndex].checkItem || m_nodes[runtimeIndex].checkBox || m_nodes[runtimeIndex].slider || m_nodes[runtimeIndex].dropdown || m_nodes[runtimeIndex].colorPicker || m_nodes[runtimeIndex].textBox || m_nodes[runtimeIndex].textArea || m_nodes[runtimeIndex].tabs) m_interactiveNodes.push_back(runtimeIndex);
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
        addDamage({0, 0, width, height});
        m_lastLayoutWidth = width; m_lastLayoutHeight = height;
    }
    if (m_root.valid()) m_layout.layout(m_root, width, height);
}

void UiRuntime::addDamage(Rect rect) {
    if (rect.empty()) return;
    if (!m_hasDamage) { m_damage = rect; m_hasDamage = true; return; }
    const f32 left = std::min(m_damage.x, rect.x), top = std::min(m_damage.y, rect.y);
    const f32 right = std::max(m_damage.right(), rect.right()), bottom = std::max(m_damage.bottom(), rect.bottom());
    m_damage = {left, top, right - left, bottom - top};
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
        layoutStyle.minW = number(v, 0); layoutStyle.minH = number(v, 1);
    }
    if (node.style.has(PropId::MaxSize)) {
        const StyleValue& v = node.style.get(PropId::MaxSize);
        layoutStyle.maxW = number(v, 0, kInf); layoutStyle.maxH = number(v, 1, kInf);
    }
    if (node.style.has(PropId::Pad)) {
        const StyleValue& v = node.style.get(PropId::Pad);
        layoutStyle.padding = {number(v, 3), number(v, 0), number(v, 1), number(v, 2)};
    }
    if (node.style.has(PropId::Margin)) {
        const StyleValue& v = node.style.get(PropId::Margin);
        layoutStyle.margin = {number(v, 3), number(v, 0), number(v, 1), number(v, 2)};
    }
    if (node.style.has(PropId::Gap)) {
        const StyleValue& v = node.style.get(PropId::Gap);
        layoutStyle.gap = {number(v, 0), number(v, 1)};
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
    m_dpiScale = std::max(0.25f, dpiScale);
    for (u32 nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex) {
        Node& node = m_nodes[nodeIndex];
        if ((node.fpsCounter && !m_fpsEnabled) || !isNodeVisible(nodeIndex)) continue;
        const StateMask states = activeStates(node);
        if (!node.styleResolved || node.resolvedStates != states) {
            m_styles->resolve(node.styleSet, states, 0, node.style);
            node.inheritedTextMask = 0;
            node.resolvedStates = states;
            node.styleResolved = true;
        }
        if (node.parentIndex != 0xFFFFFFFFu && node.parentIndex < nodeIndex) {
            const PropId inherited[] = {PropId::Font, PropId::FontSize, PropId::Fg, PropId::Align, PropId::LetterSpacing};
            const PropTable& parentStyle = m_nodes[node.parentIndex].style;
            for (u8 i = 0; i < std::size(inherited); ++i) {
                const PropId property = inherited[i];
                const u64 bit = u64{1} << static_cast<int>(property);
                const bool wasInherited = (node.inheritedTextMask & u8(1u << i)) != 0;
                if (wasInherited) {
                    if (parentStyle.has(property)) node.style.v[static_cast<int>(property)] = parentStyle.get(property);
                    else { node.style.mask &= ~bit; node.inheritedTextMask &= u8(~(1u << i)); }
                } else if (!node.style.has(property) && parentStyle.has(property)) {
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

        Color bg = color(node.style, PropId::Bg, Color{0, 0, 0, 0});
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
            shape.border = Color{0, 0, 0, 0};
            if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
                const Atom& borderColor = node.style.get(PropId::Border).atoms[2];
                if (borderColor.kind == AtomKind::Color) shape.border = Color::fromRGBA8(borderColor.bits);
                const Atom& borderStyle = node.style.get(PropId::Border).atoms[1];
                shape.dashed = borderStyle.kind == AtomKind::Ident && m_strings->view(borderStyle.str()) == "dashed";
            }
            if (node.style.has(PropId::Shadow)) {
                const StyleValue& shadow = node.style.get(PropId::Shadow);
                ShadowStyle effect;
                effect.offset = {number(shadow, 0), number(shadow, 1)};
                effect.blur = number(shadow, 2, 8.f);
                effect.spread = number(shadow, 3);
                effect.color = shadow.count > 4 && shadow.atoms[4].kind == AtomKind::Color
                    ? Color::fromRGBA8(shadow.atoms[4].bits) : Color::fromRGBA8(0x00000055u);
                if (node.style.has(PropId::Radius)) effect.radius = {number(node.style.get(PropId::Radius), 0), number(node.style.get(PropId::Radius), 1), number(node.style.get(PropId::Radius), 2), number(node.style.get(PropId::Radius), 3)};
                painter.shadow(rect, effect);
            }
            if (node.style.has(PropId::Radius)) {
                const StyleValue& radius = node.style.get(PropId::Radius);
                shape.radius = {number(radius, 0), number(radius, 1), number(radius, 2), number(radius, 3)};
            }
            if (shape.fill.a > 0 || (shape.borderWidth > 0 && shape.border.a > 0)) painter.roundRect(rect, shape);
        }
        if (node.checkItem || node.checkBox) {
            const f32 boxSize = std::min(18.f, std::max(12.f, rect.h - 8.f));
            const Rect box{rect.x + 2.f, rect.y + (rect.h - boxSize) * 0.5f, boxSize, boxSize};
            RectStyle check;
            check.fill = node.checked ? color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu)) : Color{0, 0, 0, 0};
            check.border = Color::fromRGBA8(0x94A3B8FFu); check.borderWidth = 1.f;
            if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
                const Atom& borderColor = node.style.get(PropId::Border).atoms[2];
                if (borderColor.kind == AtomKind::Color) check.border = Color::fromRGBA8(borderColor.bits);
            }
            check.radius = CornerRadii::all(3.f);
            painter.roundRect(box, check);
            if (node.checked) {
                const Color mark = Color::fromRGBA8(0x0F172AFFu);
                painter.line({box.x + 4.f, box.y + boxSize * 0.52f}, {box.x + boxSize * 0.43f, box.y + boxSize - 4.f}, 2.f, mark);
                painter.line({box.x + boxSize * 0.43f, box.y + boxSize - 4.f}, {box.x + boxSize - 3.f, box.y + 4.f}, 2.f, mark);
            }
        }
        if (node.dropdown) {
            const f32 cx = rect.right() - 15.f, cy = rect.y + rect.h * 0.5f;
            const Color mark = color(node.style, PropId::Fg, Color::fromRGBA8(0xF2F6FFFFu));
            painter.line({cx - 4.f, cy - 2.f}, {cx, cy + 2.f}, 1.5f, mark);
            painter.line({cx, cy + 2.f}, {cx + 4.f, cy - 2.f}, 1.5f, mark);
        }
        if (node.colorPicker) {
            const f32 side = std::min(22.f, std::max(12.f, rect.h - 8.f));
            const Rect swatch{rect.right() - side - 5.f, rect.y + (rect.h - side) * 0.5f, side, side};
            painter.fillRect(swatch, node.pickerColor, CornerRadii::all(3.f));
        }
        if (node.progressBar || node.slider) {
            const Color track = bg.a > 0 ? bg : Color::fromRGBA8(0x334155FFu);
            const Color fill = color(node.style, PropId::Fg, Color::fromRGBA8(0x60A5FAFFu));
            const f32 fraction = node.maxValue > node.minValue
                ? std::clamp((node.value - node.minValue) / (node.maxValue - node.minValue), 0.f, 1.f) : 0.f;
            if (node.progressBar) {
                const f32 thickness = std::max(4.f, rect.h * 0.35f);
                const Rect trackRect{rect.x, rect.y + (rect.h - thickness) * 0.5f, rect.w, thickness};
                painter.fillRect(trackRect, track, CornerRadii::all(thickness * 0.5f));
                painter.fillRect({trackRect.x, trackRect.y, trackRect.w * fraction, trackRect.h}, fill, CornerRadii::all(thickness * 0.5f));
            } else {
                const f32 thickness = std::max(3.f, rect.h * 0.16f);
                const f32 centerY = rect.y + rect.h * 0.5f;
                painter.fillRect({rect.x, centerY - thickness * 0.5f, rect.w, thickness}, track, CornerRadii::all(thickness * 0.5f));
                painter.fillRect({rect.x, centerY - thickness * 0.5f, rect.w * fraction, thickness}, fill, CornerRadii::all(thickness * 0.5f));
                const f32 diameter = std::min(rect.h, 22.f);
                painter.fillRect({rect.x + (rect.w - diameter) * fraction, centerY - diameter * 0.5f, diameter, diameter}, fill, CornerRadii::all(diameter * 0.5f));
            }
        }
        if (node.progressBar || node.slider) { painter.popOpacity(); continue; }
        const bool placeholderText = (node.textBox || node.textArea) && node.text.empty() && !node.placeholder.empty();
        std::string passwordMask;
        std::string_view displayText = placeholderText ? std::string_view(node.placeholder) : std::string_view(node.text);
        if (node.password && !node.text.empty()) {
            for (std::size_t i = 0; i < node.text.size(); i = nextUtf8(node.text, i)) appendUtf8(passwordMask, 0x2022u);
            displayText = passwordMask;
        }
        if (displayText.empty()) { painter.popOpacity(); continue; }

        if (!node.font.valid()) { painter.popOpacity(); continue; }
        const Color fg = color(node.style, PropId::Fg, Color::fromRGBA8(0xF5F5F5FFu));
        const f32 fontSize = node.style.has(PropId::FontSize) ? number(node.style.get(PropId::FontSize), 0, 16) : 16.f;
        const f32 rasterSize = fontSize * m_dpiScale;
        const f32 letterSpacing = node.style.has(PropId::LetterSpacing) ? number(node.style.get(PropId::LetterSpacing), 0) : 0.f;
        const Edges textPadding = node.hasXuiPadding ? node.xuiPadding : node.style.has(PropId::Pad)
            ? Edges{number(node.style.get(PropId::Pad), 3), number(node.style.get(PropId::Pad), 0),
                    number(node.style.get(PropId::Pad), 1), number(node.style.get(PropId::Pad), 2)} : Edges{};
        const f32 padTop = textPadding.t;
        const f32 padBottom = textPadding.b;
        const f32 textLeft = textPadding.l;
        const f32 textRight = textPadding.r + ((node.dropdown || node.colorPicker) ? 30.f : 0.f);
        const f32 checkOffset = (node.checkItem || node.checkBox) ? 26.f : 0.f;
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
            m_glyphScratch.push_back({Rect{(shaped.x + glyph.offset.x) * scale + letterSpacing * static_cast<f32>(glyphIndex), (shaped.y + glyph.offset.y) * scale, glyph.width * scale, glyph.height * scale}, glyph.uv});
        }
        f32 x = rect.x;
        const f32 padLeft = textPadding.l;
        const f32 padRight = textPadding.r;
        const f32 runWidth = run.width / m_dpiScale + letterSpacing * static_cast<f32>(run.glyphs.size() > 0 ? run.glyphs.size() - 1 : 0);
        const f32 runHeight = run.height / m_dpiScale;
        const f32 baseline = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f) + font->baselineAt(rasterSize) / m_dpiScale;
        const Align textAlign = node.style.has(PropId::Align) ? align(node.style.get(PropId::Align).atoms[0]) : Align::Start;
        const bool textCenter = node.button || (!node.checkItem && textAlign == Align::Center);
        if (node.checkItem || node.checkBox) x += textLeft + checkOffset;
        else if (node.dropdown) x += padLeft;
        else if (node.colorPicker) x += padLeft;
        else if (textCenter) x += textPadding.l + std::max(0.f, (rect.w - textPadding.l - textPadding.r - runWidth) * 0.5f);
        else if (!node.checkItem && textAlign == Align::End) x += rect.w - runWidth - padRight;
        else x += padLeft;
        if (node.textBox && !placeholderText && node.focused) {
            const std::string_view prefix(node.text.data(), std::min(node.caret, node.text.size()));
            const TextLayout& caretRun = m_text->layout(node.font, prefix, rasterSize, 0.f);
            const f32 caretPosition = caretRun.width / m_dpiScale;
            x = rect.x + textLeft - std::clamp(caretPosition - contentWidth, 0.f, std::max(0.f, runWidth - contentWidth));
        } else if (node.textArea || node.textBox) x = rect.x + textLeft;
        else if (x + runWidth > rect.right() - padRight) x = rect.right() - padRight - runWidth;
        painter.pushClip({rect.x + textLeft + checkOffset, rect.y + padTop, std::max(0.f, rect.w - textLeft - textRight - checkOffset), std::max(0.f, rect.h - padTop - padBottom)});
        if ((node.textBox || node.textArea) && node.focused && node.caret != node.selectionAnchor && !placeholderText) {
            const std::size_t begin = std::min(node.caret, node.selectionAnchor);
            const std::size_t end = std::max(node.caret, node.selectionAnchor);
            auto caretAt = [&](std::size_t byteOffset) {
                const TextLayout& prefix = m_text->layout(node.font, std::string_view(node.text).substr(0, byteOffset), rasterSize, node.textArea ? contentWidth * m_dpiScale : 0.f);
                const u32 line = prefix.lineStarts.empty() ? 0u : static_cast<u32>(prefix.lineStarts.size() - 1);
                const u32 lineFirst = prefix.lineStarts.empty() ? 0u : prefix.lineStarts.back();
                const f32 cx = prefix.glyphs.size() > lineFirst
                    ? (prefix.glyphs.back().x + prefix.glyphs.back().advance) / m_dpiScale : 0.f;
                return std::pair<u32, f32>{line, cx};
            };
            const auto [firstLine, firstX] = caretAt(begin);
            const auto [lastLine, lastX] = caretAt(end);
            const f32 lineHeight = std::max(fontSize, font->lineHeight(rasterSize) / m_dpiScale);
            const f32 textTop = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f);
            for (u32 line = firstLine; line <= lastLine; ++line) {
                const f32 left = line == firstLine ? firstX : 0.f;
                const f32 right = line == lastLine ? lastX : contentWidth;
                painter.fillRect({x + left, textTop + f32(line) * lineHeight, std::max(2.f, right - left), lineHeight}, Color::fromRGBA8(0x4F7DE080u));
            }
        }
        painter.glyphRun(atlas->texture(), m_glyphScratch.data(), static_cast<u32>(m_glyphScratch.size()), {x, baseline}, fg);
        if ((node.textBox || node.textArea) && node.focused && !node.disabled && !node.readOnly && !placeholderText) {
            const std::string_view prefix(node.text.data(), std::min(node.caret, node.text.size()));
            const TextLayout& caretRun = m_text->layout(node.font, prefix, rasterSize, node.textArea ? contentWidth * m_dpiScale : 0.f);
            f32 caretX = 0.f, caretY = 0.f;
            if (!caretRun.glyphs.empty()) {
                const u32 lineStart = caretRun.lineStarts.empty() ? 0 : caretRun.lineStarts.back();
                const ShapedGlyph& previous = caretRun.glyphs.back();
                caretX = (previous.x + previous.advance) / m_dpiScale;
                caretY = f32(caretRun.lineStarts.size() - 1) * caretRun.lineHeight / m_dpiScale;
                (void)lineStart;
            }
            if (node.textBox) {
                const f32 caretPosition = caretRun.width / m_dpiScale;
                const f32 offset = std::clamp(caretPosition - contentWidth, 0.f, std::max(0.f, runWidth - contentWidth));
                caretX = caretPosition - offset;
            }
            const f32 y = rect.y + padTop + std::max(0.f, (rect.h - padTop - padBottom - runHeight) * 0.5f) + caretY;
            painter.line({x + caretX, y + 1.f}, {x + caretX, y + std::max(2.f, fontSize - 1.f)}, 1.f, fg);
        }
        painter.popClip();
        painter.popOpacity();
    }

    // Tabs and popups render after the retained tree, so their chrome stays above page content.
    painter.setLayer(1);
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
            m_glyphScratch.push_back({{shaped.x * scale + glyph.offset.x * scale, shaped.y * scale + glyph.offset.y * scale, glyph.width * scale, glyph.height * scale}, glyph.uv});
        }
        const f32 width = run.width / m_dpiScale;
        const Align alignment = style.has(PropId::Align) ? align(style.get(PropId::Align).atoms[0]) : Align::Start;
        const f32 x = alignment == Align::Center ? rect.x + (rect.w - width) * 0.5f :
                      alignment == Align::End ? rect.right() - inset - width : rect.x + inset;
        const f32 y = rect.y + std::max(0.f, (rect.h - run.height / m_dpiScale) * 0.5f);
        painter.pushClip(rect);
        painter.glyphRun(m_text->atlas()->texture(), m_glyphScratch.data(), static_cast<u32>(m_glyphScratch.size()),
                         {x, y + font->baselineAt(size) / m_dpiScale}, color(style, PropId::Fg, fallback));
        painter.popClip();
    };
    for (u32 tabsIndex = 0; tabsIndex < m_nodes.size(); ++tabsIndex) {
        Node& tabs = m_nodes[tabsIndex];
        if (!tabs.tabs || !isNodeVisible(tabsIndex)) continue;
        const Rect bounds = m_layout.absRect(tabs.layout);
        const u32 count = m_layout.childCount(tabs.layout);
        if (!count) continue;
        u32 ordinal = 0;
        for (NodeId child = m_layout.firstChild(tabs.layout); child.valid(); child = m_layout.nextSibling(child), ++ordinal) {
            Node& tab = m_nodes[child.index];
            const Rect header{bounds.x + bounds.w * f32(ordinal) / f32(count), bounds.y,
                              bounds.w * f32(ordinal + 1) / f32(count) - bounds.w * f32(ordinal) / f32(count), 34.f};
            PropTable tabStyle;
            const StateMask state = stateBit(State::Resting) |
                (ordinal == tabs.selectedTab ? stateBit(State::Selected) : 0) |
                (header.contains(m_pointerPosition) ? stateBit(State::Hover) : 0) |
                (tabs.disabled ? stateBit(State::Disabled) : 0);
            m_styles->resolve(tab.styleSet, state, 0, tabStyle);
            const Color background = color(tabStyle, PropId::Bg,
                ordinal == tabs.selectedTab ? Color::fromRGBA8(0x24334FFFu) : Color::fromRGBA8(0x151E32FFu));
            painter.fillRect(header, background);
            if (tabStyle.has(PropId::Border) && tabStyle.get(PropId::Border).count >= 3) {
                const auto& border = tabStyle.get(PropId::Border);
                const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
                painter.roundRect(header, RectStyle{Color{0,0,0,0}, edge, number(border, 0), CornerRadii{}});
            }
            drawLabel(tab.tabTitle, header, tab.font, tabStyle, Color::fromRGBA8(0xDDE7F8FFu), 8.f, false);
        }
    }
    // Popups render after the retained tree, so they always remain on top of later siblings.
    for (Node& node : m_nodes) {
        if (!node.popupOpen || (!node.dropdown && !node.colorPicker)) continue;
        const Rect anchor = m_layout.absRect(node.layout);
        const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight,node.pickerWidth,node.pickerHeight);
        const Color panel = color(node.style, PropId::Bg, Color::fromRGBA8(0x151E32FFu));
        const Color foreground = color(node.style, PropId::Fg, Color::fromRGBA8(0xF2F6FFFFu));
        painter.fillRect(popup, panel, CornerRadii::all(5.f));
        if (node.style.has(PropId::Border) && node.style.get(PropId::Border).count >= 3) {
            const auto& border = node.style.get(PropId::Border);
            const Color edge = border.atoms[2].kind == AtomKind::Color ? Color::fromRGBA8(border.atoms[2].bits) : Color{};
            painter.roundRect(popup, RectStyle{Color{0,0,0,0}, edge, number(border, 0), CornerRadii::all(5.f)});
        }
        if (node.dropdown) {
            for (u32 i = 0; i < node.options.size(); ++i) {
                const Rect row{popup.x, popup.y + 28.f * i, popup.w, 28.f};
                if (i == node.selectedOption) painter.fillRect(row, foreground.withAlpha(0.12f), CornerRadii::all(2.f));
                if (static_cast<i32>(i) == node.hoveredOption) {
                    // Popup rows are virtual option targets, not descendants of the header.
                    // Use a row-local highlight so the header's :hover style cannot leak here.
                    painter.fillRect(row, foreground.withAlpha(0.16f), CornerRadii::all(2.f));
                }
                drawLabel(node.options[i], row, node.font, node.style, foreground, 8.f, false);
            }
        } else {
            constexpr u32 columns = 14, rows = 8;
            f32 hue = 0.f;
            const f32 hi = std::max({node.pickerColor.r,node.pickerColor.g,node.pickerColor.b}), lo = std::min({node.pickerColor.r,node.pickerColor.g,node.pickerColor.b}), delta = hi - lo;
            f32 saturation=hi>0.f?delta/hi:0.f, value=hi;
            if (delta > 0.f) {
                if (hi == node.pickerColor.r) hue = std::fmod((node.pickerColor.g-node.pickerColor.b)/delta,6.f)/6.f;
                else if (hi == node.pickerColor.g) hue = ((node.pickerColor.b-node.pickerColor.r)/delta+2.f)/6.f;
                else hue = ((node.pickerColor.r-node.pickerColor.g)/delta+4.f)/6.f;
                if (hue < 0.f) hue += 1.f;
            }
            if(node.pickerShowSaturationValue) for (u32 row = 0; row < rows; ++row) for (u32 col = 0; col < columns; ++col) {
                const f32 svSaturation = f32(col) / f32(columns - 1), svValue = 1.f - f32(row) / f32(rows - 1);
                const Rect cell{popup.x + popup.w * f32(col) / columns, popup.y + 4.f + 116.f * f32(row) / rows, popup.w / columns + 0.5f, 116.f / rows + 0.5f};
                painter.fillRect(cell, hsv(hue,svSaturation,svValue));
            }
            const Color indicator=color(node.style,PropId::Fg,Color::fromRGBA8(0xFFFFFFFFu));
            const f32 markerX=popup.x+std::clamp(saturation,0.f,1.f)*popup.w;
            const f32 markerY=popup.y+4.f+std::clamp(1.f-value,0.f,1.f)*116.f;
            if(node.pickerShowSaturationValue) painter.roundRect({markerX-5.f,markerY-5.f,10.f,10.f},RectStyle{Color{0,0,0,0},indicator,1.5f,CornerRadii::all(5.f)});
            constexpr u32 hues = 14;
            if(node.pickerShowHue) for (u32 col = 0; col < hues; ++col) {
                const Rect cell{popup.x + popup.w * f32(col) / hues, popup.y + 124.f, popup.w / hues + 0.5f, 18.f};
                painter.fillRect(cell, hsv(f32(col) / hues, 1.f, 1.f));
            }
            const f32 hueX=popup.x+std::clamp(hue,0.f,1.f)*popup.w;
            if(node.pickerShowHue) painter.fillRect({hueX-1.f,popup.y+123.f,2.f,20.f},indicator);
            if (node.pickerShowAlpha && node.pickerAlpha) {
                constexpr u32 alphaSteps=28;
                for(u32 step=0;step<alphaSteps;++step) {
                    Color sample=node.pickerColor; sample.a=f32(step)/f32(alphaSteps-1);
                    painter.fillRect({popup.x+f32(step)*popup.w/alphaSteps,popup.y+146.f,popup.w/alphaSteps+0.5f,10.f},sample);
                }
                painter.roundRect({popup.x,popup.y+146.f,popup.w,10.f},RectStyle{Color{0,0,0,0},foreground.withAlpha(0.4f),1.f,CornerRadii::all(3.f)});
            }
            painter.fillRect({popup.x+4.f,popup.y+162.f,18.f,10.f},node.pickerColor,CornerRadii::all(2.f));
            if (node.pickerShowFormatSwitcher) drawLabel(std::string(node.pickerFormat).append("  ▾"), {popup.x+27.f,popup.y+159.f,72.f,18.f},node.font,node.style,foreground,0.f,false);
            else drawLabel(node.pickerFormat, {popup.x+27.f,popup.y+159.f,72.f,18.f},node.font,node.style,foreground,0.f,false);
            if (node.pickerShowInputs) {
                const bool rgb = node.pickerFormat == "rgb" || node.pickerFormat == "hex";
                const char* labels = rgb ? (node.pickerAlpha ? "R       G       B       A" : "R       G       B") :
                    node.pickerFormat == "hsv" ? (node.pickerAlpha ? "H       S       V       A" : "H       S       V") :
                    (node.pickerAlpha ? "H       S       L       A" : "H       S       L");
                drawLabel(labels, {popup.x+5.f,popup.y+180.f,popup.w-10.f,13.f},node.font,node.style,foreground,0.f,false);
                drawLabel("HEX", {popup.x+5.f,popup.y+221.f,40.f,13.f},node.font,node.style,foreground,0.f,false);
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
            painter.roundRect(rect, RectStyle{Color{0,0,0,0}, edge, number(border,0), CornerRadii::all(radius)});
        }
        const f32 fontSize = editor.style.has(PropId::FontSize) ? number(editor.style.get(PropId::FontSize),0,12.f) : 12.f;
        if (editor.focused && editor.caret != editor.selectionAnchor && m_text && editor.font.valid()) {
            const std::size_t begin = std::min(editor.caret, editor.selectionAnchor), end = std::max(editor.caret, editor.selectionAnchor);
            const f32 x1 = m_text->layout(editor.font, std::string_view(editor.text).substr(0, begin), fontSize*m_dpiScale).width/m_dpiScale;
            const f32 x2 = m_text->layout(editor.font, std::string_view(editor.text).substr(0, end), fontSize*m_dpiScale).width/m_dpiScale;
            painter.fillRect({rect.x+5.f+x1,rect.y+3.f,std::max(1.f,x2-x1),rect.h-6.f},Color::fromRGBA8(0x4F7DE088u));
        }
        drawLabel(editor.text, rect, editor.font, editor.style, foreground, 5.f, false);
        if (editor.focused && !editor.disabled && !editor.readOnly && m_text && editor.font.valid()) {
            const f32 caretX = m_text->layout(editor.font, std::string_view(editor.text).substr(0, std::min(editor.caret, editor.text.size())), fontSize*m_dpiScale).width/m_dpiScale;
            painter.line({rect.x+5.f+caretX,rect.y+5.f},{rect.x+5.f+caretX,rect.bottom()-5.f},1.f,foreground);
        }
    }
    const bool popupOpen = m_tooltipVisible && std::any_of(m_nodes.begin(), m_nodes.end(), [](const Node& node) { return node.popupOpen; });
    if (m_tooltipVisible && !popupOpen && m_tooltipOwner < m_nodes.size()) {
        const Node& owner = m_nodes[m_tooltipOwner];
        PropTable tooltipStyle;
        m_styles->resolve(m_tooltipStyleSet, stateBit(State::Resting), 0, tooltipStyle);
        const FontHandle selectedTooltipFont = selectFont(tooltipStyle);
        const FontHandle tooltipFont = selectedTooltipFont.valid() ? selectedTooltipFont : owner.font;
        const f32 fontSize = tooltipStyle.has(PropId::FontSize) ? number(tooltipStyle.get(PropId::FontSize), 0, 13.f) : 13.f;
        const f32 maxWidth = std::min(360.f, std::max(80.f, m_lastLayoutWidth - 16.f));
        const TextLayout& measured = m_text->layout(tooltipFont, owner.tooltipText, fontSize * m_dpiScale, (maxWidth - 20.f) * m_dpiScale);
        const f32 padding = tooltipStyle.has(PropId::Pad) ? number(tooltipStyle.get(PropId::Pad), 0, 7.f) : 7.f;
        const f32 width = std::min(maxWidth, measured.width / m_dpiScale + padding * 2.f);
        const f32 height = measured.height / m_dpiScale + padding * 2.f;
        const Rect anchor = m_layout.absRect(owner.layout);
        Rect bubble{anchor.x, anchor.bottom() + 6.f, width, height};
        if (bubble.bottom() > m_lastLayoutHeight) bubble.y = anchor.y - height - 6.f;
        bubble.x = std::clamp(bubble.x, 4.f, std::max(4.f, m_lastLayoutWidth - width - 4.f));
        bubble.y = std::clamp(bubble.y, 4.f, std::max(4.f, m_lastLayoutHeight - height - 4.f));
        painter.fillRect(bubble, color(tooltipStyle, PropId::Bg, Color::fromRGBA8(0x202A40F5u)),
                         CornerRadii::all(tooltipStyle.has(PropId::Radius) ? number(tooltipStyle.get(PropId::Radius), 0, 5.f) : 5.f));
        drawLabel(owner.tooltipText, {bubble.x + padding, bubble.y + padding, bubble.w - padding * 2.f, bubble.h - padding * 2.f},
                  tooltipFont, tooltipStyle, Color::fromRGBA8(0xF2F6FFFFu), 0.f, true);
    }
}

bool UiRuntime::pointerMove(Vec2 point) {
    m_pointerPosition = point;
    bool changed = false;
    const u32 target = hitTestNode(point);
    u32 popupTarget = 0xFFFFFFFFu;
    for (std::size_t i = m_nodes.size(); i > 0; --i) {
        const u32 index = static_cast<u32>(i - 1);
        const Node& node = m_nodes[index];
        if (!node.popupOpen || (!node.dropdown && !node.colorPicker) || !isNodeVisible(index)) continue;
        if (popupBounds(m_layout.absRect(node.layout), node.dropdown, node.options.size(), m_lastLayoutHeight,node.pickerWidth,node.pickerHeight).contains(point)) {
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
        const Rect popup = popupBounds(m_layout.absRect(node.layout), true, node.options.size(), m_lastLayoutHeight,node.pickerWidth,node.pickerHeight);
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
    return changed;
}

CursorShape UiRuntime::cursorAt(Vec2 point) const {
    const u32 target = hitTestNode(point);
    if (target < m_nodes.size()) {
        const Node& node = m_nodes[target];
        if ((node.textBox || node.textArea) && !node.disabled && !node.readOnly) return CursorShape::IBeam;
    }
    return CursorShape::Arrow;
}

bool UiRuntime::pointerDown(Vec2 point) {
    m_pointerPosition = point;
    bool changed = false;
    // Any click dismisses an already visible tip. The dismissed owner stays suppressed
    // until it loses both hover and focus, preventing the tip from reopening over a popup.
    if (m_tooltipOwner < m_nodes.size() && m_nodes[m_tooltipOwner].tooltipHideOnClick) {
        Node& owner = m_nodes[m_tooltipOwner];
        owner.tooltipDismissed = true;
        addDamage({0, 0, m_lastLayoutWidth, m_lastLayoutHeight});
        m_tooltipOwner = 0xFFFFFFFFu;
        m_tooltipVisible = false;
        m_tooltipDeadline = m_tooltipHideDeadline = {};
        changed = true;
    }
    // Popup surfaces are painted above the tree; the last painted popup gets the event.
    for (std::size_t i = m_nodes.size(); i > 0; --i) {
        const u32 index = static_cast<u32>(i - 1);
        Node& node = m_nodes[index];
        if (!node.popupOpen || (!node.dropdown && !node.colorPicker) || !isNodeVisible(index)) continue;
        const Rect anchor = m_layout.absRect(node.layout);
        const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight,node.pickerWidth,node.pickerHeight);
        if (!popup.contains(point)) continue;
        if (node.dropdown) {
            if (node.options.empty()) return true;
            const u32 selected = std::min<u32>(static_cast<u32>((point.y - popup.y) / 28.f), static_cast<u32>(node.options.size() - 1));
            node.selectedOption = selected;
            node.text = node.options[selected];
            node.popupOpen = false;
            m_layout.markContentChanged(node.layout);
            addDamage(anchor); addDamage(popup);
            if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
        } else {
            const f32 localY = point.y-popup.y;
            if (localY >= 160.f && localY < 182.f) {
                if (node.pickerShowFormatSwitcher && point.x < popup.x+76.f && !node.pickerFormats.empty()) {
                    auto format=std::find(node.pickerFormats.begin(),node.pickerFormats.end(),node.pickerFormat);
                    const std::size_t next=format==node.pickerFormats.end()?0:(static_cast<std::size_t>(format-node.pickerFormats.begin())+1)%node.pickerFormats.size();
                    node.pickerFormat=node.pickerFormats[next]; node.text=colorText(node.pickerColor,node.pickerFormat,node.pickerAlpha,node.pickerPrecision);
                    syncPickerEditors(index); addDamage(popup); addDamage(anchor);
                }
            } else if (node.pickerShowInputs) {
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
            if (localY >= 146.f && localY < 158.f && node.pickerShowAlpha && node.pickerAlpha) {
                node.pressed=true; (void)updateColorPicker(node,point);
            } else if (localY < 124.f) {
                node.pressed=true; (void)updateColorPicker(node,point);
            } else if (localY >= 124.f && localY < 146.f) {
                node.pressed=true; (void)updateColorPicker(node,point);
            }
        }
        return true;
    }
    // An outside click dismisses the popup, then may activate the newly exposed target.
    for (Node& node : m_nodes) if (node.popupOpen && (node.dropdown || node.colorPicker)) {
        const Rect anchor = m_layout.absRect(node.layout);
        const Rect popup = popupBounds(anchor, node.dropdown, node.options.size(), m_lastLayoutHeight,node.pickerWidth,node.pickerHeight);
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
        node.focused = focus;
        if (!node.hovered && !node.focused) node.tooltipDismissed = false;
        if ((node.textBox || node.textArea) && focus) node.caret = node.text.size();
        if ((node.textBox || node.textArea) && focus) node.selectionAnchor = node.caret;
    }
    refreshTooltipTarget();

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
            target->dropdown || target->colorPicker || target->textBox || target->textArea;
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
            if (target->dropdown || target->colorPicker) {
                for (Node& node : m_nodes) if (&node != target && node.popupOpen) node.popupOpen = false;
                target->popupOpen = true;
                addDamage(popupBounds(m_layout.absRect(target->layout), target->dropdown, target->options.size(), m_lastLayoutHeight,target->pickerWidth,target->pickerHeight));
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
    }
    if (activated && (activated->checkItem || activated->checkBox)) {
        activated->checked = !activated->checked;
        changed = true;
        addDamage(m_layout.absRect(activated->layout));
    }
    if (activated && (activated->button || activated->checkItem || activated->checkBox) &&
        activated->pressHandler.v && m_eventFn)
        m_eventFn(m_eventContext, m_strings->view(activated->pressHandler));
    m_splitDrag = {};
    m_dragSplit = {};
    return changed || activated != nullptr;
}

bool UiRuntime::keyDown(Key key) { return keyDown(key, 0, nullptr); }

bool UiRuntime::keyDown(Key key, u8 mods, Platform* platform) {
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
            } else if (key == Key::Backspace && field.caret) {
                const std::size_t start = previousUtf8(field.text, field.caret);
                field.text.erase(start, field.caret - start); field.caret = field.selectionAnchor = start; edited = true;
            } else if ((key == Key::Delete || (control && key == static_cast<Key>('X'))) && field.caret < field.text.size()) {
                const std::size_t end = nextUtf8(field.text, field.caret);
                field.text.erase(field.caret, end - field.caret); field.selectionAnchor = field.caret; edited = true;
            }
        } else if (key == Key::Left && field.caret) {
            field.caret = previousUtf8(field.text, field.caret);
            if (!shift) field.selectionAnchor = field.caret;
        } else if (key == Key::Right && field.caret < field.text.size()) {
            field.caret = nextUtf8(field.text, field.caret);
            if (!shift) field.selectionAnchor = field.caret;
        } else if (key == Key::Home) {
            const std::size_t lineBreak = field.textArea && field.caret ? field.text.rfind('\n', field.caret - 1) : std::string::npos;
            field.caret = lineBreak == std::string::npos ? 0 : lineBreak + 1;
            if (!shift) field.selectionAnchor = field.caret;
        } else if (key == Key::End) {
            const std::size_t end = field.text.find('\n', field.caret);
            field.caret = end == std::string::npos ? field.text.size() : end;
            if (!shift) field.selectionAnchor = field.caret;
        } else if (!field.readOnly && field.textArea && key == Key::Enter) {
            const std::size_t countAfterSelection = codepointCount(field.text) - (hasSelection ? codepointCount(std::string_view(field.text).substr(selectionBegin, selectionEnd - selectionBegin)) : 0u);
            if (!field.maxLength || countAfterSelection < field.maxLength) {
                if (hasSelection) field.text.erase(selectionBegin, selectionEnd - selectionBegin);
                field.text.insert(selectionBegin, 1, '\n');
                field.caret = field.selectionAnchor = selectionBegin + 1;
                edited = true;
            }
        } else break;
        if (edited) {
            field.text = truncateUtf8(field.text, field.maxLength);
            field.caret = std::min(field.caret, field.text.size());
            field.selectionAnchor = std::min(field.selectionAnchor, field.text.size());
            m_layout.markContentChanged(field.layout);
            if (field.pickerEditor) updatePickerFromEditor(field);
            else if (field.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(field.pressHandler));
        }
        if (edited || oldCaret != field.caret) { addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout)); return true; }
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
        }
        refreshTooltipTarget();
        return true;
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
            const f32 before=node.value;
            node.value = std::clamp(node.value + direction * step, node.minValue, node.maxValue);
            if(node.value==before) return false;
            addDamage(m_layout.absRect(node.layout));
            if (node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
            return true;
        }
    }
    if (key == Key::Escape) {
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
    if (key != Key::Enter && key != Key::Space) return false;
    for (u32 index : m_interactiveNodes) {
        Node& node = m_nodes[index];
        if (!node.focused || node.disabled) continue;
        if (node.checkItem || node.checkBox) node.checked = !node.checked;
        if (node.dropdown || node.colorPicker) node.popupOpen = !node.popupOpen;
        addDamage(m_layout.absRect(node.layout));
        if (!node.dropdown && !node.colorPicker && node.pressHandler.v && m_eventFn) m_eventFn(m_eventContext, m_strings->view(node.pressHandler));
        return true;
    }
    return false;
}

bool UiRuntime::textInput(u32 codepoint) {
    for (u32 index : m_interactiveNodes) {
        Node& field = m_nodes[index];
        if (!(field.textBox || field.textArea) || !field.focused || field.disabled || field.readOnly || !codepoint || codepoint == '\r' || codepoint == '\n') continue;
        const std::size_t begin = std::min(field.caret, field.selectionAnchor);
        const std::size_t end = std::max(field.caret, field.selectionAnchor);
        const u32 selectedCount = codepointCount(std::string_view(field.text).substr(begin, end - begin));
        if (field.maxLength && codepointCount(field.text) - selectedCount >= field.maxLength) return false;
        std::string encoded; appendUtf8(encoded, codepoint);
        if (end > begin) field.text.erase(begin, end - begin);
        field.text.insert(begin, encoded);
        field.caret = field.selectionAnchor = begin + encoded.size();
        m_layout.markContentChanged(field.layout); addDamage(field.pickerEditor ? pickerEditorRect(field) : m_layout.absRect(field.layout));
        if (field.pickerEditor) updatePickerFromEditor(field);
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
    const Rect popup = popupBounds(anchor, false, 0, m_lastLayoutHeight,node.pickerWidth,node.pickerHeight);
    if (!popup.contains(point)) return false;
    f32 hue = 0.f, saturation = 0.f, value = 0.f;
    const f32 hi = std::max({node.pickerColor.r, node.pickerColor.g, node.pickerColor.b});
    const f32 lo = std::min({node.pickerColor.r, node.pickerColor.g, node.pickerColor.b});
    const f32 delta = hi - lo; value = hi; saturation = hi > 0.f ? delta / hi : 0.f;
    if (delta > 0.f) {
        if (hi == node.pickerColor.r) hue = std::fmod((node.pickerColor.g - node.pickerColor.b) / delta, 6.f) / 6.f;
        else if (hi == node.pickerColor.g) hue = ((node.pickerColor.b - node.pickerColor.r) / delta + 2.f) / 6.f;
        else hue = ((node.pickerColor.r - node.pickerColor.g) / delta + 4.f) / 6.f;
        if (hue < 0.f) hue += 1.f;
    }
    if (point.y >= popup.y + 158.f) return false;
    if (point.y >= popup.y + 146.f && (!node.pickerShowAlpha || !node.pickerAlpha)) return false;
    Color selected=node.pickerColor;
    if (point.y >= popup.y + 146.f && node.pickerShowAlpha && node.pickerAlpha)
        selected.a=std::clamp((point.x-popup.x)/popup.w,0.f,1.f);
    else if (point.y >= popup.y + 124.f) {
        if(!node.pickerShowHue) return false;
        hue = std::clamp((point.x - popup.x) / popup.w, 0.f, 1.f);
        selected=hsv(hue,saturation,value).withAlpha(node.pickerAlpha?node.pickerColor.a:1.f);
    } else {
        if(!node.pickerShowSaturationValue) return false;
        saturation = std::clamp((point.x - popup.x) / popup.w, 0.f, 1.f); value = 1.f - std::clamp((point.y - popup.y) / 118.f, 0.f, 1.f);
        selected=hsv(hue,saturation,value).withAlpha(node.pickerAlpha?node.pickerColor.a:1.f);
    }
    if (selected == node.pickerColor) return false;
    node.pickerColor = selected;
    node.text = colorText(selected,node.pickerFormat,node.pickerAlpha,node.pickerPrecision);
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
    const Rect popup = popupBounds(m_layout.absRect(owner.layout), false, 0, m_lastLayoutHeight, owner.pickerWidth, owner.pickerHeight);
    if (editor.pickerSlot < 4) {
        const u32 count = owner.pickerAlpha ? 4u : 3u;
        const f32 gap = 4.f, left = 5.f, width = (popup.w - left * 2.f - gap * (count - 1)) / count;
        return {popup.x + left + editor.pickerSlot * (width + gap), popup.y + 194.f, width, 24.f};
    }
    return {popup.x + 5.f, popup.y + 235.f, popup.w - 10.f, 24.f};
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
    } else {
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

u32 UiRuntime::hitTestNode(Vec2 point) const {
    if (point.x < 0.f || point.y < 0.f || point.x >= m_lastLayoutWidth || point.y >= m_lastLayoutHeight)
        return 0xFFFFFFFFu;
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
        if (!isNodeVisible(index)) continue;
        if (node.fpsCounter && !m_fpsEnabled) continue;
        if (node.pickerEditor) continue;
        const Rect rect = m_layout.absRect(node.layout);
        if (rect.empty() || !rect.contains(point)) continue;

        const bool interactive = node.button || node.checkItem || node.checkBox || node.slider || node.dropdown ||
            node.colorPicker || node.textBox || node.textArea || node.tabs;
        const bool structuralBlocker = typeIs(node.type, *m_strings, "Window") || typeIs(node.type, *m_strings, "Text") ||
            typeIs(node.type, *m_strings, "Panel") || typeIs(node.type, *m_strings, "Split") ||
            typeIs(node.type, *m_strings, "CheckList") || typeIs(node.type, *m_strings, "Tab") ||
            node.progressBar || node.fpsCounter;
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

bool UiRuntime::isNodeVisible(u32 index) const {
    if (index >= m_nodes.size()) return false;
    if (m_nodes[index].pickerEditor) {
        const u32 owner = m_nodes[index].pickerOwner;
        return owner < m_nodes.size() && m_nodes[owner].popupOpen && isNodeVisible(owner) &&
            (m_nodes[index].pickerSlot != 3 || m_nodes[owner].pickerAlpha);
    }
    u32 child = index;
    while (m_nodes[child].parentIndex != 0xFFFFFFFFu) {
        const u32 parent = m_nodes[child].parentIndex;
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
        } else return;
    }
    if (m_tooltipOwner != 0xFFFFFFFFu) addDamage({0, 0, m_lastLayoutWidth, m_lastLayoutHeight});
    m_tooltipOwner = next;
    m_tooltipVisible = false;
    m_tooltipHideDeadline = {};
    m_tooltipDeadline = next == 0xFFFFFFFFu ? std::chrono::steady_clock::time_point{} : now + std::chrono::milliseconds(m_nodes[next].tooltipShowDelayMs);
}

bool UiRuntime::updateTimers(std::chrono::steady_clock::time_point now) {
    bool redraw = false;
    if (m_tooltipOwner != 0xFFFFFFFFu && !m_tooltipVisible && now >= m_tooltipDeadline) {
        m_tooltipVisible = true;
        addDamage({0, 0, m_lastLayoutWidth, m_lastLayoutHeight});
        redraw = true;
    }
    if (m_tooltipVisible && m_tooltipHideDeadline.time_since_epoch().count() != 0 && now >= m_tooltipHideDeadline) {
        addDamage({0, 0, m_lastLayoutWidth, m_lastLayoutHeight});
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
    return delay;
}

Color UiRuntime::controlColor(std::string_view id) const {
    for (const Node& node : m_nodes) if (node.colorPicker && node.id.v && m_strings->view(node.id) == id) return node.pickerColor;
    return {};
}

std::string_view UiRuntime::controlText(std::string_view id) const {
    for (const Node& node : m_nodes) if ((node.dropdown || node.colorPicker || node.textBox || node.textArea || typeIs(node.type,*m_strings,"Text")) && node.id.v && m_strings->view(node.id) == id) return node.text;
    return {};
}

std::optional<UiValue> UiRuntime::value(std::string_view id) const {
    for (const Node& node : m_nodes) {
        if (!node.id.v || m_strings->view(node.id) != id) continue;
        if (node.checkItem || node.checkBox) return UiValue{node.checked};
        if (node.slider) return UiValue{static_cast<f64>(node.value)};
        if (node.dropdown || node.textBox || node.textArea || typeIs(node.type,*m_strings,"Text")) return UiValue{node.text};
        if (node.colorPicker) return UiValue{node.pickerColor};
        if (node.tabs) return UiValue{node.selectedTab};
    }
    return std::nullopt;
}

bool UiRuntime::setValue(std::string_view id, const UiValue& valueToSet, bool notify) {
    for (Node& node : m_nodes) {
        if (!node.id.v || m_strings->view(node.id) != id) continue;
        bool changed = false;
        if (node.checkItem || node.checkBox) {
            const bool* value = std::get_if<bool>(&valueToSet); if (!value) return false;
            changed = node.checked != *value; node.checked = *value;
        } else if (node.slider) {
            const f64* value = std::get_if<f64>(&valueToSet); if (!value) return false;
            const f32 clamped = std::clamp(static_cast<f32>(*value), node.minValue, node.maxValue);
            changed = node.value != clamped; node.value = clamped;
        } else if (node.dropdown) {
            const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
            const auto found = std::find(node.options.begin(), node.options.end(), *value); if (found == node.options.end()) return false;
            const u32 selected = static_cast<u32>(found - node.options.begin());
            changed = node.selectedOption != selected; node.selectedOption = selected; node.text = *value;
            if (changed) m_layout.markContentChanged(node.layout);
        } else if (node.textBox || node.textArea || typeIs(node.type,*m_strings,"Text")) {
            const std::string* value = std::get_if<std::string>(&valueToSet); if (!value) return false;
            std::string next = truncateUtf8(*value, node.maxLength);
            changed = node.text != next; node.text = std::move(next); node.caret = node.selectionAnchor = node.text.size();
            if (changed) m_layout.markContentChanged(node.layout);
        } else if (node.colorPicker) {
            Color next;
            if (const Color* colorValue = std::get_if<Color>(&valueToSet)) next=*colorValue;
            else if (const std::string* stringValue = std::get_if<std::string>(&valueToSet)) { if(!parseColorText(*stringValue,next)) return false; }
            else return false;
            if(!node.pickerAlpha) next.a=1.f;
            changed = node.pickerColor != next; node.pickerColor = next; node.text = colorText(next,node.pickerFormat,node.pickerAlpha,node.pickerPrecision);
            if (changed) syncPickerEditors(static_cast<u32>(&node - m_nodes.data()));
            if (changed) m_layout.markContentChanged(node.layout);
        } else if (node.tabs) {
            const u32* value = std::get_if<u32>(&valueToSet); if (!value) return false;
            const u32 count = m_layout.childCount(node.layout); if (!count) return false;
            const u32 selected = std::min(*value, count - 1); changed = node.selectedTab != selected; node.selectedTab = selected;
            if (changed) for (u32 i = 0; i < m_nodes.size(); ++i) if (m_nodes[i].focused && !isNodeVisible(i)) m_nodes[i].focused = false;
        } else return false;
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
    if (!runtimeNode || runtimeNode->text.empty() || !m_text || !runtimeNode->font.valid()) return {};
    f32 fontSize = runtimeNode->style.has(PropId::FontSize) ? number(runtimeNode->style.get(PropId::FontSize), 0, 16) : 16.f;
    const f32 wrapWidth = constraint.fw ? constraint.w : 0.f;
    const TextLayout& run = m_text->layout(runtimeNode->font, runtimeNode->text, fontSize, wrapWidth);
    const f32 spacing = runtimeNode->style.has(PropId::LetterSpacing) ? number(runtimeNode->style.get(PropId::LetterSpacing), 0) : 0.f;
    return {run.width + spacing * static_cast<f32>(run.glyphs.size() > 0 ? run.glyphs.size() - 1 : 0), run.height};
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
