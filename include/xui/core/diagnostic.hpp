#pragma once
/// @file diagnostic.hpp  Error reporting with file:line:col and a caret-highlighted snippet.
#include <xui/core/types.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace xui {
	enum class Severity : u8 { Error, Warning, Note };
	struct Diagnostic {
		Severity severity = Severity::Error;
		u32 offset = 0, length = 0, line = 1, col = 1;   ///< 1-based line/col, byte offsets
		std::string message, hint;
	};
	/// Collects diagnostics for one source file. Duplicate (offset,message) pairs are dropped.
	class DiagnosticList {
	public:
		DiagnosticList() = default;
		DiagnosticList(std::string file, std::string source) : m_file(std::move(file)), m_source(std::move(source)) {}
		void add(Severity s, u32 offset, u32 length, std::string message, std::string hint = {});
		void error(u32 offset, u32 length, std::string message, std::string hint = {}) {
			add(Severity::Error, offset, length, std::move(message), std::move(hint));
		}
		bool hasErrors() const;
		std::span<const Diagnostic> items() const { return m_items; }
		const std::string& file() const { return m_file; }
		/// Renders all diagnostics:  file:line:col: error: msg / source line / ^~~~ underline / hint.
		std::string format() const;
		std::string format(const Diagnostic& d) const;
	private:
		std::string m_file, m_source;
		std::vector<Diagnostic> m_items;
	};
	/// Levenshtein-based suggestion. Returns "" when nothing is close enough.
	std::string didYouMean(std::string_view word, std::span<const std::string_view> candidates);
}
