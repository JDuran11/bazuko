#include <SDL3/SDL.h>

#include <cstdio>

#include "core/FrameLoop.h"
#include "core/FrameMetrics.h"

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Bazuko", 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    bazuko::FrameMetrics metrics;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    double titleTimer = 0;
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const double realDt = static_cast<double>(now - last) / freq;
        last = now;
        const double dt = bazuko::clampDt(realDt);
        metrics.sample(static_cast<double>(now) * 1000.0 / freq);

        // Fase 0: sin render todavía; la limpieza de pantalla llega con la capa GL (fase 4).
        SDL_GL_SwapWindow(window);

        titleTimer += dt;
        if (titleTimer >= 0.5) {
            titleTimer = 0;
            char title[96];
            std::snprintf(title, sizeof title, "Bazuko - %.0f FPS (%.1f ms)",
                          metrics.current(), metrics.frameMs());
            SDL_SetWindowTitle(window, title);
        }
    }

    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
