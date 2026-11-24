#pragma once
/// @file model.hpp Observable values consumed by .xui bindings and collection controls.
#include <xui/core/math.hpp>
#include <variant>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <utility>

namespace xui {

	/// A copyable value for application state. Objects and arrays are recursive and own their data.
	struct ModelValue {
		using Array = std::vector<ModelValue>;
		using Object = std::unordered_map<std::string, ModelValue>;
		using Storage = std::variant<std::monostate, bool, f64, std::string, Color, Array, Object>;
		Storage data;

		ModelValue() = default;
		ModelValue(bool value) : data(value) {}
		ModelValue(f64 value) : data(value) {}
		ModelValue(f32 value) : data(static_cast<f64>(value)) {}
		ModelValue(i32 value) : data(static_cast<f64>(value)) {}
		ModelValue(u32 value) : data(static_cast<f64>(value)) {}
		ModelValue(const char* value) : data(std::string(value ? value : "")) {}
		ModelValue(std::string value) : data(std::move(value)) {}
		ModelValue(Color value) : data(value) {}
		ModelValue(Array value) : data(std::move(value)) {}
		ModelValue(Object value) : data(std::move(value)) {}

		bool truthy() const {
			if (const auto* value = std::get_if<bool>(&data)) return *value;
			if (const auto* value = std::get_if<f64>(&data)) return *value != 0.0;
			if (const auto* value = std::get_if<std::string>(&data)) return !value->empty();
			if (const auto* value = std::get_if<Array>(&data)) return !value->empty();
			if (std::holds_alternative<Object>(data)) return true;
			return false;
		}
		std::string toString() const {
			if (const auto* value = std::get_if<bool>(&data)) return *value ? "true" : "false";
			if (const auto* value = std::get_if<f64>(&data)) {
				std::ostringstream text; text << std::setprecision(15) << *value; return text.str();
			}
			if (const auto* value = std::get_if<std::string>(&data)) return *value;
			if (const auto* value = std::get_if<Color>(&data)) {
				std::ostringstream text; text << '#' << std::hex << std::setfill('0') << std::setw(8) << value->toRGBA8();
				return text.str();
			}
			return {};
		}
	};

} // namespace xui
