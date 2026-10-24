#include <opengl/gl_drawlist_renderer.hpp>
#include <glfw/glfw_platform.hpp>
#include <xui/xui.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {
	struct GameState {
		xui::Xui* ui = nullptr;
		xui::Platform* platform = nullptr;
		xui::u32 page = 0;
		xui::u32 settingsReturnPage = 0;
		xui::u32 inventoryReturnPage = 1;
		int playerHealth = 100;
		int enemyHealth = 100;
		int enemyMaxHealth = 100;
		int potions = 3;
		int score = 0;
		int turn = 1;
		bool guarding = false;
		bool finished = false;
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

	void setPage(GameState& game, xui::u32 page) {
		game.page = page;
		game.ui->runtime().setValue("game-pages", page);
	}

	bool hasEquipment(GameState& game, std::string_view id) {
		const auto equipment = valueAs<xui::CheckListValue>(game, "equipment");
		return equipment && std::find(equipment->begin(), equipment->end(), id) != equipment->end();
	}

	int strikePower(GameState& game) {
		const auto power = valueAs<xui::f64>(game, "strike-power");
		return power ? static_cast<int>(std::lround(*power)) : 14;
	}

	std::string difficultyName(GameState& game) {
		const auto difficulty = valueAs<std::string>(game, "difficulty");
		return difficulty.value_or("Normal");
	}

	int counterDamage(GameState& game) {
		const std::string difficulty = difficultyName(game);
		if (difficulty == "Easy") return 6;
		if (difficulty == "Hard") return 15;
		return 9;
	}

	void updateHint(GameState& game) {
		const auto hints = valueAs<bool>(game, "combat-hints");
		const bool enabled = hints.value_or(true);
		setText(game, "hints-readout", enabled ? "Hints: on" : "Hints: off");
		setText(game, "battle-hint", enabled
			? "Hint: use Guard to blunt the next counterattack."
			: "Combat hints are hidden in Settings.");
	}

	void updateEquipmentReadout(GameState& game) {
		std::string text = "Equipment: ";
		bool any = false;
		if (hasEquipment(game, "rune-blade")) { text += "Rune blade"; any = true; }
		if (hasEquipment(game, "iron-shield")) {
			text += any ? ", Iron shield" : "Iron shield";
			any = true;
		}
		if (!any) text += "none";
		setText(game, "equipment-readout", std::move(text));
	}

	void updateHud(GameState& game) {
		auto& runtime = game.ui->runtime();
		runtime.setValue("player-health", xui::f64(game.playerHealth) / 100.0);
		runtime.setValue("enemy-health", game.enemyMaxHealth > 0
			? xui::f64(game.enemyHealth) / game.enemyMaxHealth : 0.0);
		setText(game, "player-health-readout", std::to_string(game.playerHealth) + " / 100");
		setText(game, "enemy-health-readout", std::to_string(game.enemyHealth) + " / " + std::to_string(game.enemyMaxHealth));
		setText(game, "turn-readout", "Turn " + std::to_string(game.turn));
		setText(game, "score-readout", "Score " + std::to_string(game.score));
		setText(game, "potion-readout", "Healing draughts: " + std::to_string(game.potions));
		setText(game, "power-readout", "Strike power: " + std::to_string(strikePower(game)));
	}

	void animateNotice(GameState& game, std::string_view id) {
		game.ui->playAnimation(id, xui::AnimationMode::Once);
	}

	void finishEncounter(GameState& game, bool victory) {
		game.finished = true;
		setText(game, "result-title", victory ? "Victory" : "Defeat");
		setText(game, "result-detail", victory
			? "The Ashen Gate opens. The road is yours."
			: "The sentinel holds the road. Regroup and try again.");
		setText(game, "final-score", "Final score: " + std::to_string(game.score));
		setPage(game, 5);
		animateNotice(game, "result-title");
	}

	void counterTurn(GameState& game, std::string actionMessage) {
		if (game.enemyHealth <= 0) {
			updateHud(game);
			setText(game, "battle-log", std::move(actionMessage));
			finishEncounter(game, true);
			return;
		}
		int damage = counterDamage(game);
		if (hasEquipment(game, "iron-shield")) damage = std::max(1, damage - 3);
		if (game.guarding) {
			damage = std::max(1, (damage + 1) / 2);
			game.guarding = false;
		}
		game.playerHealth = std::max(0, game.playerHealth - damage);
		++game.turn;
		updateHud(game);
		actionMessage += " The sentinel counters for " + std::to_string(damage) + ".";
		setText(game, "battle-log", std::move(actionMessage));
		animateNotice(game, "battle-log");
		if (game.playerHealth == 0) finishEncounter(game, false);
	}

	void attack(GameState& game, bool heavy) {
		if (game.finished) return;
		int damage = strikePower(game);
		if (hasEquipment(game, "rune-blade")) damage += 4;
		if (heavy) damage = damage * 17 / 10;
		damage = std::min(damage, game.enemyHealth);
		game.enemyHealth -= damage;
		game.score += damage * 10;
		counterTurn(game, std::string(heavy ? "Heavy strike" : "Strike") + " deals " + std::to_string(damage) + " damage.");
	}

	void resetEncounter(GameState& game) {
		game.playerHealth = 100;
		game.potions = 3;
		game.score = 0;
		game.turn = 1;
		game.guarding = false;
		game.finished = false;
		const std::string difficulty = difficultyName(game);
		game.enemyMaxHealth = difficulty == "Easy" ? 80 : (difficulty == "Hard" ? 125 : 100);
		game.enemyHealth = game.enemyMaxHealth;
		updateHud(game);
		setText(game, "battle-log", "The Ash sentinel waits. Your turn.");
		updateHint(game);
	}

	void onUiEvent(void* context, std::string_view name) {
		auto& game = *static_cast<GameState*>(context);
		if (name == "onPageChange") {
			if (const auto page = valueAs<xui::u32>(game, "game-pages")) game.page = *page;
		}
		else if (name == "onStart") {
			resetEncounter(game);
			setPage(game, 1);
			animateNotice(game, "battle-log");
		}
		else if (name == "onStrike") attack(game, false);
		else if (name == "onHeavyStrike") attack(game, true);
		else if (name == "onHeal") {
			if (game.finished) return;
			if (game.potions <= 0) {
				setText(game, "battle-log", "No healing draughts remain.");
				animateNotice(game, "battle-log");
				return;
			}
			const int restored = std::min(30, 100 - game.playerHealth);
			--game.potions;
			game.playerHealth = std::min(100, game.playerHealth + 30);
			counterTurn(game, "The draught restores " + std::to_string(restored) + " health.");
		}
		else if (name == "onGuard") {
			if (game.finished) return;
			game.guarding = true;
			counterTurn(game, "You brace behind your shield.");
		}
		else if (name == "onPause") setPage(game, 2);
		else if (name == "onResume") setPage(game, 1);
		else if (name == "onOpenSettings") {
			game.settingsReturnPage = game.page;
			setPage(game, 3);
		}
		else if (name == "onSettingsBack") setPage(game, game.settingsReturnPage);
		else if (name == "onOpenInventory") {
			game.inventoryReturnPage = game.page;
			setPage(game, 4);
		}
		else if (name == "onInventoryBack") setPage(game, game.inventoryReturnPage);
		else if (name == "onMainMenu") setPage(game, 0);
		else if (name == "onRestart") {
			resetEncounter(game);
			setPage(game, 1);
			animateNotice(game, "battle-log");
		}
		else if (name == "onQuit") game.platform->closeWindow(game.ui->window());
		else if (name == "onDifficultyChange") {
			setText(game, "difficulty-readout", "Difficulty: " + difficultyName(game));
		}
		else if (name == "onPowerChange") {
			const std::string text = "Strike power: " + std::to_string(strikePower(game));
			setText(game, "settings-power-readout", text);
			setText(game, "power-readout", text);
		}
		else if (name == "onHintsChange") updateHint(game);
		else if (name == "onInventoryChange") updateEquipmentReadout(game);
	}
}

int main() {
	xui::XuiOptions options;
	options.appName = "xui-turn-based-game";
	options.windowTitle = "The Ashen Gate - Xui game UI";
	options.width = 1000;
	options.height = 720;
	options.minWidth = 760;
	options.minHeight = 540;

	xui::Xui ui(options);
	if (!ui.loadFont("Roboto.ttf", "Roboto") ||
		!ui.loadXui("game.xui") ||
		!ui.loadXss("game.xss")) {
		std::cerr << ui.lastError() << '\n';
		return 1;
	}

	GlDrawListRenderer renderer;
	GlfwPlatform platform;
	GameState game{ &ui, &platform };
	ui.setEventHandler(onUiEvent, &game);
	if (!ui.attach(renderer, platform)) {
		std::cerr << ui.lastError() << '\n';
		return 1;
	}
	updateHud(game);
	updateHint(game);
	updateEquipmentReadout(game);
	return ui.run();
}
