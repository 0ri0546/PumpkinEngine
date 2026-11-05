module Pumpkin.Demo.Helpers;

import Pumpkin.Renderer;
import Pumpkin.Math.Quaternion;
import std;

namespace Pumpkin::Demo {
    std::size_t EcsWorkerCount() {
        return std::max<std::size_t>(1, std::thread::hardware_concurrency() / 4);
    }

    const char* ShaderName(ShaderId id) {
        switch (id) {
        case ShaderId::Lambert:    return "Lambert";
        case ShaderId::BlinnPhong: return "BlinnPhong";
        case ShaderId::Toon:       return "Toon";
        case ShaderId::PBR:        return "PBR";
        default:                   return "other";
        }
    }

    Quaternionf MakeYawPitch(float yaw, float pitch) {
        const float sy = std::sin(yaw * 0.5f),   cy = std::cos(yaw * 0.5f);
        const float sx = std::sin(pitch * 0.5f), cx = std::cos(pitch * 0.5f);
        // qY * qX, written out so only the raw (x, y, z, w) constructor is needed.
        return Quaternionf{ cy * sx, sy * cx, -sy * sx, cy * cx }.Normalized();
    }

    void LoadTextureOrWarn(Texture& texture, const char* path) {
        if (!LoadTexture(texture, path)) {
            std::println("Impossible to load texture: {}", path);
        }
    }
}
