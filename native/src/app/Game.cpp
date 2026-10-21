#include "app/Game.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include "core/AdaptiveResolution.h"
#include "core/FrameLoop.h"
#include "core/FrameMetrics.h"
#include "core/Input.h"
#include "core/Rng.h"
#include "game/CameraRig.h"
#include "game/GameState.h"
#include "game/Lighting.h"
#include "game/PlayerMotor.h"
#include "render/BuildingVisibility.h"
#include "render/LocalLights.h"
#include "render/Renderer.h"
#include "render/VisualWorld.h"
#include "world/Bogota.h"
#include "world/BusService.h"
#include "world/PedestrianAgent.h"
#include "world/TrafficSystem.h"

namespace bazuko {

namespace {

Key mapKey(SDL_Scancode sc, bool& ok) {
    ok = true;
    switch (sc) {
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_LSHIFT: return Key::ShiftLeft;
        case SDL_SCANCODE_RSHIFT: return Key::ShiftRight;
        case SDL_SCANCODE_UP: return Key::ArrowUp;
        case SDL_SCANCODE_DOWN: return Key::ArrowDown;
        case SDL_SCANCODE_LEFT: return Key::ArrowLeft;
        case SDL_SCANCODE_RIGHT: return Key::ArrowRight;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_SPACE: return Key::Space;
        default: ok = false; return Key::Count;
    }
}

// Personaje de cajas (se sustituye por los modelos reales en la fase 7).
void personBoxes(std::vector<DynBox>& out, const Vec3& pos, double rotY, int variant, float scale, bool silhouette = false) {
    static const std::uint32_t jackets[] = {0xc0392b, 0x2f6fb0, 0x3f7a3a, 0x7d8288, 0xe0962b, 0x7a4fa0, 0x1f1f1f, 0xe8e3d6};
    static const std::uint32_t pants[] = {0x33445c, 0x1f1f1f, 0x6b4a2e, 0x2f6fb0};
    static const std::uint32_t skins[] = {0xf6d3b3, 0xe8b88f, 0xd39a6d, 0xb07a52, 0x8a5a3b, 0x5e3b27};
    const glm::vec3 p(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z));
    const float r = static_cast<float>(rotY);
    auto add = [&](glm::vec3 c, glm::vec3 s, std::uint32_t hex) {
        out.push_back({p, r, c * scale, s * scale, rgbFromHex(hex), silhouette});
    };
    add({0, 0.45f, 0}, {0.4f, 0.9f, 0.3f}, pants[variant % 4]);
    add({0, 1.2f, 0}, {0.5f, 0.7f, 0.3f}, jackets[variant % 8]);
    add({0, 1.7f, 0}, {0.3f, 0.3f, 0.3f}, skins[(variant / 3) % 6]);
    add({0, 1.9f, 0}, {0.32f, 0.1f, 0.34f}, jackets[(variant + 3) % 8]);
    add({0, 1.7f, 0.17f}, {0.2f, 0.06f, 0.06f}, 0x2a1a10);  // marca el frente (+Z)
}

}  // namespace

struct Game::Impl {
    Options opts;
    SDL_Window* window = nullptr;
    SDL_GLContext gl = nullptr;

    GameData data;
    Bogota city;
    CollisionWorld collisions;
    PedestrianNavigation nav;
    Mt19937Rng rng;
    TrafficSystem traffic;
    BusService bus;
    std::vector<PedestrianAgent> agents;
    GameState state;
    PlayerMotor motor;
    CameraRig rig;
    Input input;
    FrameMetrics metrics;
    std::unique_ptr<Renderer> renderer;
    VisualWorld visual;
    std::unique_ptr<BuildingVisibility> visibility;
    std::unique_ptr<LocalLights> lights;
    AdaptiveResolution adaptive;
    double deviceRatio = 1, simTime = 0;

    bool rightDrag = false;

    explicit Impl(const Options& o)
        : opts(o),
          data(GameData::load(std::string(BAZUKO_DATA_DIR) + "/game_data.json")),
          city(),
          collisions(city.buildCollisions()),
          nav(city.createPedestrianNavigation()),
          rng(o.seed ? o.seed : std::random_device{}()),
          traffic(city.trafficRoutes(), 4, rng),
          bus(city.busRoute()),
          state(data) {
        for (const Vec3& p : city.pedestrianSpawns(nav)) {
            AgentOptions ao;
            ao.navigation = &nav;
            ao.isWalkable = [this](double x, double z) { return nav.isWalkableAt(x, z); };
            ao.groundHeight = [this](double x, double z) { return nav.groundHeightAt(x, z); };
            agents.emplace_back(p, ao, rng);
        }
        const auto& spawn = city.reserved().spawn;
        motor.position = {spawn.x, city.groundHeightAt(spawn.x, spawn.z), spawn.z};
        if (opts.posX > -1e8) {
            const double mx = opts.atOriginal ? streetX().world(opts.posX) : opts.posX * kTileSize;
            const double mz = opts.atOriginal ? streetZ().world(opts.posZ) : opts.posZ * kTileSize;
            motor.position = {mx, city.groundHeightAt(mx, mz), mz};
        }
        if (opts.isometric) rig.toggle();
        if (opts.yaw > -1e8) rig.yaw = opts.yaw;
        if (opts.hour >= 0) state.clock.hour = opts.hour;
    }

    void initWindow() {
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        window = SDL_CreateWindow("Bazuko", opts.width, opts.height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
        gl = SDL_GL_CreateContext(window);
        if (!gl) throw std::runtime_error(std::string("SDL_GL_CreateContext: ") + SDL_GetError());
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
            throw std::runtime_error("no se pudieron cargar las funciones de OpenGL");
        SDL_GL_SetSwapInterval(opts.screenshot.empty() ? 1 : 0);
        std::printf("OpenGL %s · %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    }

    ~Impl() {
        renderer.reset();  // antes de destruir el contexto
        if (gl) SDL_GL_DestroyContext(gl);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void handleEvent(const SDL_Event& e, bool& running) {
        switch (e.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                if (e.key.repeat) break;
                bool ok;
                const Key k = mapKey(e.key.scancode, ok);
                if (ok) input.setKey(k, e.type == SDL_EVENT_KEY_DOWN);
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST: input.releaseAll(); rightDrag = false; break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: renderer->resize(e.window.data1, e.window.data2); break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_RIGHT && rig.mode == CameraMode::ThirdPerson) rightDrag = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_RIGHT) rightDrag = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (rightDrag && rig.mode == CameraMode::ThirdPerson) rig.orbit(e.motion.xrel, e.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (rig.mode == CameraMode::ThirdPerson) rig.zoom(-e.wheel.y * 100.0);
                break;
            default: break;
        }
    }

    void update(double dt) {
        if (input.consumePress(Key::Escape)) SDL_PushEvent(new SDL_Event{.type = SDL_EVENT_QUIT});
        if (input.consumePress(Key::V)) {
            rig.toggle(motor.rotationY);
            rightDrag = false;
        }
        rig.arrowOrbit(dt, (input.isDown(Key::ArrowRight) ? 1 : 0) - (input.isDown(Key::ArrowLeft) ? 1 : 0),
                       (input.isDown(Key::ArrowDown) ? 1 : 0) - (input.isDown(Key::ArrowUp) ? 1 : 0));

        state.update(dt);
        if (opts.hour >= 0) state.clock.hour = opts.hour;  // hora fija para capturas

        double fx, fz;
        rig.forwardXZ(fx, fz);
        motor.update(dt, input.axis(), input.isRunning(), rig.mode, fx, fz, state.stats.getSpeedMultiplier(), &collisions);
        motor.position.y = city.groundHeightAt(motor.position.x, motor.position.z);
        rig.follow(motor.position, dt, &collisions);

        // Cámara: posición y vector "derecha" horizontal, para los rayos de BuildingVisibility.
        {
            Vec3 f{rig.lookAt.x - rig.position.x, 0, rig.lookAt.z - rig.position.z};
            const double len = std::max(1e-9, std::hypot(f.x, f.z));
            visibility->update(dt, rig.mode == CameraMode::Isometric, rig.position, {-f.z / len, 0, f.x / len}, motor.position);
        }

        std::vector<Vec3> people;
        people.reserve(agents.size());
        for (auto& a : agents) {
            a.update(dt, rng);
            people.push_back(a.position);
        }
        simTime += dt;
        lights->update(simTime, state.clock.isNight(), motor.position,
                       [this](double x, double z) { return city.groundHeightAt(x, z); }, [](const Aabb&) { return true; });
        traffic.update(dt, &motor.position, people);
        bus.update(dt, &motor.position, people);
        input.endFrame();
    }

    FrameInput buildFrame(int w, int h) {
        FrameInput f;
        f.camera = &rig;
        f.aspect = static_cast<double>(w) / std::max(1, h);
        f.lighting = lightingAt(state.clock.hour);
        applyDesaturation(f.lighting, state.stats.getDesaturation());
        f.night = state.clock.isNight();
        f.lights = lights->pool();
        f.halos = &lights->halos();
        f.shadowTarget = motor.position;
        f.houseSeparate = &visibility->separate();
        for (size_t h = 0; h < visual.houses.size(); ++h) {
            if (!visibility->separate()[visual.houses[h].id]) continue;
            for (int part = 0; part < BuildingVisibility::kParts; ++part) {
                const double o = visibility->opacity(h, part);
                if (o > 0) f.parts.push_back({visual.houses[h].id, part, static_cast<float>(o)});
            }
        }

        personBoxes(f.boxes, motor.position, motor.rotationY, 0, 1.0f, true);

        for (size_t i = 0; i < agents.size(); ++i)
            personBoxes(f.boxes, agents[i].position, agents[i].rotationY, static_cast<int>(i * 7 + 3), 0.95f);

        for (const Vendor& v : city.vendors()) {
            const Vec3 back{v.x - std::sin(v.rotY) * 0.9, city.groundHeightAt(v.x, v.z), v.z - std::cos(v.rotY) * 0.9};
            personBoxes(f.boxes, back, v.rotY, static_cast<int>(v.x), 0.95f);
        }

        static const std::uint32_t paints[] = {0x2f6fb0, 0xc0392b, 0x7d8288, 0xe8e3d6, 0x1f1f1f, 0x3f7a3a, 0x7a4fa0, 0x0f6b6b};
        int idx = 0;
        for (const TrafficVehicle& v : traffic.vehicles()) {
            const bool taxi = idx % 4 == 0;
            const Rgb paint = rgbFromHex(taxi ? 0xf2c300 : paints[idx % 8]);
            const glm::vec3 pos(static_cast<float>(v.position.x), 0, static_cast<float>(v.position.z));
            f.boxes.push_back({pos, static_cast<float>(v.rotationY), {0, 0.55f, 0}, {4.3f, 0.9f, 1.76f}, paint});
            f.boxes.push_back({pos, static_cast<float>(v.rotationY), {-0.25f, 1.25f, 0}, {2.2f, 0.6f, 1.55f}, rgbFromHex(0x9fb4c4)});
            ++idx;
        }
        if (bus.opacity() > 0)
            f.boxes.push_back({{static_cast<float>(bus.position.x), 0, static_cast<float>(bus.position.z)}, 0,
                               {0, 1.7f, 0}, {static_cast<float>(kBusWidth), 3.0f, static_cast<float>(kBusLength)}, rgbFromHex(0xb3232b)});
        return f;
    }

    bool savePng(const std::string& path) {
        int w, h;
        const auto px = renderer->readPixels(w, h);
        return stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4) != 0;
    }

    int run() {
        initWindow();
        renderer = std::make_unique<Renderer>();
        int pw, ph;
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        renderer->resize(pw, ph);
        renderer->setRenderScale(opts.renderScale);
        deviceRatio = std::max(1.0, static_cast<double>(SDL_GetWindowPixelDensity(window)));
        adaptive = AdaptiveResolution(deviceRatio);
        visual = buildVisualWorld(city, rng);
        visibility = std::make_unique<BuildingVisibility>(visual.houses);
        lights = std::make_unique<LocalLights>(visual.lights);
        renderer->uploadWorld(visual);

        const bool shot = !opts.screenshot.empty();
        Uint64 last = SDL_GetPerformanceCounter();
        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        double titleTimer = 0;
        int frame = 0;
        bool running = true;
        RenderStats stats;

        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) handleEvent(e, running);

            const Uint64 now = SDL_GetPerformanceCounter();
            const double real = static_cast<double>(now - last) / freq;
            last = now;
            const double dt = shot ? 1.0 / 60 : clampDt(real);
            metrics.sample(static_cast<double>(now) * 1000.0 / freq);
            if (!shot && adaptive.sample(real * 1000.0)) renderer->setRenderScale(adaptive.ratio() / deviceRatio);

            update(dt);
            SDL_GetWindowSizeInPixels(window, &pw, &ph);
            if (pw != 0) renderer->resize(pw, ph);
            stats = renderer->render(buildFrame(pw, ph));
            SDL_GL_SwapWindow(window);
            ++frame;

            titleTimer += dt;
            if (titleTimer >= 0.5 && !shot) {
                titleTimer = 0;
                char title[160];
                std::snprintf(title, sizeof title, "Bazuko - Día %d %02d:%02d - %.0f FPS (%.1f ms) - %d dibujos, %zu tri - %.0f%%",
                              state.clock.day, static_cast<int>(state.clock.hour),
                              static_cast<int>(state.clock.hour * 60) % 60, metrics.current(), metrics.frameMs(),
                              stats.drawCalls, stats.triangles, renderer->renderScale() * 100);
                SDL_SetWindowTitle(window, title);
            }
            if (shot && frame >= opts.frames) {
                stats = renderer->render(buildFrame(pw, ph));
                // Se vuelve a dibujar sin intercambiar para leer el framebuffer trasero.
                renderer->render(buildFrame(pw, ph));
                if (!savePng(opts.screenshot)) {
                    std::fprintf(stderr, "no se pudo escribir %s\n", opts.screenshot.c_str());
                    return 1;
                }
                std::printf("captura: %s (%dx%d, %d dibujos (%d de sombra), %zu triángulos, %d lotes visibles / %d descartados, %d halos)\n", opts.screenshot.c_str(), pw, ph,
                            stats.drawCalls, stats.shadowDrawCalls, stats.triangles, stats.batchesVisible, stats.batchesCulled, stats.halos);
                break;
            }
        }
        return 0;
    }
};

Game::Game(const Options& options) : impl_(std::make_unique<Impl>(options)) {}
Game::~Game() = default;
int Game::run() { return impl_->run(); }

}  // namespace bazuko
