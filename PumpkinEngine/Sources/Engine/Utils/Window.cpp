module;

#include <SDL3/SDL.h>

module Pumpkin.Utils.Window;

import std;

namespace Pumpkin {
    void SetMultiResWindowIcon(SDL_Window* window) {
        SDL_Surface* baseIcon = SDL_LoadPNG("Assets/Icons/Pumpkin@32.png");
        SDL_Surface* icon16   = SDL_LoadPNG("Assets/Icons/Pumpkin@16.png");
        SDL_Surface* icon64   = SDL_LoadPNG("Assets/Icons/Pumpkin@64.png");

        if (!baseIcon || !icon16 || !icon64) {
            SDL_Log("Couldn't load icons : %s", SDL_GetError());
            // SDL_DestroySurface(nullptr) is a safe no-op; release whatever did load.
            SDL_DestroySurface(baseIcon);
            SDL_DestroySurface(icon16);
            SDL_DestroySurface(icon64);
            return;
        }

        // The alternate images are ref-counted by SDL, so our handles can go right away.
        SDL_AddSurfaceAlternateImage(baseIcon, icon16);
        SDL_DestroySurface(icon16);
        SDL_AddSurfaceAlternateImage(baseIcon, icon64);
        SDL_DestroySurface(icon64);

        if (!SDL_SetWindowIcon(window, baseIcon)) {
            SDL_Log("Error while setting the window icon : %s", SDL_GetError());
        }

        SDL_DestroySurface(baseIcon);
    }

    bool WindowNotFocused(SDL_Window* window) {
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
        return !(flags & SDL_WINDOW_INPUT_FOCUS) || (flags & SDL_WINDOW_MINIMIZED);
    }

    void ResizeBuffers(Renderer& renderer, FrameBuffer& fb, DepthBuffer& db,
                       int width, int height) {
        if (width <= 0 || height <= 0) return;

        renderer.Resize(width, height);
        fb.width  = width;
        fb.height = height;
        db.width  = width;
        db.height = height;
        db.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 1.0f);
    }
}
