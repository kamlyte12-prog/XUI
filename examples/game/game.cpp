#include <opengl/gl_drawlist_renderer.hpp>
#include <glfw/glfw_platform.hpp>
#include <xui/xui.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	constexpr int kBoardWidth = 10;
	constexpr int kBoardHeight = 20;
	using Board = std::array<std::array<bool, kBoardWidth>, kBoardHeight>;
	using Cell = std::pair<int, int>;
	constexpr std::array<std::array<std::string_view, 4>, 7> kShapes{{
		{{"....", "####", "....", "...."}}, // I
		{{"#...", "###.", "....", "...."}}, // J
		{{"..#.", "###.", "....", "...."}}, // L
		{{".##.", ".##.", "....", "...."}}, // O
		{{".##.", "##..", "....", "...."}}, // S
		{{".#..", "###.", "....", "...."}}, // T
		{{"##..", ".##.", "....", "...."}}  // Z
	}};
	constexpr std::array<std::string_view, 7> kPieceNames{ "I", "J", "L", "O", "S", "T", "Z" };

	struct Piece { int type = 0, rotation = 0, x = 3, y = -1; };
	struct GameState {
		xui::Xui* ui = nullptr;
		xui::Platform* platform = nullptr;
		Board board{};
		Board renderedBoard{};
		bool renderedBoardValid = false;
		Piece current{};
		std::array<int, 7> bag{};
		std::size_t bagIndex = 7;
		std::mt19937 random{ std::random_device{}() };
		int nextType = 0;
		int score = 0, lines = 0, level = 1, startingLevel = 1;
		int page = 0, returnPage = 0;
		bool active = false, gameOver = false, started = false;
	};

	template<class T>
	std::optional<T> valueAs(GameState& game, std::string_view id) {
		const auto value = game.ui->runtime().value(id);
		if (!value) return std::nullopt;
		if (const T* typed = std::get_if<T>(&*value)) return *typed;
		return std::nullopt;
	}

	void setText(GameState& game, std::string_view id, std::string text) {
		game.ui->runtime().setValue(id, std::move(text));
	}

	std::vector<Cell> cellsFor(Piece piece) {
		std::vector<Cell> cells;
		for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
			if (kShapes[piece.type][row][col] != '#') continue;
			int x = col, y = row;
			for (int r = 0; r < (piece.rotation & 3); ++r) { const int oldX = x; x = 3 - y; y = oldX; }
			cells.emplace_back(piece.x + x, piece.y + y);
		}
		return cells;
	}

	bool collides(const GameState& game, Piece piece) {
		for (const auto [x, y] : cellsFor(piece)) {
			if (x < 0 || x >= kBoardWidth || y >= kBoardHeight) return true;
			if (y >= 0 && game.board[y][x]) return true;
		}
		return false;
	}

	int takeFromBag(GameState& game) {
		if (game.bagIndex >= game.bag.size()) {
			std::iota(game.bag.begin(), game.bag.end(), 0);
			std::shuffle(game.bag.begin(), game.bag.end(), game.random);
			game.bagIndex = 0;
		}
		return game.bag[game.bagIndex++];
	}

	void setPage(GameState& game, int page) {
		game.page = page;
		game.ui->runtime().setValue("game-pages", static_cast<xui::u32>(page));
	}

	void updateBoard(GameState& game) {
		std::array<std::array<bool, kBoardWidth>, kBoardHeight> visible = game.board;
		if (game.started && !game.gameOver) {
			for (const auto [x, y] : cellsFor(game.current))
				if (x >= 0 && x < kBoardWidth && y >= 0 && y < kBoardHeight) visible[y][x] = true;
		}
		static const auto ids = [] {
			std::array<std::array<std::string, kBoardWidth>, kBoardHeight> result;
			for (int row = 0; row < kBoardHeight; ++row) for (int col = 0; col < kBoardWidth; ++col)
				result[row][col] = "cell-" + std::to_string(row) + "-" + std::to_string(col);
			return result;
		}();
		for (int row = 0; row < kBoardHeight; ++row) for (int col = 0; col < kBoardWidth; ++col) {
			if (!game.renderedBoardValid || visible[row][col] != game.renderedBoard[row][col])
				game.ui->runtime().setValue(ids[row][col], xui::f64(visible[row][col] ? 1.0 : 0.0));
		}
		game.renderedBoard = visible;
		game.renderedBoardValid = true;
	}

	void updateHud(GameState& game) {
		setText(game, "score-readout", std::to_string(game.score));
		setText(game, "lines-readout", std::to_string(game.lines));
		setText(game, "level-readout", std::to_string(game.level));
		setText(game, "piece-readout", std::string(kPieceNames[game.current.type]));
		setText(game, "next-readout", std::string("Next: ") + std::string(kPieceNames[game.nextType]));
		setText(game, "gameover-score", "Score " + std::to_string(game.score));
		setText(game, "gameover-lines", "Lines " + std::to_string(game.lines));
		updateBoard(game);
	}

	xui::u32 gravityInterval(const GameState& game) {
		return static_cast<xui::u32>(std::max(100, 600 - (game.level - 1) * 45));
	}

	bool gravityTick(void* context, xui::f64 elapsedSeconds);
	void lockCurrent(GameState& game);

	void scheduleGravity(GameState& game) {
		game.ui->setTickHandler(game.active && !game.gameOver ? gravityTick : nullptr,
			game.active && !game.gameOver ? &game : nullptr,
			game.active && !game.gameOver ? gravityInterval(game) : 0);
	}

	void lockCurrent(GameState& game) {
		for (const auto [x, y] : cellsFor(game.current)) {
			if (y < 0) { game.gameOver = true; game.active = false; break; }
			game.board[y][x] = true;
		}
		if (game.gameOver) {
			setText(game, "gameover-score", "Score " + std::to_string(game.score));
			setText(game, "gameover-lines", "Lines " + std::to_string(game.lines));
			setPage(game, 4);
			game.ui->playAnimation("gameover-title", xui::AnimationMode::Once);
			game.ui->setTickHandler(nullptr, nullptr, 0);
			updateBoard(game);
			return;
		}
		int cleared = 0;
		for (int row = kBoardHeight - 1; row >= 0;) {
			if (std::all_of(game.board[row].begin(), game.board[row].end(), [](bool cell) { return cell; })) {
				for (int move = row; move > 0; --move) game.board[move] = game.board[move - 1];
				game.board[0].fill(false);
				++cleared;
			} else --row;
		}
		if (cleared) {
			constexpr std::array<int, 5> points{ 0, 100, 300, 500, 800 };
			const int lineScore = points[cleared] * game.level;
			game.score += lineScore;
			game.lines += cleared;
			game.level = game.startingLevel + game.lines / 10;
			setText(game, "line-toast", "+" + std::to_string(lineScore) + "   " +
				(cleared == 4 ? "TETRIS!" : std::to_string(cleared) + (cleared == 1 ? " LINE" : " LINES")));
			game.ui->playAnimation("line-toast", xui::AnimationMode::Once);
		}
		game.current = { game.nextType, 0, 3, -1 };
		game.nextType = takeFromBag(game);
		if (collides(game, game.current)) {
			game.gameOver = true;
			game.active = false;
			setText(game, "gameover-score", "Score " + std::to_string(game.score));
			setText(game, "gameover-lines", "Lines " + std::to_string(game.lines));
			setPage(game, 4);
			game.ui->playAnimation("gameover-title", xui::AnimationMode::Once);
			game.ui->setTickHandler(nullptr, nullptr, 0);
		}
	}

	bool gravityTick(void* context, xui::f64) {
		auto& game = *static_cast<GameState*>(context);
		if (!game.active || game.gameOver) return false;
		Piece below = game.current;
		++below.y;
		if (!collides(game, below)) game.current = below;
		else lockCurrent(game);
		updateHud(game);
		scheduleGravity(game);
		return true;
	}

	void resetGame(GameState& game) {
		game.board = {};
		game.bagIndex = 7;
		game.renderedBoardValid = false;
		game.score = 0;
		game.lines = 0;
		game.startingLevel = 1;
		if (const auto value = valueAs<std::string>(game, "starting-level"))
			try { game.startingLevel = std::clamp(std::stoi(*value), 1, 5); } catch (...) { game.startingLevel = 1; }
		game.level = game.startingLevel;
		game.current = { takeFromBag(game), 0, 3, -1 };
		game.nextType = takeFromBag(game);
		game.active = true;
		game.gameOver = false;
		game.started = true;
		setText(game, "line-toast", "");
		setPage(game, 1);
		updateHud(game);
		scheduleGravity(game);
	}

	void move(GameState& game, int dx) {
		if (!game.active) return;
		Piece moved = game.current;
		moved.x += dx;
		if (!collides(game, moved)) { game.current = moved; updateBoard(game); }
	}

	void rotate(GameState& game) {
		if (!game.active || game.current.type == 3) return; // the square piece is unchanged by rotation
		Piece turned = game.current;
		turned.rotation = (turned.rotation + 1) & 3;
		for (const int kick : { 0, -1, 1, -2, 2 }) {
			Piece candidate = turned;
			candidate.x += kick;
			if (!collides(game, candidate)) { game.current = candidate; updateBoard(game); return; }
		}
	}

	void softDrop(GameState& game) {
		if (!game.active) return;
		Piece below = game.current;
		++below.y;
		if (!collides(game, below)) { game.current = below; ++game.score; updateHud(game); }
	}

	void hardDrop(GameState& game) {
		if (!game.active) return;
		Piece below = game.current;
		int distance = 0;
		for (;;) {
			++below.y;
			if (collides(game, below)) break;
			game.current = below;
			++distance;
		}
		game.score += distance * 2;
		lockCurrent(game);
		updateHud(game);
		scheduleGravity(game);
	}

	void pauseGame(GameState& game) {
		if (!game.active) return;
		game.active = false;
		game.returnPage = 1;
		game.ui->setTickHandler(nullptr, nullptr, 0);
		setPage(game, 2);
	}

	void onUiEvent(void* context, std::string_view event) {
		auto& game = *static_cast<GameState*>(context);
		if (event == "onStart" || event == "onRestart") resetGame(game);
		else if (event == "onPause") pauseGame(game);
		else if (event == "onResume") { game.active = true; setPage(game, 1); scheduleGravity(game); }
		else if (event == "onLeft") move(game, -1);
		else if (event == "onRight") move(game, 1);
		else if (event == "onRotate") rotate(game);
		else if (event == "onSoftDrop") softDrop(game);
		else if (event == "onHardDrop") hardDrop(game);
		else if (event == "onOpenSettings") { game.returnPage = game.active ? 1 : 0; game.active = false; game.ui->setTickHandler(nullptr, nullptr, 0); setPage(game, 3); }
		else if (event == "onSettingsBack") {
			setPage(game, game.returnPage);
			if (game.returnPage == 1 && game.started && !game.gameOver) { game.active = true; scheduleGravity(game); }
		}
		else if (event == "onMainMenu") {
			game.active = false; game.gameOver = false;
			game.ui->setTickHandler(nullptr, nullptr, 0); setPage(game, 0);
			game.ui->playAnimation("menu-title", xui::AnimationMode::Once);
		}
		else if (event == "onQuit") game.platform->closeWindow(game.ui->window());
		else if (event == "onPageChange") {
			if (const auto page = valueAs<xui::u32>(game, "game-pages")) {
				game.page = static_cast<int>(*page);
				if (game.page != 1) { game.active = false; game.ui->setTickHandler(nullptr, nullptr, 0); }
				else if (game.started && !game.gameOver) { game.active = true; scheduleGravity(game); }
			}
		}
	}
}

int main() {
	xui::XuiOptions options;
	options.appName = "xui-tetris";
	options.windowTitle = "Tetris - Xui example";
	options.width = 980; options.height = 760;
	options.minWidth = 820; options.minHeight = 680;

	xui::Xui ui(options);
	if (!ui.loadFont("Roboto.ttf", "Roboto") || !ui.loadXui("game.xui") || !ui.loadXss("game.xss")) {
		std::cerr << ui.lastError() << '\n'; return 1;
	}
	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	GameState game{ .ui = &ui, .platform = &platform };
	ui.setEventHandler(onUiEvent, &game);
	if (!ui.attach(renderer, platform)) { std::cerr << ui.lastError() << '\n'; return 1; }
	updateHud(game);
	ui.playAnimation("menu-title", xui::AnimationMode::Once);
	return ui.run();
}
