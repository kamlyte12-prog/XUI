#include <glfw/glfw_platform.hpp>
#include <opengl/gl_drawlist_renderer.hpp>
#include <xui/xui.hpp>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {
	struct Calculator {
		xui::Xui* ui = nullptr;
		std::string entry = "0";
		std::string expression;
		char pending = 0;
		double accumulator = 0.0;
		double memory = 0.0;
		bool replaceEntry = true;
		bool hasMemory = false;
		bool error = false;

		double number() const { return std::strtod(entry.c_str(), nullptr); }
		void show() {
			ui->runtime().setValue("display", entry);
			ui->runtime().setValue("expression", expression);
		}
		static std::string format(double value) {
			if (!std::isfinite(value)) return "Cannot divide by zero";
			if (value == 0.0) return "0";
			std::ostringstream out;
			out << std::setprecision(15) << value;
			std::string text = out.str();
			if (text.size() > 18) { out.str({}); out.clear(); out << std::setprecision(9) << std::scientific << value; text = out.str(); }
			return text;
		}
		void digit(char c) {
			if (error) clear();
			if (replaceEntry) { entry = c == '.' ? "0." : std::string(1, c); replaceEntry = false; }
			else if (c == '.') { if (entry.find('.') == std::string::npos) entry.push_back('.'); }
			else if (entry == "0") entry.assign(1, c);
			else if (entry == "-0") entry = "-" + std::string(1, c);
			else if (entry.size() < 18) entry.push_back(c);
			show();
		}
		std::optional<double> apply(double lhs, double rhs, char op) const {
			switch (op) {
			case '+': return lhs + rhs;
			case '-': return lhs - rhs;
			case '*': return lhs * rhs;
			case '/': if (rhs == 0.0) return std::nullopt; return lhs / rhs;
			default: return rhs;
			}
		}
		void setResult(double value) {
			entry = format(value);
			error = entry == "Cannot divide by zero";
			replaceEntry = true;
		}
		void operation(char op) {
			if (error) clear();
			const double current = number();
			if (pending && !replaceEntry) {
				const auto result = apply(accumulator, current, pending);
				if (!result) { setResult(std::numeric_limits<double>::quiet_NaN()); pending = 0; expression.clear(); show(); return; }
				accumulator = *result;
				entry = format(accumulator);
			} else accumulator = current;
			pending = op;
			replaceEntry = true;
			expression = format(accumulator) + " " + opSymbol(op);
			show();
		}
		static std::string opSymbol(char op) { return op == '*' ? "×" : op == '/' ? "÷" : op == '-' ? "−" : "+"; }
		void equals() {
			if (!pending || error) return;
			const char op = pending;
			const double rhs = number();
			const auto result = apply(accumulator, rhs, op);
			expression = format(accumulator) + " " + opSymbol(op) + " " + format(rhs) + " =";
			pending = 0;
			if (result) setResult(*result); else setResult(std::numeric_limits<double>::quiet_NaN());
			show();
		}
		void unary(std::string_view which) {
			if (error) clear();
			const double value = number();
			if (which == "percent") setResult(value / 100.0);
			else if (which == "negate") setResult(-value);
			else if (which == "square") setResult(value * value);
			else if (which == "root") setResult(value < 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::sqrt(value));
			else if (which == "reciprocal") setResult(value == 0.0 ? std::numeric_limits<double>::quiet_NaN() : 1.0 / value);
			show();
		}
		void clearEntry() { entry = "0"; error = false; replaceEntry = true; show(); }
		void clear() { entry = "0"; expression.clear(); pending = 0; accumulator = 0.0; error = false; replaceEntry = true; show(); }
		void backspace() {
			if (error || replaceEntry) { clearEntry(); return; }
			if (entry.size() <= 1 || (entry.size() == 2 && entry[0] == '-')) entry = "0";
			else entry.pop_back();
			show();
		}
		void memoryAction(std::string_view action) {
			if (action == "store") { memory = number(); hasMemory = true; }
			else if (action == "clear") { memory = 0.0; hasMemory = false; }
			else if (action == "recall" && hasMemory) { entry = format(memory); replaceEntry = true; }
			else if (action == "add") { memory += number(); hasMemory = true; }
			else if (action == "subtract") { memory -= number(); hasMemory = true; }
			show();
		}
	};

	void onUiEvent(void* context, std::string_view event) {
		auto& calc = *static_cast<Calculator*>(context);
		if (event.starts_with("onDigit")) { calc.digit(event.back()); return; }
		if (event == "onDecimal") { calc.digit('.'); return; }
		if (event == "onAdd") { calc.operation('+'); return; }
		if (event == "onSubtract") { calc.operation('-'); return; }
		if (event == "onMultiply") { calc.operation('*'); return; }
		if (event == "onDivide") { calc.operation('/'); return; }
		if (event == "onEquals") { calc.equals(); return; }
		if (event == "onClear") { calc.clear(); return; }
		if (event == "onClearEntry") { calc.clearEntry(); return; }
		if (event == "onBackspace") { calc.backspace(); return; }
		if (event == "onPercent") { calc.unary("percent"); return; }
		if (event == "onNegate") { calc.unary("negate"); return; }
		if (event == "onSquare") { calc.unary("square"); return; }
		if (event == "onSquareRoot") { calc.unary("root"); return; }
		if (event == "onReciprocal") { calc.unary("reciprocal"); return; }
		if (event == "onMemoryClear") { calc.memoryAction("clear"); return; }
		if (event == "onMemoryRecall") { calc.memoryAction("recall"); return; }
		if (event == "onMemoryStore") { calc.memoryAction("store"); return; }
		if (event == "onMemoryAdd") { calc.memoryAction("add"); return; }
		if (event == "onMemorySubtract") { calc.memoryAction("subtract"); return; }
	}

	bool onKey(void* context, xui::Key key, xui::u8 modifiers) {
		auto& calc = *static_cast<Calculator*>(context);
		const auto raw = static_cast<unsigned>(key);
		if (raw == 'C') { calc.clear(); return true; }
		if ((modifiers & static_cast<xui::u8>(xui::Mod::Shift)) && raw == '8') { calc.operation('*'); return true; }
		if (raw >= '0' && raw <= '9') { calc.digit(static_cast<char>(raw)); return true; }
		if (raw >= static_cast<unsigned>(xui::Key::Kp0) && raw <= static_cast<unsigned>(xui::Key::Kp9)) {
			calc.digit(static_cast<char>('0' + raw - static_cast<unsigned>(xui::Key::Kp0))); return true;
		}
		switch (key) {
		case xui::Key::Period: case xui::Key::KpDecimal: calc.digit('.'); return true;
		case xui::Key::Equal: case xui::Key::KpAdd: calc.operation('+'); return true;
		case xui::Key::Minus: case xui::Key::KpSubtract: calc.operation('-'); return true;
		case xui::Key::KpMultiply: calc.operation('*'); return true;
		case xui::Key::Slash: case xui::Key::KpDivide: calc.operation('/'); return true;
		case xui::Key::Enter: case xui::Key::KpEnter: case xui::Key::KpEqual: calc.equals(); return true;
		case xui::Key::Backspace: calc.backspace(); return true;
		case xui::Key::Delete: calc.clearEntry(); return true;
		case xui::Key::Escape: calc.clear(); return true;
		default: return false;
		}
	}
}

int main() {
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	xui::Xui ui;
	Calculator calculator{ &ui };
	ui.setEventHandler(onUiEvent, &calculator);
	ui.setKeyHandler(onKey, &calculator);
	xui::XuiOptions options;
	options.appName = "Calculator";
	options.width = 400;
	options.height = 665;
	ui.setWindowOptions(options);
	if (!ui.loadXui("calculator.xui") || !ui.loadXss("calculator.xss")) {
		std::cerr << ui.lastError() << '\n';
		return 1;
	}
	if (!ui.loadFont("Roboto.ttf")) {
		std::cerr << ui.lastError() << '\n';
		return 1;
	}
	if (!ui.attach(renderer, platform)) {
		std::cerr << ui.lastError() << '\n';
		return 1;
	}
	return ui.run();
}
