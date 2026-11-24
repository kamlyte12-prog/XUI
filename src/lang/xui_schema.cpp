#include <xui/lang/xui.hpp>
#include <algorithm>
namespace xui {

	void XuiSchema::addWidget(std::string type, std::vector<std::string> ctor, std::vector<std::string> props,
							  std::vector<std::string> unimplemented, bool runtimeImplemented) {
		m_widgets[std::move(type)] =
			Widget{std::move(ctor), std::move(props), std::move(unimplemented), runtimeImplemented};
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
		static const std::vector<std::string_view> g = {"id",
														"class",
														"key",
														"slot",
														"part",
														"grow",
														"shrink",
														"gridColumn",
														"gridRow",
														"columnSpan",
														"rowSpan",
														"visible",
														"disabled",
														"tooltip",
														"tooltipShowDelay",
														"tooltipHideDelay",
														"tooltipTrigger",
														"tooltipHideOnClick",
														"tooltipDisabled",
														"tooltipFollowCursor",
														"align",
														"justify",
														"position",
														"left",
														"top",
														"right",
														"bottom",
														"pad",
														"margin",
														"size",
														"minSize",
														"maxSize",
														"width",
														"height",
														"minWidth",
														"maxWidth",
														"minHeight",
														"maxHeight",
														"opacity"};
		return g;
	}

	// Core subset of the standard library. Full property sets are owned by the widget library (M7), which
	// re-registers each widget with its complete documented list; unknown names here are what the validator rejects.
	XuiSchema XuiSchema::withCoreWidgets() {
		XuiSchema s;
		auto W = [&](const char* t, std::vector<std::string> ctor, std::vector<std::string> props,
					 std::vector<std::string> unimplemented = {}, bool runtimeImplemented = true) {
			s.addWidget(t, std::move(ctor), std::move(props), std::move(unimplemented), runtimeImplemented);
		};
		W("Window", {"title", "size"}, {"resizable", "platformTopMenu"}, {"resizable"});
		W("TitleBar", {}, { "platformButtons" });
		W("Row", {"gap"}, {"wrap"}, {"wrap"});
		W("Column", {"gap"}, {"wrap"}, {"wrap"});
		W("Grid", { "columns" }, { "rows", "gap" });  W("Stack", {}, {});
		W("Split", {"direction"}, {"ratio", "gutter", "collapsible"}, {"collapsible"});
		W("ScrollView", {}, { "horizontal", "vertical", "scrollbar", "scrollSpeed", "scrollBehavior" });
		W("Panel", {"title"}, {"collapsed", "collapsible"}, {"title", "collapsed", "collapsible"});
		W("Tabs", {}, { "selected" });          W("Tab", { "title" }, { "icon" });
		W("Toolbar", {}, {});                 W("MenuBar", {}, {});     W("StatusBar", {}, {});
		W("Menu", { "label" }, { "anchor", "context" }); W("MenuItem", { "label" }, { "shortcut", "checked", "icon", "disabled" });
		W("Popover", {}, {"open", "anchor"}, {}, false);
		W("Modal", {"title"}, {"open"}, {}, false);
		W("ContextMenu", {}, {}, {}, false);
		W("Tooltip", {"text"}, {}, {}, false);
		W("DockHost", {}, { "gutter", "headerHeight" }); W("DockSpace", {}, { "gutter", "headerHeight" });
		W("DockPanel", { "title" }, { "dock", "region", "target", "floating", "closable", "icon", "preferredSize", "floatMode", "float-mode", "nativeTitleBar", "native-title-bar" });
		W("Button", {"label"}, {"variant", "icon", "loading"}, {"variant", "icon", "loading"});
		W("IconButton", {"icon"}, {});
		W("ToggleButton", {"label"}, {"checked", "icon"}, {"icon"});
		W("CheckBox", { "label" }, { "checked" }); W("CheckList", {}, { "gap" }); W("CheckItem", { "label" }, { "checked" });
		W("RadioGroup", {}, { "value", "options" });
		W("Switch", {}, { "checked", "label" });
		W("TextBox", {}, {"placeholder", "value", "maxLength", "password", "readOnly", "filter", "validate"});
		W("TextArea", {}, {"placeholder", "value", "maxLength", "readOnly", "rows", "tabSize", "filter", "validate"});
		W("NumberField", {},
		  {"value", "min", "max", "step", "precision", "unit", "label", "readOnly", "filter", "validate"});
		W("Slider", {}, {"value", "min", "max", "step"}, {"step"});
		W("RangeSlider", {}, {"low", "high", "min", "max", "step"});
		W("Dropdown", {}, {"value", "options", "placeholder"}, {"placeholder"});
		W("ComboBox", {}, {"value", "options", "placeholder"}, {"placeholder"});
		W("SearchField", {}, {"placeholder", "value", "maxLength", "readOnly", "filter", "validate"});
		W("ColorPicker", {},
		  {"value", "alpha", "format", "formats", "precision", "popupSize", "showAlpha", "showInputs",
		   "showFormatSwitcher", "showHue", "showSaturationValue", "filter", "validate"});
		W("DatePicker", {}, { "value" });       W("KeybindField", {}, { "value", "placeholder" });
		W("Stepper", {}, { "value", "min", "max", "step", "precision", "unit" });  W("SegmentedControl", {}, { "options", "value" });
		W("Rating", {}, { "value", "max" });    W("Breadcrumb", {}, { "options", "value" });
		W("Text", {"text"}, {"wrap", "maxLines"}, {"wrap", "maxLines"});
		W("Label", {"text"}, {});
		W("Image", { "source" }, { "fit" });      W("Icon", { "name" }, {});
		W("FpsCounter", {}, { "showFrameTime" });
		W("ProgressBar", {}, {"value", "min", "max", "indeterminate"}, {"indeterminate"});
		W("Spinner", {}, {});
		W("Divider", {}, {});                 W("Separator", {}, {});   W("Badge", { "text" }, {});
		W("TreeView", {}, { "items", "selected" });  W("ListView", {}, { "items", "selected" });
		W("Table", {}, {"columns", "rows", "selected"});
		W("Canvas", {}, {"zoom", "pan"}, {}, false);
		W("HUD", {}, {}, {}, false);
		W("Crosshair", {}, {}, {}, false);
		W("RadialMenu", {}, {"items"}, {}, false);
		W("HealthBar", {}, {"value", "max"}, {}, false);
		W("DamageNumber", {"text"}, {}, {}, false);
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
			const std::vector<u8>& componentBody;

			void unknownName(std::string_view what, std::string_view name, u32 off, std::string_view onType, const std::vector<std::string_view>& valid) {
				std::string s = didYouMean(name, valid);
				std::string hint = !s.empty() ? "did you mean '" + s + "'?" : (valid.empty() ? std::string() : std::string(onType) + " accepts: " + join(valid));
				dl.error(off, u32(name.size()), "unknown " + std::string(what) + " '" + std::string(name) + "' on " + std::string(onType), hint);
			}

			void node(u32 nodeIndex) {
				const XNode& n = doc.nodes[nodeIndex];
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
				if (w && !w->runtimeImplemented)
					dl.add(Severity::Warning, n.off, n.len,
						   "widget '" + std::string(type) +
							   "' is accepted by the schema but has no runtime implementation",
						   "replace it with a supported widget or wait for runtime support");
				std::vector<std::string_view> ctor, valid;
				if (comp) { for (const XParam& p : owner->paramsOf(*comp)) ctor.push_back(in.view(p.name)); }
				else for (const auto& c : w->ctor) ctor.push_back(c);
				valid = ctor;
				if (w) for (const auto& p : w->props) valid.push_back(p);
				for (auto g : XuiSchema::genericProps()) valid.push_back(g);

				std::vector<std::string_view> seen; std::size_t pos = 0; bool tooMany = false, sawNamedArgument = false;
				auto warnIfUnimplemented = [&](std::string_view name, u32 off, u32 len) {
					if (!w ||
						std::find(w->unimplemented.begin(), w->unimplemented.end(), name) == w->unimplemented.end())
						return;
					dl.add(Severity::Warning, off, len,
						   "'" + std::string(name) + "' is accepted on " + std::string(type) +
							   " but is not implemented",
						   "remove this value; runtime support is not available yet");
				};
				auto mark = [&](std::string_view name, u32 off, u32 len) {
					if (contains(seen, name)) dl.error(off, len, "'" + std::string(name) + "' is given more than once");
					else seen.push_back(name);
					};
				for (const XArg& a : doc.argsOf(n)) {
					if (a.name.v == 0) {
						if (sawNamedArgument) dl.error(a.off, a.len, "positional arguments cannot follow named arguments");
						if (pos >= ctor.size()) {
							if (!tooMany) dl.error(a.off, a.len, std::string(type) + " takes " + std::to_string(ctor.size()) + " positional argument(s)", "pass extra values by name, e.g. Name(prop: value)");
							tooMany = true;
						} else {
							const std::string_view name = ctor[pos++];
							mark(name, a.off, a.len);
							warnIfUnimplemented(name, a.off, a.len);
						}
					}
					else {
						sawNamedArgument = true;
						std::string_view name = in.view(a.name);
						if (!contains(valid, name)) unknownName("parameter", name, a.off, type, valid);
						else {
							mark(name, a.off, u32(name.size()));
							warnIfUnimplemented(name, a.off, u32(name.size()));
						}
					}
				}
				for (const XProp& p : doc.propsOf(n)) {
					if (p.role != XPropRole::Value) continue;   // bind/if/for/events are valid on every node
					std::string_view name = in.view(p.name);
					if (!contains(valid, name)) unknownName("property", name, p.off, type, valid);
					else {
						mark(name, p.off, u32(name.size()));
						warnIfUnimplemented(name, p.off, u32(name.size()));
					}
					if (name == "part") {
						if (!componentBody[nodeIndex]) dl.error(p.off, p.len, "`part` is only valid on widgets inside a component definition");
						if (p.expr >= doc.exprs.size() || (doc.exprs[p.expr].kind != XExprKind::String && doc.exprs[p.expr].kind != XExprKind::Name))
							dl.error(p.off, p.len, "`part` must be a literal name", "use part: \"title\"");
						else if (doc.exprs[p.expr].name.v == 0 || in.view(doc.exprs[p.expr].name).empty())
							dl.error(p.off, p.len, "`part` cannot be empty");
					}
				}
				if (comp) for (const XParam& p : owner->paramsOf(*comp))
					if (p.defaultExpr == kNoExpr && !contains(seen, in.view(p.name)))
						dl.error(n.off, n.len, "missing required parameter '" + std::string(in.view(p.name)) + "' for component " + std::string(type));
			}
		};
	}

	Result<XuiValidationReport, XuiError> validate(const Program& prog, const XuiSchema& schema,
												   const StringInterner& in, XuiValidationOptions options) {
		XuiError e;
		XuiValidationReport report;
		for (const Document& d : prog.files) {
			DiagnosticList dl = d.diagnostics();
			std::vector<u8> componentBody(d.nodes.size(), 0);
			auto markComponentTree = [&](auto&& self, u32 index) -> void {
				if (index >= d.nodes.size() || componentBody[index]) return;
				componentBody[index] = 1;
				for (u32 child : d.childrenOf(d.nodes[index])) self(self, child);
			};
			for (const XComponent& component : d.components)
				if (component.root != kNoExpr) markComponentTree(markComponentTree, component.root);
			Validator v{ prog, schema, in, d, dl, componentBody };
			for (u32 i = 0; i < d.nodes.size(); ++i) v.node(i);
			if (options.warningsAsErrors)
				dl.promoteWarningsToErrors();
			if (dl.hasErrors()) e.lists.push_back(std::move(dl));
			else if (!dl.items().empty())
				report.warnings.push_back(std::move(dl));
		}
		if (!e.lists.empty()) return fail(std::move(e));
		return report;
	}
}
