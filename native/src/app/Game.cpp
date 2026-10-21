#include "app/Game.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include "character/Character.h"
#include "character/ProceduralCharacter.h"
#include "core/AdaptiveResolution.h"
#include "core/FrameLoop.h"
#include "core/FrameMetrics.h"
#include "core/Input.h"
#include "core/Rng.h"
#include "app/EntityModels.h"
#include "game/Entities.h"
#include "game/Interaction.h"
#include "game/Places.h"
#include "game/systems/Survival.h"
#include "game/CameraRig.h"
#include "game/GameState.h"
#include "game/Lighting.h"
#include "game/PlayerMotor.h"
#include "render/BuildingVisibility.h"
#include "render/CameraMatrices.h"
#include "render/Frustum.h"
#include "render/LocalLights.h"
#include "render/Renderer.h"
#include "render/VisualWorld.h"
#include "world/Bogota.h"
#include "world/BusService.h"
#include "world/PedestrianAgent.h"
#include "world/TrafficSystem.h"

namespace bazuko {

namespace {

// BAZUKO_PROFILE=1 imprime lo que tarda cada etapa del arranque.
struct StageTimer {
    const char* name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    explicit StageTimer(const char* n) : name(n) {}
    ~StageTimer() {
        if (!std::getenv("BAZUKO_PROFILE")) return;
        std::fprintf(stderr, "  %-10s %6.0f ms\n", name, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
};
#define BAZUKO_TIMER(name) StageTimer bazukoTimer_(name)

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
    Entities entities;
    std::unique_ptr<Interaction> interaction;
    std::unique_ptr<EntityModels> models;
    double playerActionLeft = 0;
    std::string playerAction;

    // Personajes reales: el jugador, la gente de la calle y los vendedores. Cada peatón guarda su versión
    // liviana y la completa; se muestra una u otra según la distancia (con histéresis).
    struct Person {
        std::unique_ptr<Character> lod, full;
        bool showFull = false, onScreen = true;
        int frame = 0;
        double animAcc = 0;
    };
    Catalog catalog;
    std::unique_ptr<CharacterLibrary> library;
    std::unique_ptr<Character> playerChar;
    std::unique_ptr<ProceduralCharacter> playerProc;
    bool proceduralPlayer = false;
    double playerClock = 0;
    std::vector<Person> crowd;    // alineada con `agents`
    std::vector<Person> vendors;  // alineada con city.vendors()
    double aspect = 16.0 / 9;
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
          state(data),
          catalog(Catalog::load(std::string(BAZUKO_MODELS_DIR) + "/personaje/catalogo.json")) {
        for (const Vec3& p : city.pedestrianSpawns(nav)) {
            AgentOptions ao;
            ao.navigation = &nav;
            ao.isWalkable = [this](double x, double z) { return nav.isWalkableAt(x, z); };
            ao.groundHeight = [this](double x, double z) { return nav.groundHeightAt(x, z); };
            agents.emplace_back(p, ao, rng);
        }
        entities = spawnEntities(city, rng);
        interaction = std::make_unique<Interaction>(state, entities, city, rng);
        // Caja de colisión de la tienda (su fachada queda junto al punto de interacción).
        collisions.add({entities.shop.position.x, 0, entities.shop.position.z - EntityModels::kShopBack}, {5.2, 3.2, 3.6});
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

    static glm::mat4 characterModel(const Vec3& p, double rotY) {
        return glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z))),
                                      static_cast<float>(rotY), glm::vec3(0, 1, 0)),
                          glm::vec3(static_cast<float>(kRealisticScale)));
    }

    // LOD y animación de un peatón según su distancia al jugador (Npc.update): lejos usa la versión liviana, la
    // animación se reduce a un cuadro de cada tres pasados 20 m y se congela pasados 40 m si está quieto.
    void updatePerson(Person& p, const Vec3& pos, bool moving, double speed, double dt, const Frustum& view) {
        const double dist = std::hypot(pos.x - motor.position.x, pos.z - motor.position.z);
        const Aabb sphere{{pos.x - 1.4, pos.y + 0.9 - 1.4, pos.z - 1.4}, {pos.x + 1.4, pos.y + 0.9 + 1.4, pos.z + 1.4}};
        p.onScreen = view.intersects(sphere);
        p.animAcc += dt;
        if (!p.onScreen) return;  // fuera de cámara: ni se dibuja ni se anima
        if (!p.showFull && dist < 7) p.showFull = true;
        else if (p.showFull && dist > 10) p.showFull = false;
        ++p.frame;
        if (dist > 40 && !moving) return;
        if (dist > 20 && p.frame % 3 != 0) return;
        Character& c = p.showFull ? *p.full : *p.lod;
        c.update(std::min(p.animAcc, 0.2), {moving ? speed / kRealisticScale : 0.0, false, {}});
        p.animAcc = 0;
    }

    Person makePerson(const Look& look) {
        Person p;
        p.lod = std::make_unique<Character>(library->build(look, true));
        p.full = std::make_unique<Character>(library->build(look, false));
        // cada uno arranca en otro punto de la animación para que no se muevan al unísono
        const double t = rng.next() * 3;
        p.lod->update(t, {});
        p.full->update(t, {});
        return p;
    }

    // Hasta que exista la interfaz (fase 9) los eventos del juego se muestran en la consola.
    void drainEvents(double dt) {
        playerActionLeft = std::max(0.0, playerActionLeft - dt);
        for (const GameEvent& ev : state.bus.drain()) {
            if (const auto* a = std::get_if<EvPlayerAction>(&ev)) {
                playerAction = a->name;
                playerActionLeft = a->seconds;
            } else if (const auto* t = std::get_if<EvToast>(&ev)) {
                std::printf("[%s] %s\n", t->type.c_str(), t->text.c_str());
            } else if (const auto* r = std::get_if<EvBegResult>(&ev)) {
                std::printf("[mendigar] %s\n", r->message.c_str());
            } else if (const auto* p = std::get_if<EvPickup>(&ev)) {
                std::printf("[basura] %zu objetos%s%s\n", p->added.size(), p->rare ? " (¡raro!)" : "", p->sorted ? " (seleccionada)" : "");
            } else if (std::holds_alternative<EvOpenRecycler>(ev)) {
                std::printf("[recicladora] vendes %d\n", sellToRecycler(state, interaction->boostActive() ? 1.2 : 1.0));
            } else if (std::holds_alternative<EvOpenCambuche>(ev)) {
                if (state.hasBuilt("base")) {
                    const SleepResult r = sleep(state, rng);
                    std::printf("[cambuche] dormir: %s %.1f h\n", r.ok ? "ok" : "no", r.hours);
                }
            }
        }
        if (isDead(state)) {
            respawn(state);
            motor.position = {entities.cambuche.position.x, city.groundHeightAt(entities.cambuche.position.x, entities.cambuche.position.z),
                              entities.cambuche.position.z + 2};
            std::printf("[muerte] reapareces en el cambuche\n");
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
        interaction->update(dt, input, motor.position);
        updateEntities(entities, city, rng, dt);
        drainEvents(dt);
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
        if (input.consumePress(Key::M)) proceduralPlayer = !proceduralPlayer;
        playerClock += dt;
        if (proceduralPlayer) {
            ProceduralCharacter::Input pin;
            pin.speed = motor.speed / kProceduralScale;
            pin.running = motor.state == MoveState::Run;
            pin.t = playerClock;
            pin.dirt = std::max(0.0, (40 - state.stats.higiene) / 40);
            playerProc->update(dt, pin);
        } else {
            playerChar->update(dt, {motor.speed / kRealisticScale, motor.state == MoveState::Run, playerActionLeft > 0 ? playerAction : std::string()});
        }
        {
            const CameraMatrices cm = cameraMatrices(rig, aspect);
            const Frustum view = Frustum::fromViewProjection(cm.proj * cm.view);
            for (size_t i = 0; i < agents.size(); ++i) updatePerson(crowd[i], agents[i].position, agents[i].moving, agents[i].walkSpeed, dt, view);
            for (size_t i = 0; i < vendors.size(); ++i) {
                const Vendor& v = city.vendors()[i];
                updatePerson(vendors[i], {v.x - std::sin(v.rotY) * 0.9, city.groundHeightAt(v.x, v.z), v.z - std::cos(v.rotY) * 0.9}, false, 0, dt, view);
            }
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

        if (proceduralPlayer) {
            glm::mat4 m = characterModel(motor.position, motor.rotationY);
            m = glm::scale(m, glm::vec3(static_cast<float>(kProceduralScale / kRealisticScale)));
            playerProc->collect(m, true, f.skinned);
        } else {
            playerChar->collect(characterModel(motor.position, motor.rotationY), true, f.skinned);
        }
        for (size_t i = 0; i < crowd.size(); ++i) {
            if (!crowd[i].onScreen) continue;
            (crowd[i].showFull ? *crowd[i].full : *crowd[i].lod).collect(characterModel(agents[i].position, agents[i].rotationY), false, f.skinned);
        }
        for (size_t i = 0; i < vendors.size(); ++i) {
            if (!vendors[i].onScreen) continue;
            const Vendor& v = city.vendors()[i];
            const Vec3 back{v.x - std::sin(v.rotY) * 0.9, city.groundHeightAt(v.x, v.z), v.z - std::cos(v.rotY) * 0.9};
            (vendors[i].showFull ? *vendors[i].full : *vendors[i].lod).collect(characterModel(back, v.rotY), false, f.skinned);
        }

        InteractionView view;
        view.boost = interaction->boostActive();
        view.sortTime = data.c("SORT_BASE_TIME");
        models->collect(entities, state, motor.position, simTime, view, f.meshes);
        models->collectTraffic(traffic, bus, f.meshes);
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
        {
            BAZUKO_TIMER("personajes");
            library = std::make_unique<CharacterLibrary>(catalog, data.look, BAZUKO_MODELS_DIR, *renderer);
            Look playerLook;  // apariencia por defecto hasta que exista el creador
            playerChar = std::make_unique<Character>(library->build(playerLook, false));
            playerProc = std::make_unique<ProceduralCharacter>(playerLook, data.look, *renderer, opts.seed ? opts.seed : 1);
            proceduralPlayer = opts.procedural;
            for (size_t i = 0; i < agents.size(); ++i) crowd.push_back(makePerson(randomStreetLook(catalog, data.look, rng, true)));
            for (size_t i = 0; i < city.vendors().size(); ++i) vendors.push_back(makePerson(randomStreetLook(catalog, data.look, rng, false)));
        }
        renderer->uploadWorld(visual);
        models = std::make_unique<EntityModels>(*renderer);

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

            SDL_GetWindowSizeInPixels(window, &pw, &ph);
            aspect = static_cast<double>(pw) / std::max(1, ph);
            update(dt);
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
