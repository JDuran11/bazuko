#include "game/CameraRig.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bazuko {

namespace {

constexpr Vec3 kOffset{12, 16, 12};
constexpr Vec3 kLookOffset{0, 1.5, 0};
constexpr double kLerpSpeed = 6;

bool contains(const Aabb& b, const Vec3& p) {
    return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
}

// Distancia a la que el rayo entra en la caja (origen fuera), o -1 si no la toca.
double rayBox(const Vec3& o, const Vec3& d, const Aabb& b) {
    double tmin = -std::numeric_limits<double>::infinity(), tmax = std::numeric_limits<double>::infinity();
    const double os[3] = {o.x, o.y, o.z}, ds[3] = {d.x, d.y, d.z};
    const double lo[3] = {b.min.x, b.min.y, b.min.z}, hi[3] = {b.max.x, b.max.y, b.max.z};
    for (int i = 0; i < 3; ++i) {
        if (ds[i] == 0) {
            if (os[i] < lo[i] || os[i] > hi[i]) return -1;
            continue;
        }
        double t0 = (lo[i] - os[i]) / ds[i], t1 = (hi[i] - os[i]) / ds[i];
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
    }
    if (tmax < tmin || tmax < 0) return -1;
    return tmin >= 0 ? tmin : tmax;
}

}  // namespace

CameraMode CameraRig::toggle(double facing) {
    mode = mode == CameraMode::ThirdPerson ? CameraMode::Isometric : CameraMode::ThirdPerson;
    if (mode == CameraMode::ThirdPerson) yaw = facing + kPi;  // detrás de la dirección del personaje
    snap_ = true;
    return mode;
}

void CameraRig::orbit(double dx, double dy) {
    yaw -= dx * 0.005;
    pitch = std::clamp(pitch + dy * 0.005, 0.05, 0.65);
}

void CameraRig::zoom(double delta) { distance = std::clamp(distance + delta * 0.005, 2.5, 9.0); }

void CameraRig::arrowOrbit(double dt, int horizontal, int vertical) {
    if (mode != CameraMode::ThirdPerson) return;
    orbit(horizontal * dt * 250, vertical * dt * 180);
}

void CameraRig::forwardXZ(double& x, double& z) const {
    // La cámara mira de su posición hacia el objetivo: −(sin yaw, cos yaw) en el plano.
    x = -std::sin(yaw);
    z = -std::cos(yaw);
}

void CameraRig::follow(const Vec3& target, double dt, const CollisionWorld* collisions) {
    const Vec3 look{target.x + kLookOffset.x, target.y + kLookOffset.y, target.z + kLookOffset.z};
    Vec3 dir{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};

    if (mode == CameraMode::Isometric) {
        const Vec3 desired{target.x + kOffset.x, target.y + kOffset.y, target.z + kOffset.z};
        if (snap_) {
            position = desired;
        } else {
            const double k = 1 - std::exp(-kLerpSpeed * dt);
            position = {position.x + (desired.x - position.x) * k, position.y + (desired.y - position.y) * k,
                        position.z + (desired.z - position.z) * k};
        }
        lookAt = look;
    } else {
        // Acorta el brazo de cámara antes de una pared; el margen protege el plano cercano.
        double d = distance;
        if (collisions) {
            for (const Aabb& raw : collisions->boxes()) {
                const Aabb box{{raw.min.x - 0.2, raw.min.y - 0.2, raw.min.z - 0.2},
                               {raw.max.x + 0.2, raw.max.y + 0.2, raw.max.z + 0.2}};
                if (contains(box, look)) { d = 0; break; }
                const double t = rayBox(look, dir, box);
                if (t >= 0) d = std::min(d, std::max(0.0, t - 0.05));
            }
        }
        // Se acerca de inmediato ante obstáculos y vuelve suavemente al alejarse.
        if (snap_ || d < arm_) arm_ = d;
        else arm_ += (d - arm_) * (1 - std::exp(-8 * dt));
        position = {look.x + dir.x * arm_, look.y + dir.y * arm_, look.z + dir.z * arm_};
        // Orientación definida incluso si un obstáculo obliga a acercarse hasta el punto de mira.
        lookAt = {look.x - dir.x, look.y - dir.y, look.z - dir.z};
    }
    snap_ = false;
}

}  // namespace bazuko
