#include <glfw/glfw_platform.hpp>
#include <opengl/gl_drawlist_renderer.hpp>
#include <xui/xui.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	constexpr std::size_t kMaxDigits = 16;
	constexpr std::size_t kHistorySlots = 6;
	constexpr std::size_t kMemorySlots = 6;

	struct HistoryEntry {
		std::string expression;
		std::string result;
		double value = 0.0;
	};

	struct Calculator {
		xui::Xui* ui = nullptr;
		xui::Platform* platform = nullptr;

		// Display/entry state. `entry` keeps the raw, ungrouped digits so parsing and editing
		// stay exact; grouping is only applied when the text is sent to the runtime.
		std::string entry = "0";
		std::string expression;
		double accumulator = 0.0;
		double lastOperand = 0.0;
		char pending = 0;
		char lastOp = 0;
		bool replaceEntry = true; // the next digit replaces the display
		bool operandReady = false; // the current entry belongs to the pending operation
		bool evaluated = false;    // the last action was "="
		bool error = false;

		bool drawerOpen = false;
		bool historyOpen = false;
		bool memoryOpen = false;
		bool noticeOpen = false;

		std::vector<HistoryEntry> history;
		std::vector<double> memory;

		// ---- formatting -------------------------------------------------------------------

		static std::string formatNumber(double value) {
			if (!std::isfinite(value))
				return "Overflow";
			if (value == 0.0)
				return "0";
			std::ostringstream out;
			out << std::setprecision(16) << value;
			std::string text = out.str();
			const std::size_t exponent = text.find('e');
			if (exponent != std::string::npos) {
				const std::size_t dot = text.find('.');
				if (dot != std::string::npos && dot < exponent) {
					std::size_t trim = exponent;
					while (trim > dot + 1 && text[trim - 1] == '0')
						--trim;
					if (trim == dot + 1)
						trim = dot;
					text.erase(trim, exponent - trim);
				}
				return text;
			}
			if (const std::size_t dot = text.find('.'); dot != std::string::npos) {
				while (!text.empty() && text.back() == '0')
					text.pop_back();
				if (!text.empty() && text.back() == '.')
					text.pop_back();
			}
			return text == "-0" ? "0" : text;
		}

		static std::string grouped(std::string text) {
			if (text.empty() || text.find_first_of("eE") != std::string::npos)
				return text;
			std::size_t start = text.front() == '-' ? 1 : 0;
			const std::size_t dot = text.find('.', start);
			const std::size_t end = dot == std::string::npos ? text.size() : dot;
			if (end <= start + 3)
				return text;
			std::string out = text.substr(0, start);
			const std::size_t digits = end - start;
			for (std::size_t i = 0; i < digits; ++i) {
				if (i > 0 && (digits - i) % 3 == 0)
					out.push_back(',');
				out.push_back(text[start + i]);
			}
			out += text.substr(end);
			return out;
		}

		static std::string symbol(char op) {
			switch (op) {
			case '+': return "+";
			case '-': return "−";
			case '*': return "×";
			case '/': return "÷";
			default: return "";
			}
		}

		static std::size_t digitCount(const std::string& text) {
			std::size_t count = 0;
			for (char c : text)
				if (c >= '0' && c <= '9')
					++count;
			return count;
		}

		// Returns false for division by zero and non-finite results; callers report the reason.
		static bool calculate(double lhs, double rhs, char op, double& result) {
			switch (op) {
			case '+': result = lhs + rhs; break;
			case '-': result = lhs - rhs; break;
			case '*': result = lhs * rhs; break;
			case '/':
				if (rhs == 0.0)
					return false;
				result = lhs / rhs;
				break;
			default: result = rhs; break;
			}
			return std::isfinite(result);
		}

		static std::string failure(char op, double lhs, double rhs) {
			if (op == '/' && rhs == 0.0)
				return lhs == 0.0 ? "Result is undefined" : "Cannot divide by zero";
			return "Overflow";
		}

		double number() const {
			return std::strtod(entry.c_str(), nullptr);
		}

		// ---- runtime sync -----------------------------------------------------------------

		void show() {
			ui->runtime().setValue("display", error ? entry : grouped(entry));
			ui->runtime().setValue("expression", expression);
		}

		void syncHistory() {
			ui->setModelValue("historyEmpty", history.empty());
			for (std::size_t i = 0; i < kHistorySlots; ++i) {
				const std::string index = std::to_string(i);
				const bool present = i < history.size();
				ui->setModelValue("history" + index, present);
				ui->runtime().setValue("history-" + index + "-expression",
					present ? history[i].expression : std::string{});
				ui->runtime().setValue("history-" + index + "-result",
					present ? grouped(history[i].result) : std::string{});
			}
		}

		void syncMemory() {
			ui->setModelValue("memoryEmpty", memory.empty());
			for (std::size_t i = 0; i < kMemorySlots; ++i) {
				const std::string index = std::to_string(i);
				const bool present = i < memory.size();
				ui->setModelValue("memory" + index, present);
				ui->runtime().setValue("memory-" + index + "-value",
					present ? grouped(formatNumber(memory[i])) : std::string{});
			}
		}

		void setPanels(bool drawer, bool historyVisible, bool memoryVisible, bool notice) {
			drawerOpen = drawer;
			historyOpen = historyVisible;
			memoryOpen = memoryVisible;
			noticeOpen = notice;
			ui->setModelValue("drawerOpen", drawerOpen);
			ui->setModelValue("historyOpen", historyOpen);
			ui->setModelValue("memoryOpen", memoryOpen);
			ui->setModelValue("modeNotice", noticeOpen);
		}

		bool anyPanelOpen() const {
			return drawerOpen || historyOpen || memoryOpen || noticeOpen;
		}

		void initialize() {
			setPanels(false, false, false, false);
			ui->setModelValue("historyEmpty", true);
			ui->setModelValue("memoryEmpty", true);
			for (std::size_t i = 0; i < kHistorySlots; ++i)
				ui->setModelValue("history" + std::to_string(i), false);
			for (std::size_t i = 0; i < kMemorySlots; ++i)
				ui->setModelValue("memory" + std::to_string(i), false);
			show();
		}

		// ---- arithmetic -------------------------------------------------------------------

		void addHistory(const std::string& formattedExpression, const std::string& formattedResult, double value) {
			history.push_back({formattedExpression, formattedResult, value});
			if (history.size() > kHistorySlots)
				history.erase(history.begin());
			syncHistory();
		}

		void fail(std::string message) {
			entry = std::move(message);
			expression.clear();
			pending = 0;
			lastOp = 0;
			lastOperand = 0.0;
			accumulator = 0.0;
			replaceEntry = true;
			operandReady = false;
			evaluated = false;
			error = true;
			show();
		}

		void clear() {
			entry = "0";
			expression.clear();
			pending = 0;
			lastOp = 0;
			lastOperand = 0.0;
			accumulator = 0.0;
			replaceEntry = true;
			operandReady = false;
			evaluated = false;
			error = false;
			show();
		}

		void clearEntry() {
			entry = "0";
			error = false;
			replaceEntry = true;
			operandReady = false;
			evaluated = false;
			if (!pending) {
				expression.clear();
				lastOp = 0;
				lastOperand = 0.0;
			}
			show();
		}

		void setEntryFromValue(double value) {
			entry = formatNumber(value);
			expression.clear();
			pending = 0;
			lastOp = 0;
			lastOperand = 0.0;
			replaceEntry = true;
			operandReady = true;
			evaluated = false;
			error = false;
			show();
		}

		void digit(char c) {
			if (error) {
				clear();
			} else if (evaluated) {
				expression.clear();
				pending = 0;
				lastOp = 0;
				lastOperand = 0.0;
				accumulator = 0.0;
				evaluated = false;
				replaceEntry = true;
			}
			if (replaceEntry) {
				entry = c == '.' ? "0." : std::string(1, c);
				replaceEntry = false;
			} else if (c == '.') {
				if (entry.find('.') == std::string::npos)
					entry.push_back('.');
			} else if (digitCount(entry) < kMaxDigits) {
				if (entry == "0")
					entry.assign(1, c);
				else if (entry == "-0")
					entry = std::string("-") + c;
				else
					entry.push_back(c);
			}
			operandReady = true;
			show();
		}

		void backspace() {
			if (error || replaceEntry) {
				clearEntry();
				return;
			}
			if (entry == "0")
				return;
			entry.pop_back();
			if (entry.empty() || entry == "-")
				entry = "0";
			operandReady = true;
			show();
		}

		void operation(char op) {
			if (error)
				return;
			const double current = number();
			if (pending && operandReady) {
				double result = 0.0;
				if (!calculate(accumulator, current, pending, result)) {
					fail(failure(pending, accumulator, current));
					return;
				}
				accumulator = result;
				entry = formatNumber(accumulator);
			} else {
				accumulator = current;
			}
			pending = op;
			lastOp = 0;
			lastOperand = 0.0;
			replaceEntry = true;
			operandReady = false;
			evaluated = false;
			expression = grouped(formatNumber(accumulator)) + " " + symbol(op);
			show();
		}

		void equals() {
			if (error)
				return;
			char op = 0;
			double rhs = 0.0;
			if (pending) {
				op = pending;
				rhs = number();
			} else if (lastOp) {
				op = lastOp;
				rhs = lastOperand;
			} else {
				return;
			}
			double result = 0.0;
			if (!calculate(accumulator, rhs, op, result)) {
				fail(failure(op, accumulator, rhs));
				return;
			}
			expression = grouped(formatNumber(accumulator)) + " " + symbol(op) + " " + grouped(formatNumber(rhs)) + " =";
			accumulator = result;
			lastOp = op;
			lastOperand = rhs;
			pending = 0;
			entry = formatNumber(result);
			replaceEntry = true;
			operandReady = false;
			evaluated = true;
			addHistory(expression, entry, result);
			show();
		}

		void percent() {
			if (error)
				return;
			const double value = number();
			double result = value / 100.0;
			if (pending == '+' || pending == '-')
				result = accumulator * value / 100.0;
			if (!std::isfinite(result)) {
				fail("Overflow");
				return;
			}
			entry = formatNumber(result);
			replaceEntry = true;
			operandReady = true;
			evaluated = false;
			lastOp = 0;
			lastOperand = 0.0;
			show();
		}

		void negate() {
			if (error) {
				clear();
				return;
			}
			if (entry != "0" && entry != "0.") {
				if (entry.front() == '-')
					entry.erase(entry.begin());
				else
					entry.insert(entry.begin(), '-');
			}
			operandReady = true;
			show();
		}

		void unarySquare() {
			if (error)
				return;
			const double value = number();
			const double result = value * value;
			if (!std::isfinite(result)) {
				fail("Overflow");
				return;
			}
			entry = formatNumber(result);
			replaceEntry = true;
			operandReady = true;
			evaluated = false;
			lastOp = 0;
			lastOperand = 0.0;
			if (!pending)
				expression = "sqr(" + grouped(formatNumber(value)) + ")";
			show();
		}

		void unarySquareRoot() {
			if (error)
				return;
			const double value = number();
			if (value < 0.0) {
				fail("Invalid input");
				return;
			}
			entry = formatNumber(std::sqrt(value));
			replaceEntry = true;
			operandReady = true;
			evaluated = false;
			lastOp = 0;
			lastOperand = 0.0;
			if (!pending)
				expression = "√(" + grouped(formatNumber(value)) + ")";
			show();
		}

		void unaryReciprocal() {
			if (error)
				return;
			const double value = number();
			if (value == 0.0) {
				fail("Cannot divide by zero");
				return;
			}
			entry = formatNumber(1.0 / value);
			replaceEntry = true;
			operandReady = true;
			evaluated = false;
			lastOp = 0;
			lastOperand = 0.0;
			if (!pending)
				expression = "1/(" + grouped(formatNumber(value)) + ")";
			show();
		}

		// ---- memory -----------------------------------------------------------------------

		void memoryStore() {
			memory.insert(memory.begin(), number());
			if (memory.size() > kMemorySlots)
				memory.resize(kMemorySlots);
			syncMemory();
		}

		void memoryAddCurrent() {
			if (memory.empty())
				memory.push_back(number());
			else
				memory.front() += number();
			syncMemory();
		}

		void memorySubtractCurrent() {
			if (memory.empty())
				memory.push_back(-number());
			else
				memory.front() -= number();
			syncMemory();
		}

		void memoryRecall() {
			if (memory.empty())
				return;
			setEntryFromValue(memory.front());
		}

		void memoryClearAll() {
			memory.clear();
			syncMemory();
		}

		void memoryRecallItem(std::size_t index) {
			if (index >= memory.size())
				return;
			setEntryFromValue(memory[index]);
			setPanels(false, false, false, false);
		}

		void memoryItemClear(std::size_t index) {
			if (index >= memory.size())
				return;
			memory.erase(memory.begin() + static_cast<std::ptrdiff_t>(index));
			syncMemory();
		}

		void memoryItemAdd(std::size_t index, double delta) {
			if (index >= memory.size())
				return;
			memory[index] += delta;
			syncMemory();
		}

		// ---- history ----------------------------------------------------------------------

		void clearHistory() {
			history.clear();
			syncHistory();
		}

		void historyRecall(std::size_t index) {
			if (index >= history.size())
				return;
			setEntryFromValue(history[index].value);
			setPanels(false, false, false, false);
		}

		// ---- clipboard --------------------------------------------------------------------

		void copyDisplay() {
			if (platform)
				platform->setClipboardText(entry);
		}

		void paste() {
			if (!platform)
				return;
			const std::string clipboard = platform->clipboardText();
			std::string cleaned;
			for (char c : clipboard) {
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',')
					continue;
				if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')
					cleaned.push_back(c);
				else
					return;
			}
			if (cleaned.empty())
				return;
			char* end = nullptr;
			const double value = std::strtod(cleaned.c_str(), &end);
			if (end == cleaned.c_str() || *end != '\0' || !std::isfinite(value))
				return;
			setEntryFromValue(value);
		}

		// ---- events -----------------------------------------------------------------------

		void toggleMenu() {
			setPanels(!drawerOpen, false, false, false);
		}

		void toggleHistory() {
			setPanels(false, !historyOpen, false, false);
		}

		void toggleMemory() {
			setPanels(false, false, !memoryOpen, false);
		}

		void showNotice() {
			setPanels(false, false, false, true);
		}

		void closePanels() {
			setPanels(false, false, false, false);
		}

		void dispatch(std::string_view event) {
			if (event.starts_with("onDigit") && event.size() == 8) {
				digit(event.back());
				return;
			}

			auto slotAfter = [&](std::string_view prefix) -> std::optional<std::size_t> {
				if (!event.starts_with(prefix))
					return std::nullopt;
				const std::string_view suffix = event.substr(prefix.size());
				if (suffix.size() != 1 || suffix.front() < '0' || suffix.front() > '5')
					return std::nullopt;
				return static_cast<std::size_t>(suffix.front() - '0');
			};

			if (const auto index = slotAfter("onHistoryRecall")) {
				historyRecall(*index);
				return;
			}
			if (const auto index = slotAfter("onMemoryRecall")) {
				memoryRecallItem(*index);
				return;
			}
			if (const auto index = slotAfter("onMemoryItemClear")) {
				memoryItemClear(*index);
				return;
			}
			if (const auto index = slotAfter("onMemoryItemAdd")) {
				memoryItemAdd(*index, number());
				return;
			}
			if (const auto index = slotAfter("onMemoryItemSubtract")) {
				memoryItemAdd(*index, -number());
				return;
			}

			if (event == "onDecimal") { digit('.'); return; }
			if (event == "onAdd") { operation('+'); return; }
			if (event == "onSubtract") { operation('-'); return; }
			if (event == "onMultiply") { operation('*'); return; }
			if (event == "onDivide") { operation('/'); return; }
			if (event == "onEquals") { equals(); return; }
			if (event == "onClear") { clear(); return; }
			if (event == "onClearEntry") { clearEntry(); return; }
			if (event == "onBackspace") { backspace(); return; }
			if (event == "onPercent") { percent(); return; }
			if (event == "onNegate") { negate(); return; }
			if (event == "onSquare") { unarySquare(); return; }
			if (event == "onSquareRoot") { unarySquareRoot(); return; }
			if (event == "onReciprocal") { unaryReciprocal(); return; }

			if (event == "onMemoryClear") { memoryClearAll(); return; }
			if (event == "onMemoryRecall") { memoryRecall(); return; }
			if (event == "onMemoryStore") { memoryStore(); return; }
			if (event == "onMemoryAdd") { memoryAddCurrent(); return; }
			if (event == "onMemorySubtract") { memorySubtractCurrent(); return; }
			if (event == "onMemoryClearAll") { memoryClearAll(); return; }
			if (event == "onMemoryPanel") { toggleMemory(); return; }

			if (event == "onHistoryClearAll") { clearHistory(); return; }
			if (event == "onMenu") { toggleMenu(); return; }
			if (event == "onHistory") { toggleHistory(); return; }
			if (event == "onMode") { showNotice(); return; }
			if (event == "onModeStandard") { closePanels(); return; }
			if (event == "onNoticeClose") { closePanels(); return; }
		}
	};

	void onUiEvent(void* context, std::string_view event) {
		static_cast<Calculator*>(context)->dispatch(event);
	}

	bool onKey(void* context, xui::Key key, xui::u8 modifiers) {
		auto& calc = *static_cast<Calculator*>(context);
		const bool ctrl = (modifiers & static_cast<xui::u8>(xui::Mod::Ctrl)) != 0;
		const bool shift = (modifiers & static_cast<xui::u8>(xui::Mod::Shift)) != 0;
		const bool alt = (modifiers & static_cast<xui::u8>(xui::Mod::Alt)) != 0;
		const auto raw = static_cast<unsigned>(key);

		if (ctrl) {
			// Windows Calculator shortcuts; the memory panel opens from the M⌄ button because
			// Ctrl+M is already "store in memory".
			if (shift && raw == 'D') { calc.clearHistory(); return true; }
			if (raw == 'C') { calc.copyDisplay(); return true; }
			if (raw == 'V') { calc.paste(); return true; }
			if (raw == 'H') { calc.toggleHistory(); return true; }
			if (raw == 'M') { calc.memoryStore(); return true; }
			if (raw == 'P') { calc.memoryAddCurrent(); return true; }
			if (raw == 'Q') { calc.memorySubtractCurrent(); return true; }
			if (raw == 'R') { calc.memoryRecall(); return true; }
			if (raw == 'L') { calc.memoryClearAll(); return true; }
			return false;
		}
		if (alt)
			return false;

		if (key == xui::Key::Escape) {
			if (calc.anyPanelOpen())
				calc.closePanels();
			else
				calc.clear();
			return true;
		}
		// Shifted number-row keys: * % √ (main keyboard) and + (= key).
		if (shift) {
			if (raw == '8') { calc.operation('*'); return true; }
			if (raw == '5') { calc.percent(); return true; }
			if (raw == '2') { calc.unarySquareRoot(); return true; }
			if (raw == '=') { calc.operation('+'); return true; }
		}
		if (raw >= '0' && raw <= '9') {
			calc.digit(static_cast<char>(raw));
			return true;
		}
		// Unshifted Windows Calculator function shortcuts.
		if (raw == 'R') { calc.unaryReciprocal(); return true; }
		if (raw == 'Q') { calc.unarySquare(); return true; }

		if (key >= xui::Key::Kp0 && key <= xui::Key::Kp9) {
			calc.digit(static_cast<char>('0' + (raw - static_cast<unsigned>(xui::Key::Kp0))));
			return true;
		}

		switch (key) {
		case xui::Key::Period:
		case xui::Key::Comma:
		case xui::Key::KpDecimal:
			calc.digit('.');
			return true;
		case xui::Key::Equal:
		case xui::Key::Enter:
		case xui::Key::KpEnter:
		case xui::Key::KpEqual:
			calc.equals();
			return true;
		case xui::Key::Minus:
		case xui::Key::KpSubtract:
			calc.operation('-');
			return true;
		case xui::Key::Slash:
		case xui::Key::KpDivide:
			calc.operation('/');
			return true;
		case xui::Key::KpMultiply:
			calc.operation('*');
			return true;
		case xui::Key::KpAdd:
			calc.operation('+');
			return true;
		case xui::Key::Backspace:
			calc.backspace();
			return true;
		case xui::Key::Delete:
			calc.clearEntry();
			return true;
		case xui::Key::F9:
			calc.negate();
			return true;
		default:
			return false;
		}
	}
} // namespace

int main(int argc, char** argv) {
	// Resolve the demo's authored documents and fonts beside the executable. This keeps
	// packaged and manually launched builds working even when the process has another cwd.
	if (argc > 0 && argv[0] && *argv[0]) {
		std::error_code pathError;
		const std::filesystem::path executable = std::filesystem::absolute(argv[0], pathError);
		if (!pathError && !executable.parent_path().empty())
			std::filesystem::current_path(executable.parent_path(), pathError);
	}
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	xui::Xui ui;
	Calculator calculator;
	calculator.ui = &ui;
	calculator.platform = &platform;
	ui.setEventHandler(onUiEvent, &calculator);
	ui.setKeyHandler(onKey, &calculator);

	xui::XuiOptions options;
	options.appName = "Calculator";
	options.width = 320;
	options.height = 500;
	options.minWidth = 320;
	options.minHeight = 500;
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
	calculator.initialize();
	return ui.run();
}
