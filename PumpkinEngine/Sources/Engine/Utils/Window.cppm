module;

#include <SDL_FixedIncludes.h>

export module Pumpkin.Utils.Window;

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.Renderer;
import Pumpkin.Renderer.Types;

export namespace Pumpkin {
    // Sets the 16 / 32 / 64 px Pumpkin icons on the window (logs and returns on failure).
    void SetMultiResWindowIcon(SDL_Window* window);

    // True when the window is minimized or has lost the input focus.
    [[nodiscard]] bool WindowNotFocused(SDL_Window* window);

    // Resizes the renderer, the frame / depth buffers and the camera aspect ratio.
    // Ignored for a zero-sized window (e.g. while minimizing).
    void ResizeBuffers(Renderer& renderer, FrameBuffer& fb, DepthBuffer& db,
                       int width, int height);
}
