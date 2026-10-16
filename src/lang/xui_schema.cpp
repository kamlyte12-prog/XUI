#include <xui/lang/xui.hpp>
#include <algorithm>
namespace xui {

	void XuiSchema::addWidget(std::string type, std::vector<std::string> ctor, std::vector<std::string> props) {
		m_widgets[std::move(type)] = Widget{ std::move(ctor), std::move(props) };
	}
	const XuiSchema::Widget* XuiSchema::find(std::string_view type) const {
		auto it = m_widgets.find(std::string(type));
		return it == m_widgets.end() ? nullptr : &it->second;
	}
	std::vector<std::string_view> XuiSchema::widgetNames() const {
		std::vector<std::string_view> v; for (const auto& [k, w] : m_widgets) { (void)w; v.push_back(k); }
		std::sort(v.begin(), v.end()); return v;
	}
	const std::vector<std::string_view>& XuiSchema::genericProps() {
		static const std::vector<std::string_view> g = { "id", "class", "key", "slot", "grow", "shrink", "visible", "disabled", "tooltip",
			"tooltipShowDelay", "tooltipHideDelay", "tooltipTrigger", "tooltipHideOnClick", "tooltipDisabled",
			"align", "justify", "position", "left", "top", "right", "bottom", "pad", "margin", "size", "minSize", "maxSize", "width", "height",
			"minWidth", "maxWidth", "minHeight", "maxHeight", "opacity" };
		return g;
	}

	// Core subset of the standard library. Full property sets are owned by the widget library (M7), which
	// re-registers each widget with its complete documented list; unknown names here are what the validator rejects.
	XuiSchema XuiSchema::withCoreWidgets() {
		XuiSchema s;
		auto W = [&](const char* t, std::vector<std::string> ctor, std::vector<std::string> props) { s.addWidget(t, std::move(ctor), std::move(props)); };
		W("Window", { "title", "size" }, { "resizable" });
		W("Row", { "gap" }, { "wrap" });          W("Column", { "gap" }, { "wrap" });
		W("Grid", { "columns" }, { "rows", "gap" });  W("Stack", {}, {});
		W("Split", { "direction" }, { "ratio", "gutter", "collapsible" });
		W("ScrollView", {}, { "horizontal", "vertical", "scrollbar", "scrollSpeed", "scrollBehavior" });
		W("Panel", { "title" }, { "collapsed", "collapsible" });
		W("Tabs", {}, { "selected" });          W("Tab", { "title" }, { "icon" });
		W("Toolbar", {}, {});                 W("MenuBar", {}, {});     W("StatusBar", {}, {});
		W("Menu", { "label" }, {});             W("MenuItem", { "label" }, { "shortcut", "checked", "icon" });
		W("Popover", {}, { "open", "anchor" }); W("Modal", { "title" }, { "open" });
		W("ContextMenu", {}, {});             W("Tooltip", { "text" }, {});   W("DockSpace", {}, {});
		W("Button", { "label" }, { "variant", "icon", "loading" });
		W("IconButton", { "icon" }, {});        W("ToggleButton", { "label" }, { "checked", "icon" });
		W("CheckBox", { "label" }, { "checked" }); W("CheckList", {}, { "gap" }); W("CheckItem", { "label" }, { "checked" });
		W("RadioGroup", {}, { "value", "options" });
		W("Switch", {}, { "checked", "label" });
		W("TextBox", {}, { "placeholder", "value", "maxLength", "password", "readOnly" });
		W("TextArea", {}, { "placeholder", "value", "maxLength", "readOnly", "rows", "tabSize" });
		W("NumberField", {}, { "value", "min", "max", "step", "precision", "unit", "label", "readOnly" });
		W("Slider", {}, { "value", "min", "max", "step" });  W("RangeSlider", {}, { "low", "high", "min", "max", "step" });
		W("Dropdown", {}, { "value", "options", "placeholder" });  W("ComboBox", {}, { "value", "options", "placeholder" });
		W("SearchField", {}, { "placeholder", "value", "maxLength", "readOnly" });   W("ColorPicker", {}, { "value", "alpha", "format", "formats", "precision", "popupSize", "showAlpha", "showInputs", "showFormatSwitcher", "showHue", "showSaturationValue" });
		W("DatePicker", {}, { "value" });       W("KeybindField", {}, { "value", "placeholder" });
		W("Stepper", {}, { "value", "min", "max", "step", "precision", "unit" });  W("SegmentedControl", {}, { "options", "value" });
		W("Rating", {}, { "value", "max" });    W("Breadcrumb", {}, { "options", "value" });
		W("Text", { "text" }, { "wrap", "maxLines" });  W("Label", { "text" }, {});
		W("Image", { "source" }, { "fit" });      W("Icon", { "name" }, {});
		W("FpsCounter", {}, { "showFrameTime" });
		W("ProgressBar", {}, { "value", "min", "max", "indeterminate" });  W("Spinner", {}, {});
		W("Divider", {}, {});                 W("Separator", {}, {});   W("Badge", { "text" }, {});
		W("TreeView", {}, { "items", "selected" });  W("ListView", {}, { "items", "selected" });
		W("Table", {}, { "columns", "rows" });  W("Canvas", {}, { "zoom", "pan" });
		W("HUD", {}, {});  W("Crosshair", {}, {});  W("RadialMenu", {}, { "items" });
		W("HealthBar", {}, { "value", "max" });  W("DamageNumber", { "text" }, {});
		return s;
	}

	namespace {
		bool contains(const std::vector<std::string_view>& v, std::string_view s) {
			for (auto x : v) if (x == s) return true;
			return false;
		}
		std::string join(const std::vector<std::string_view>& v) {
			std::string s;
			for (std::size_t i = 0; i < v.size(); ++i) { if (i) s += ", "; s += v[i]; } return s;
		}

		struct Validator {
			const Program& prog; const XuiSchema& schema; const StringInterner& in; const Document& doc; DiagnosticList& dl;

			void unknownName(std::string_view what, std::string_view name, u32 off, std::string_view onType, const std::vector<std::string_view>& valid) {
				std::string s = didYouMean(name, valid);
				std::string hint = !s.empty() ? "did you mean '" + s + "'?" : (valid.empty() ? std::string() : std::string(onType) + " accepts: " + join(valid));
				dl.error(off, u32(name.size()), "unknown " + std::string(what) + " '" + std::string(name) + "' on " + std::string(onType), hint);
			}

			void node(const XNode& n) {
				std::string_view type = in.view(n.type);
				if (type == "Slot") {
					for (const XArg& a : doc.argsOf(n))
						if (a.name.v == 0 || in.view(a.name) != "name") dl.error(a.off, a.len, "Slot() only accepts `name: \"...\"`");
					return;
				}
				const Document* owner = nullptr;
				const XComponent* comp = prog.findComponent(n.type, &owner);
				const XuiSchema::Widget* w = comp ? nullptr : schema.find(type);
				if (!comp && !w) {
					std::vector<std::string_view> names = schema.widgetNames();
					for (const auto& [k, v] : prog.components) { (void)v; names.push_back(in.view(k)); }
					std::string s = didYouMean(type, names);
					dl.error(n.off, n.len, "unknown widget '" + std::string(type) + "'", s.empty() ? "define it with `component " + std::string(type) + " { ... }` or import it" : "did you mean '" + s + "'?");
					return;
				}
				std::vector<std::string_view> ctor, valid;
				if (comp) { for (const XParam& p : owner->paramsOf(*comp)) ctor.push_back(in.view(p.name)); }
				else for (const auto& c : w->ctor) ctor.push_back(c);
				valid = ctor;
				if (w) for (const auto& p : w->props) valid.push_back(p);
				for (auto g : XuiSchema::genericProps()) valid.push_back(g);

				std::vector<std::string_view> seen; std::size_t pos = 0; bool tooMany = false;
				auto mark = [&](std::string_view name, u32 off, u32 len) {
					if (contains(seen, name)) dl.error(off, len, "'" + std::string(name) + "' is given more than once");
					else seen.push_back(name);
					};
				for (const XArg& a : doc.argsOf(n)) {
					if (a.name.v == 0) {
						if (pos >= ctor.size()) {
							if (!tooMany) dl.error(a.off, a.len, std::string(type) + " takes " + std::to_string(ctor.size()) + " positional argument(s)", "pass extra values by name, e.g. Name(prop: value)");
							tooMany = true;
						}
						else mark(ctor[pos++], a.off, a.len);
					}
					else {
						std::string_view name = in.view(a.name);
						if (!contains(valid, name)) unknownName("parameter", name, a.off, type, valid);
						else mark(name, a.off, u32(name.size()));
					}
				}
				for (const XProp& p : doc.propsOf(n)) {
					if (p.role != XPropRole::Value) continue;   // bind/if/for/events are valid on every node
					std::string_view name = in.view(p.name);
					if (!contains(valid, name)) unknownName("property", name, p.off, type, valid);
					else mark(name, p.off, u32(name.size()));
				}
				if (comp) for (const XParam& p : owner->paramsOf(*comp))
					if (p.defaultExpr == kNoExpr && !contains(seen, in.view(p.name)))
						dl.error(n.off, n.len, "missing required parameter '" + std::string(in.view(p.name)) + "' for component " + std::string(type));
			}
		};
	}

	Result<void, XuiError> validate(const Program& prog, const XuiSchema& schema, const StringInterner& in) {
		XuiError e;
		for (const Document& d : prog.files) {
			DiagnosticList dl = d.diagnostics();
			Validator v{ prog, schema, in, d, dl };
			for (const XNode& n : d.nodes) v.node(n);
			if (dl.hasErrors()) e.lists.push_back(std::move(dl));
		}
		if (!e.lists.empty()) return fail(std::move(e));
		return {};
	}
}
