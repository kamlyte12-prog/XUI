#include <xui/lang/xui.hpp>
#include <algorithm>
#include <set>
#include <unordered_map>

namespace xui {
	namespace {
		struct SourceNode { u32 file = 0, node = 0; };
		using Bindings = std::unordered_map<StrId, u32>;
		struct SlotContent { SourceNode source; Bindings bindings; std::string prefix; std::string partScope; };
		using Slots = std::unordered_map<std::string, std::vector<SlotContent>>;

		class Expander {
		public:
			Expander(const Program& program, StringInterner& strings, Document& output)
				: m_program(program), m_strings(strings), m_output(output) {}

			bool run(std::string& error) {
				if (m_program.files.empty() || m_program.files.front().roots.size() != 1) {
					error = "component expansion requires one entry document with exactly one root";
					return false;
				}
				const Document& entry = m_program.files.front();
				m_output.file = entry.file;
				m_output.source = entry.source;
				std::vector<u32> roots;
				if (!appendNode({ 0, entry.roots.front() }, {}, {}, {}, {}, roots, error)) return false;
				if (roots.size() != 1) { error = "component expansion changed the number of document roots"; return false; }
				m_output.roots = std::move(roots);
				return true;
			}

		private:
			const Document& doc(u32 file) const { return m_program.files[file]; }

			bool copyExpr(u32 file, u32 sourceIndex, const Bindings& bindings, u32& result, std::string& error, u32 depth = 0) {
				if (depth > 128 || sourceIndex >= doc(file).exprs.size()) {
					error = "component expression is invalid or nested too deeply"; return false;
				}
				const XExpr& source = doc(file).exprs[sourceIndex];
				if (source.kind == XExprKind::Name) {
					if (auto it = bindings.find(source.name); it != bindings.end()) { result = it->second; return true; }
				}
				XExpr expression = source;
				expression.a = expression.b = expression.c = kNoExpr;
				expression.argFirst = static_cast<u32>(m_output.callArgs.size());
				expression.argCount = 0;
				const u32 outputIndex = static_cast<u32>(m_output.exprs.size());
				m_output.exprs.push_back(expression);
				auto childExpr = [&](u32 input, u32 slot) {
					if (input == kNoExpr) return true;
					u32 output = kNoExpr;
					if (!copyExpr(file, input, bindings, output, error, depth + 1)) return false;
					if (slot == 0) m_output.exprs[outputIndex].a = output;
					else if (slot == 1) m_output.exprs[outputIndex].b = output;
					else m_output.exprs[outputIndex].c = output;
					return true;
				};
				if (!childExpr(source.a, 0) || !childExpr(source.b, 1) || !childExpr(source.c, 2)) return false;
				if (source.kind == XExprKind::Call) {
					for (u32 i = 0; i < source.argCount; ++i) {
						const u32 sourceArg = doc(file).callArgs[source.argFirst + i];
						u32 outputArg = kNoExpr;
						if (!copyExpr(file, sourceArg, bindings, outputArg, error, depth + 1)) return false;
						m_output.callArgs.push_back(outputArg);
					}
					m_output.exprs[outputIndex].argCount = source.argCount;
				}
				if (source.kind == XExprKind::Interp) {
					m_output.exprs[outputIndex].argFirst = static_cast<u32>(m_output.parts.size());
					for (u32 i = 0; i < source.argCount; ++i) {
						XInterpPart part = doc(file).parts[source.argFirst + i];
						if (part.expr != kNoExpr && !copyExpr(file, part.expr, bindings, part.expr, error, depth + 1)) return false;
						m_output.parts.push_back(part);
					}
					m_output.exprs[outputIndex].argCount = source.argCount;
				}
				result = outputIndex;
				return true;
			}

			bool isLiteral(u32 expression) const {
				if (expression >= m_output.exprs.size()) return false;
				switch (m_output.exprs[expression].kind) {
				case XExprKind::Number: case XExprKind::Size: case XExprKind::Bool:
				case XExprKind::String: case XExprKind::Color: case XExprKind::Name: return true;
				default: return false;
				}
			}

			std::string stringValue(u32 file, u32 expr, const Bindings& bindings) {
				if (expr >= doc(file).exprs.size()) return {};
				const XExpr& value = doc(file).exprs[expr];
				if (value.kind == XExprKind::Name) {
					if (auto it = bindings.find(value.name); it != bindings.end()) {
						const XExpr& bound = m_output.exprs[it->second];
						if (bound.kind == XExprKind::String || bound.kind == XExprKind::Name) return std::string(m_strings.view(bound.name));
					}
				}
				if (value.kind == XExprKind::String || value.kind == XExprKind::Name) return std::string(m_strings.view(value.name));
				return {};
			}

			bool appendPartMarker(std::vector<XProp>& props, StrId componentName, std::string_view partName, std::string& error) {
				const std::string marker = detail::componentPartMarker(m_strings.view(componentName), partName);
				for (XProp& property : props) {
					if (m_strings.view(property.name) != "class") continue;
					if (property.expr >= m_output.exprs.size()) break;
					XExpr& expression = m_output.exprs[property.expr];
					if (expression.kind != XExprKind::String && expression.kind != XExprKind::Name) {
						error = "a component part cannot be added to a non-literal class value";
						return false;
					}
					const std::string current(m_strings.view(expression.name));
					expression.kind = XExprKind::String;
					expression.name = m_strings.intern(current.empty() ? marker : current + " " + marker);
					return true;
				}
				XExpr expression; expression.kind = XExprKind::String; expression.name = m_strings.intern(marker);
				const u32 index = static_cast<u32>(m_output.exprs.size());
				m_output.exprs.push_back(expression);
				XProp property; property.name = m_strings.intern("class"); property.expr = index;
				props.push_back(property);
				return true;
			}

			bool componentBindings(u32 file, const XNode& call, const Bindings& parentBindings,
				const XComponent& component, const Document& owner, Bindings& bindings, std::string& error) {
				std::span<const XParam> params = owner.paramsOf(component);
				std::vector<bool> assigned(params.size(), false);
				u32 positional = 0; bool sawNamed = false;
				const std::string componentName(m_strings.view(component.name));
				for (const XArg& arg : doc(file).argsOf(call)) {
					u32 parameter = kNoExpr;
					if (!arg.name.v) {
						if (sawNamed) { error = "component " + componentName + " cannot use positional arguments after named arguments"; return false; }
						parameter = positional++;
						if (parameter >= params.size()) { error = "component " + componentName + " accepts at most " + std::to_string(params.size()) + " positional argument(s)"; return false; }
					}
					else {
						sawNamed = true;
						for (u32 i = 0; i < params.size(); ++i) if (params[i].name == arg.name) { parameter = i; break; }
						if (parameter == kNoExpr) { error = "unknown parameter '" + std::string(m_strings.view(arg.name)) + "' for component " + componentName; return false; }
					}
					if (assigned[parameter]) { error = "component parameter '" + std::string(m_strings.view(params[parameter].name)) + "' is supplied more than once"; return false; }
					u32 value = kNoExpr;
					if (!copyExpr(file, arg.expr, parentBindings, value, error) || !isLiteral(value)) {
						error = "parameter '" + std::string(m_strings.view(params[parameter].name)) + "' of component " + componentName + " must be a literal value or handler name"; return false;
					}
					bindings[params[parameter].name] = value;
					assigned[parameter] = true;
				}
				for (u32 i = 0; i < params.size(); ++i) {
					if (assigned[i]) continue;
					if (params[i].defaultExpr == kNoExpr) { error = "missing required parameter '" + std::string(m_strings.view(params[i].name)) + "' for component " + componentName; return false; }
					u32 value = kNoExpr;
					if (!copyExpr(componentFile(component, owner), params[i].defaultExpr, bindings, value, error) || !isLiteral(value)) {
						error = "default for parameter '" + std::string(m_strings.view(params[i].name)) + "' of component " + componentName + " must be a literal value or handler name"; return false;
					}
					bindings[params[i].name] = value;
				}
				return true;
			}

			u32 componentFile(const XComponent& component, const Document& owner) const {
				(void)component;
				for (u32 i = 0; i < m_program.files.size(); ++i) if (&m_program.files[i] == &owner) return i;
				return 0;
			}

			void componentInstanceId(u32 file, const XNode& call, const Bindings& bindings, std::string& id) {
				for (const XProp& prop : doc(file).propsOf(call))
					if (m_strings.view(prop.name) == "id") { id = stringValue(file, prop.expr, bindings); break; }
				if (id.empty()) id = "__" + std::string(m_strings.view(call.type)) + "_" + std::to_string(++m_instanceSerial);
			}

			bool appendNode(SourceNode source, const Bindings& bindings, const std::string& prefix,
				const Slots& slots, std::string_view partScope, std::vector<u32>& destination, std::string& error) {
				if (source.file >= m_program.files.size() || source.node >= doc(source.file).nodes.size()) {
					error = "invalid node while expanding a component"; return false;
				}
				const Document& sourceDoc = doc(source.file);
				const XNode& node = sourceDoc.nodes[source.node];
				if (m_strings.view(node.type) == "Slot") {
					std::string name;
					for (const XArg& arg : sourceDoc.argsOf(node))
						if (m_strings.view(arg.name) == "name") name = stringValue(source.file, arg.expr, bindings);
					if (auto found = slots.find(name); found != slots.end()) {
						const u32 token = m_activeSlotTokens.empty() ? 0u : m_activeSlotTokens.back();
						if (!m_usedSlots.insert({ token, name }).second) {
							error = "component declares Slot(name: \"" + name + "\") more than once";
							return false;
						}
						for (const SlotContent& child : found->second)
							if (!appendNode(child.source, child.bindings, child.prefix, {}, child.partScope, destination, error)) return false;
						return true;
					}
					for (u32 child : sourceDoc.childrenOf(node))
						if (!appendNode({ source.file, child }, bindings, prefix, slots, partScope, destination, error)) return false;
					return true;
				}

				const Document* componentOwner = nullptr;
				if (const XComponent* component = m_program.findComponent(node.type, &componentOwner)) {
					if (std::find(m_componentStack.begin(), m_componentStack.end(), node.type) != m_componentStack.end()) {
						error = "recursive component expansion involving '" + std::string(m_strings.view(node.type)) + "'";
						return false;
					}
					if (m_componentStack.size() >= 64) { error = "component nesting exceeds the limit of 64"; return false; }
					const u32 componentFileIndex = componentFile(*component, *componentOwner);
					Bindings childBindings;
					if (!componentBindings(source.file, node, bindings, *component, *componentOwner, childBindings, error)) return false;
					Slots childSlots;
					for (u32 child : sourceDoc.childrenOf(node)) {
						const XNode& childNode = sourceDoc.nodes[child];
						std::string slotName;
						for (const XProp& prop : sourceDoc.propsOf(childNode))
							if (m_strings.view(prop.name) == "slot") slotName = stringValue(source.file, prop.expr, bindings);
						childSlots[slotName].push_back({ { source.file, child }, bindings, prefix, std::string(partScope) });
					}
					std::string instance;
					componentInstanceId(source.file, node, bindings, instance);
					const std::string childPrefix = prefix.empty() ? instance : prefix + "." + instance;
					const u32 expansionToken = ++m_instanceSerial;
					m_activeSlotTokens.push_back(expansionToken);
					m_componentStack.push_back(node.type);
					std::vector<u32> expanded;
					const bool expandedOk = appendNode({ componentFileIndex, component->root }, childBindings, childPrefix, childSlots, m_strings.view(component->name), expanded, error);
					m_componentStack.pop_back();
					m_activeSlotTokens.pop_back();
					if (!expandedOk) return false;
					if (expanded.size() != 1) { error = "a component root must expand to exactly one widget"; return false; }
					XNode& result = m_output.nodes[expanded.front()];
					std::vector<XProp> callerProps;
					for (const XProp& prop : sourceDoc.propsOf(node)) {
						const std::string_view propertyName = m_strings.view(prop.name);
						if (propertyName == "slot") continue;
						if (propertyName == "part") {
							const std::string partName = stringValue(source.file, prop.expr, bindings);
							if (partScope.empty() || partName.empty()) { error = "component part requires a named part inside a component definition"; return false; }
							if (!appendPartMarker(callerProps, m_strings.intern(partScope), partName, error)) return false;
							continue;
						}
						XProp copied = prop;
						if (copied.expr != kNoExpr && !copyExpr(source.file, copied.expr, bindings, copied.expr, error)) return false;
						callerProps.push_back(copied);
					}
					const u32 insertion = result.propFirst + result.propCount;
					m_output.props.insert(m_output.props.begin() + insertion, callerProps.begin(), callerProps.end());
					for (u32 i = 0; i < m_output.nodes.size(); ++i)
						if (i > expanded.front() && m_output.nodes[i].propFirst >= insertion) m_output.nodes[i].propFirst += static_cast<u32>(callerProps.size());
					result.propCount += static_cast<u32>(callerProps.size());
					destination.push_back(expanded.front());
					return true;
				}

				const u32 outputIndex = static_cast<u32>(m_output.nodes.size());
				XNode copy = node;
				copy.argFirst = static_cast<u32>(m_output.args.size()); copy.argCount = 0;
				copy.propFirst = static_cast<u32>(m_output.props.size()); copy.propCount = 0;
				copy.childFirst = 0; copy.childCount = 0;
				m_output.nodes.push_back(copy);
				for (const XArg& arg : sourceDoc.argsOf(node)) {
					XArg cloned = arg;
					if (!copyExpr(source.file, arg.expr, bindings, cloned.expr, error)) return false;
					m_output.args.push_back(cloned); ++m_output.nodes[outputIndex].argCount;
				}
				std::vector<XProp> copiedProps;
				std::string partName;
				bool hasPart = false;
				for (const XProp& prop : sourceDoc.propsOf(node)) {
					const std::string_view propertyName = m_strings.view(prop.name);
					if (propertyName == "slot") continue;
					if (propertyName == "part") { hasPart = true; partName = stringValue(source.file, prop.expr, bindings); continue; }
					XProp cloned = prop;
					if (cloned.expr != kNoExpr && !copyExpr(source.file, cloned.expr, bindings, cloned.expr, error)) return false;
					if (m_strings.view(cloned.name) == "id" && !prefix.empty() && cloned.expr < m_output.exprs.size()) {
						XExpr& idExpr = m_output.exprs[cloned.expr];
						if (idExpr.kind == XExprKind::String || idExpr.kind == XExprKind::Name)
							idExpr.name = m_strings.intern(prefix + "." + std::string(m_strings.view(idExpr.name)));
					}
					copiedProps.push_back(cloned);
				}
				if (hasPart) {
					if (partScope.empty() || partName.empty()) { error = "`part` is only valid with a literal part name inside a component definition"; return false; }
					if (!appendPartMarker(copiedProps, m_strings.intern(partScope), partName, error)) return false;
				}
				m_output.nodes[outputIndex].propCount = static_cast<u32>(copiedProps.size());
				m_output.props.insert(m_output.props.end(), copiedProps.begin(), copiedProps.end());
				std::vector<u32> expandedChildren;
				for (u32 child : sourceDoc.childrenOf(node)) {
					if (!appendNode({ source.file, child }, bindings, prefix, slots, partScope, expandedChildren, error)) return false;
				}
				m_output.nodes[outputIndex].childFirst = static_cast<u32>(m_output.children.size());
				m_output.nodes[outputIndex].childCount = static_cast<u32>(expandedChildren.size());
				m_output.children.insert(m_output.children.end(), expandedChildren.begin(), expandedChildren.end());
				destination.push_back(outputIndex);
				return true;
		}

			const Program& m_program;
			StringInterner& m_strings;
			Document& m_output;
			u32 m_instanceSerial = 0;
			std::vector<u32> m_activeSlotTokens;
			std::vector<StrId> m_componentStack;
			std::set<std::pair<u32, std::string>> m_usedSlots;
		};
	}

	bool expandXuiComponents(const Program& program, StringInterner& in, Document& out, std::string& error) {
		out = {};
		return Expander(program, in, out).run(error);
	}
}
