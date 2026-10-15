#pragma once
/// @file result.hpp  Exception-free error propagation (std::expected).
#include <expected>
#include <utility>
namespace xui {
	template <class T, class E> using Result = std::expected<T, E>;
	template <class E> constexpr auto fail(E&& e) { return std::unexpected<std::decay_t<E>>(std::forward<E>(e)); }
}
