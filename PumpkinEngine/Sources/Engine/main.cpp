#include <SDL_FixedIncludes.h>

import Pumpkin.Demo;
import Pumpkin.Utils;
import Pumpkin.UI;

import std;

using namespace Pumpkin;

int main() {
    GetEngineTime();
    UI::LoadUIFile("Assets/UI/MainMenu.yaml");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL could not initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    if (!TTF_Init()) {
        std::println(stderr, "SDL_ttf couldn't initialize! SDL Error: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Everything (renderer, UI, fonts, ECS world, assets) lives inside RunApplication(),
    // so it is all destroyed before SDL / SDL_ttf shut down, on every exit path.
    const int exitCode = Demo::RunApplication();

    TTF_Quit();
    SDL_Quit();

    return exitCode;
}
