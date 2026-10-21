#pragma once

#include <memory>
#include <string>

namespace bazuko {

struct Options {
    int width = 1280, height = 720;
    bool isometric = false;
    std::string screenshot;  // si no está vacío: renderiza `frames` cuadros, guarda el PNG y sale
    int frames = 30;
    double hour = -1;        // < 0: la del reloj del juego
    double posX = -1e9, posZ = -1e9;  // posición inicial en tiles (opcional)
    double yaw = -1e9;
    unsigned seed = 0;       // 0 = aleatoria
    bool showHelp = false;
};

// Bucle principal: ventana SDL3, entrada, simulación, mundo y render.
class Game {
public:
    explicit Game(const Options& options);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    int run();  // código de salida

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace bazuko
