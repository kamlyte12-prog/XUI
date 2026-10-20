#pragma once
/// @file xss.hpp  The .xss style language: compile a source string into a flat, indexed StyleSheet.
///
/// Runtime model (no tree walk per frame):
///   node creation / class change :  StyleSheet::match()   -> StyleSetId   (bucket lookup + cached set)
///   per resolve                  :  StyleSheet::resolve() -> PropTable     (O(#decls in active states), no allocation)
///   theme switch                 :  pass a different theme index to resolve(); nothing is recompiled.
#include <xui/core/diagnostic.hpp>
#include <xui/core/result.hpp>
#include <xui/style/style_types.hpp>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
namespace xui {

	struct StyleSetId { u32 v = 0xFFFFFFFFu; bool valid() const { return v != 0xFFFFFFFFu; } friend bool operator==(StyleSetId, StyleSetId) = default; };

	/// Property lookup used to test `Type[prop: value]` filters: writes the node's current value, returns false if unset.
	using PropLookup = bool (*)(void* ctx, StrId prop, Atom& out);

	struct MatchInput {
		StrId type;                          ///< widget type name (interned)
		std::span<const StrId> classes;
		StrId id;
		PropLookup lookup = nullptr;
		void* ctx = nullptr;
	};

	class StyleSheet {
	public:
		// ---- queries ------------------------------------------------------------------------------
		/// Find (or lazily build and cache) the style set for a node. Not for per-frame use: it may allocate.
		StyleSetId match(const MatchInput& in);
		/// Merge Resting + every active state (in enum order) into @p out. Allocation-free.
		void resolve(StyleSetId set, StateMask active, u32 theme, PropTable& out) const;
		/// Transitions declared by the rules matching this set, ordered lowest -> highest precedence.
		std::span<const Transition> transitions(StyleSetId set) const;

		u32 themeCount() const { return static_cast<u32>(m_themeNames.size()); }
		std::optional<u32> themeIndex(StrId name) const;
		/// Evaluated token value for a theme (dotted name, interned). Tokens are compile-time constants.
		std::optional<Atom> token(StrId name, u32 theme = 0) const;
		const Easing& curve(u32 index) const { return m_curves[index]; }
		u32 curveCount() const { return static_cast<u32>(m_curves.size()); }
		const KeyframesDef* findKeyframes(StrId name) const;
		std::span<const Keyframe> frames(const KeyframesDef& k) const { return { m_frames.data() + k.frameFirst, k.frameCount }; }
		std::span<const Declaration> frameDecls(const Keyframe& f) const { return { m_decls.data() + f.declFirst, f.declCount }; }
		std::size_t ruleCount() const { return m_rules.size(); }
		std::size_t setCount() const { return m_sets.size(); }
		std::size_t declCount() const { return m_decls.size() + m_setDecls.size(); }

	private:
		friend class XssCompiler;
		struct Rule {
			Selector sel; u32 order = 0, score = 0;
			u32 stateFirst[kStateCount] = {}, stateCount[kStateCount] = {};   // into m_decls
			u32 transFirst = 0, transCount = 0;                               // into m_trans
		};
		struct SetRec {
			u32 stateFirst[kStateCount] = {}, stateCount[kStateCount] = {};   // into m_setDecls
			u32 transFirst = 0, transCount = 0;                               // into m_setTrans
		};
		bool ruleMatches(const Rule& r, const MatchInput& in) const;
		u32 buildSet(const std::vector<u32>& matched);

		std::vector<Rule> m_rules;
		std::vector<Declaration> m_decls;         // per-rule declarations
		std::vector<Transition> m_trans;
		std::unordered_map<StrId, std::vector<u32>> m_byType;   // key 0 = rules without a type
		std::vector<SetRec> m_sets;
		std::vector<Declaration> m_setDecls;      // merged per-set, per-state declarations
		std::vector<Transition> m_setTrans;
		std::map<std::vector<u32>, u32> m_setCache;

		std::vector<StrId> m_themeNames;
		std::vector<std::unordered_map<StrId, Atom>> m_tokens;   // per theme
		std::vector<Atom> m_palette;              // [theme * m_slotCount + slot]
		u32 m_slotCount = 0;
		std::vector<Easing> m_curves;
		std::vector<KeyframesDef> m_kf;
		std::vector<Keyframe> m_frames;
	};

	/// Compile .xss source. On failure returns every diagnostic found (the parser recovers and keeps going).
	/// The bundled base tokens (`color.on-primary`, `color.primary.active`, `color.muted`, ...) are visible to
	/// the source and can be overridden by it.
	Result<StyleSheet, DiagnosticList> compileXss(std::string_view source, std::string_view fileName, StringInterner& interner);
}
