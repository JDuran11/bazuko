#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bazuko {

// Eventos de gameplay. La lógica los emite y la UI/audio los consumen por frame
// (sustituye a los CustomEvent sobre window del juego web).
struct EvWithdrawal { bool active; };
struct EvLevelUp { int level; };
struct EvDisease { std::string name, emoji; };
struct EvDiseaseCured { std::string name; };
struct EvCraft { std::string name; int qty; };
struct EvTalentUnlock { std::string name; };
struct EvSleepStart {};
struct EvSleepEnd { int hours; };
struct EvSleepFail { std::string reason; };
struct EvToast { std::string text, type; };  // type: info, warn, rare, sell, death
struct EvDeath {};

using GameEvent = std::variant<EvWithdrawal, EvLevelUp, EvDisease, EvDiseaseCured, EvCraft,
                               EvTalentUnlock, EvSleepStart, EvSleepEnd, EvSleepFail, EvToast, EvDeath>;

class EventBus {
public:
    void emit(GameEvent e) { queue_.push_back(std::move(e)); }
    // Entrega y vacía los eventos pendientes, en orden de emisión.
    std::vector<GameEvent> drain() { return std::exchange(queue_, {}); }
    bool empty() const { return queue_.empty(); }

private:
    std::vector<GameEvent> queue_;
};

}  // namespace bazuko
