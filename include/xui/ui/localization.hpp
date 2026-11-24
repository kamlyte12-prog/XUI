#pragma once
/// @file localization.hpp Small locale string table with plural and gender forms.
#include <xui/core/types.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace xui {

	/// Locale-aware string table. Locale tags use BCP-47-like names (`en`, `en-US`, `fr-FR`).
	/// A missing locale/key falls back to the configured fallback locale, then the key itself.
	class Localization {
	public:
		struct Forms {
			std::string value, one, other, male, female, neutral;
			std::string maleOne, maleOther, femaleOne, femaleOther;
		};

		void setLocale(std::string locale) {
			m_locale = std::move(locale);
		}
		const std::string& locale() const {
			return m_locale;
		}
		void setFallbackLocale(std::string locale) {
			m_fallback = std::move(locale);
		}
		void set(std::string_view locale, std::string key, std::string value) {
			m_tables[std::string(locale)][std::move(key)].value = std::move(value);
		}
		void setForms(std::string_view locale, std::string key, Forms forms) {
			m_tables[std::string(locale)][std::move(key)] = std::move(forms);
		}
		std::string text(std::string_view key, std::optional<i64> count = {}, std::string_view gender = {}) const {
			const Forms* forms = lookup(m_locale, key);
			if (!forms)
				forms = lookup(m_fallback, key);
			if (!forms)
				return std::string(key);
			const std::string* selected = &forms->value;
			const std::string* genderForm = &forms->value;
			if (gender == "male" && !forms->male.empty())
				genderForm = &forms->male;
			else if (gender == "female" && !forms->female.empty())
				genderForm = &forms->female;
			else if (!forms->neutral.empty())
				genderForm = &forms->neutral;
			selected = genderForm;
			if (count) {
				const bool singular = *count == 1;
				if (gender == "male" && !(singular ? forms->maleOne.empty() : forms->maleOther.empty()))
					selected = singular ? &forms->maleOne : &forms->maleOther;
				else if (gender == "female" && !(singular ? forms->femaleOne.empty() : forms->femaleOther.empty()))
					selected = singular ? &forms->femaleOne : &forms->femaleOther;
				else if (singular && !forms->one.empty())
					selected = &forms->one;
				else if (!singular && !forms->other.empty())
					selected = &forms->other;
			}
			std::string result = *selected;
			if (result.empty())
				result = std::string(key);
			if (count) {
				const std::string marker = "{count}";
				for (std::size_t at = 0; (at = result.find(marker, at)) != std::string::npos; at += 1)
					result.replace(at, marker.size(), std::to_string(*count));
			}
			return result;
		}

		/// Format a number with locale decimal/group separators and fixed precision.
		std::string number(double value, unsigned precision = 0) const {
			precision = std::min(precision, 12u);
			char buffer[128];
			if (precision)
				std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(precision), value);
			else
				std::snprintf(buffer, sizeof(buffer), "%.6g", value);
			std::string result(buffer);
			const bool commaDecimal = usesCommaDecimal(m_locale);
			const char decimal = commaDecimal ? ',' : '.';
			if (result.find_first_of("eE") != std::string::npos) {
				if (commaDecimal)
					std::replace(result.begin(), result.end(), '.', ',');
				return result;
			}
			const std::size_t point = result.find('.');
			const std::size_t integerEnd = point == std::string::npos ? result.size() : point;
			const std::string_view grouping = m_locale.rfind("fr", 0) == 0 ? "\xE2\x80\xAF" : commaDecimal ? "." : ",";
			std::string formatted;
			const std::size_t firstDigit = result[0] == '-' ? 1u : 0u;
			if (firstDigit)
				formatted.push_back('-');
			for (std::size_t i = firstDigit; i < integerEnd; ++i) {
				if (i > firstDigit && (integerEnd - i) % 3 == 0)
					formatted += grouping;
				formatted += result[i];
			}
			if (point != std::string::npos) {
				formatted += decimal;
				formatted.append(result.data() + point + 1, result.size() - point - 1);
			}
			return formatted;
		}

		/// Short date formatting for common locale families; DatePicker keeps numeric date values.
		std::string date(int year, unsigned month, unsigned day) const {
			char buffer[32];
			if (m_locale.rfind("en-US", 0) == 0)
				std::snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d", month, day, year);
			else if (m_locale.rfind("ja", 0) == 0 || m_locale.rfind("zh", 0) == 0)
				std::snprintf(buffer, sizeof(buffer), "%04d/%02u/%02u", year, month, day);
			else if (m_locale.rfind("de", 0) == 0)
				std::snprintf(buffer, sizeof(buffer), "%02u.%02u.%04d", day, month, year);
			else
				std::snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d", day, month, year);
			return buffer;
		}

	private:
		using Table = std::unordered_map<std::string, Forms>;
		const Forms* lookup(std::string_view locale, std::string_view key) const {
			auto table = m_tables.find(std::string(locale));
			if (table == m_tables.end()) {
				const std::size_t separator = locale.find('-');
				if (separator != std::string_view::npos)
					table = m_tables.find(std::string(locale.substr(0, separator)));
			}
			if (table == m_tables.end())
				return nullptr;
			auto value = table->second.find(std::string(key));
			return value == table->second.end() ? nullptr : &value->second;
		}
		static bool usesCommaDecimal(std::string_view locale) {
			return locale.rfind("fr", 0) == 0 || locale.rfind("de", 0) == 0 || locale.rfind("es", 0) == 0 ||
				   locale.rfind("it", 0) == 0 || locale.rfind("pt", 0) == 0 || locale.rfind("ru", 0) == 0;
		}
		std::string m_locale = "en-US", m_fallback = "en-US";
		std::unordered_map<std::string, Table> m_tables;
	};

} // namespace xui
