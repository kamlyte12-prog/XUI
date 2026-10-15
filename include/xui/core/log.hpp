#pragma once
/// @file log.hpp  Logger owned by a Context (no global state).
#include <xui/core/callback.hpp>
#include <string_view>
namespace xui {
	enum class LogLevel : u8 { Debug, Info, Warn, Error };
	struct Logger {
		Callback<void(LogLevel, std::string_view)> sink;   ///< null = discard
		LogLevel min = LogLevel::Info;
		void log(LogLevel l, std::string_view msg) const { if (sink && l >= min) sink(l, msg); }
	};
}
