#include <xui/style/xss.hpp>
#include <algorithm>
namespace xui {

	std::optional<u32> StyleSheet::themeIndex(StrId name) const {
		for (u32 i = 0; i < m_themeNames.size(); ++i) if (m_themeNames[i] == name) return i;
		return std::nullopt;
	}
	std::optional<Atom> StyleSheet::token(StrId name, u32 theme) const {
		if (theme >= m_tokens.size()) return std::nullopt;
		auto it = m_tokens[theme].find(name);
		if (it == m_tokens[theme].end()) return std::nullopt;
		return it->second;
	}
	const KeyframesDef* StyleSheet::findKeyframes(StrId name) const {
		for (const KeyframesDef& k : m_kf) if (k.name == name) return &k;
		return nullptr;
	}

	bool StyleSheet::ruleMatches(const Rule& r, const MatchInput& in) const {
		const Selector& s = r.sel;
		if (s.type.v && s.type != in.type) return false;
		if (s.id.v && s.id != in.id) return false;
		for (StrId c : s.classes) {
			if (std::find(in.classes.begin(), in.classes.end(), c) == in.classes.end()) return false;
		}
		for (const SelectorFilter& f : s.filters) {
			Atom cur;
			if (!in.lookup || !in.lookup(in.ctx, f.prop, cur) || !atomEqual(cur, f.value)) return false;
		}
		return true;
	}

	StyleSetId StyleSheet::match(const MatchInput& in) {
		std::vector<u32> matched;
		for (StrId key : {in.type, StrId{}}) {
			auto it = m_byType.find(key);
			if (it == m_byType.end()) continue;
			for (u32 ri : it->second) if (ruleMatches(m_rules[ri], in)) matched.push_back(ri);
			if (in.type.v == 0) break;
		}
		// Fixed precedence: (score, source order) ascending; later entries override earlier ones.
		std::sort(matched.begin(), matched.end(), [&](u32 a, u32 b) {
			const Rule& x = m_rules[a], & y = m_rules[b];
			return x.score != y.score ? x.score < y.score : x.order < y.order;
			});
		if (auto it = m_setCache.find(matched); it != m_setCache.end()) return StyleSetId{ it->second };
		u32 id = buildSet(matched);
		m_setCache.emplace(std::move(matched), id);
		return StyleSetId{ id };
	}

	u32 StyleSheet::buildSet(const std::vector<u32>& matched) {
		SetRec set;
		for (int st = 0; st < kStateCount; ++st) {
			int pos[kPropCount]; std::fill(pos, pos + kPropCount, -1);
			set.stateFirst[st] = static_cast<u32>(m_setDecls.size());
			for (u32 ri : matched) {
				const Rule& r = m_rules[ri];
				for (u32 i = 0; i < r.stateCount[st]; ++i) {
					const Declaration& d = m_decls[r.stateFirst[st] + i];
					int& p = pos[static_cast<int>(d.prop)];
					if (p >= 0) m_setDecls[static_cast<std::size_t>(p)] = d;
					else { p = static_cast<int>(m_setDecls.size()); m_setDecls.push_back(d); }
				}
			}
			set.stateCount[st] = static_cast<u32>(m_setDecls.size()) - set.stateFirst[st];
		}
		set.transFirst = static_cast<u32>(m_setTrans.size());
		for (u32 ri : matched) {
			const Rule& r = m_rules[ri];
			for (u32 i = 0; i < r.transCount; ++i) m_setTrans.push_back(m_trans[r.transFirst + i]);
		}
		set.transCount = static_cast<u32>(m_setTrans.size()) - set.transFirst;
		m_sets.push_back(set);
		return static_cast<u32>(m_sets.size() - 1);
	}

	std::span<const Transition> StyleSheet::transitions(StyleSetId id) const {
		if (!id.valid()) return {};
		const SetRec& s = m_sets[id.v];
		return { m_setTrans.data() + s.transFirst, s.transCount };
	}

	void StyleSheet::resolve(StyleSetId id, StateMask active, u32 theme, PropTable& out) const {
		out.mask = 0;
		if (!id.valid()) return;
		if (theme >= m_themeNames.size()) theme = 0;
		const SetRec& s = m_sets[id.v];
		const Atom* pal = m_palette.data() + static_cast<std::size_t>(theme) * m_slotCount;
		for (int st = 0; st < kStateCount; ++st) {
			if (st != 0 && !(active & (1u << st))) continue;   // Resting (0) always applies
			const Declaration* d = m_setDecls.data() + s.stateFirst[st];
			for (u32 i = 0; i < s.stateCount[st]; ++i, ++d) {
				StyleValue& dst = out.v[static_cast<int>(d->prop)];
				dst = d->value;
				for (int a = 0; a < dst.count; ++a) if (dst.atoms[a].kind == AtomKind::Slot) dst.atoms[a] = pal[dst.atoms[a].bits];
				out.mask |= u64{ 1 } << static_cast<int>(d->prop);
			}
		}
	}
}
