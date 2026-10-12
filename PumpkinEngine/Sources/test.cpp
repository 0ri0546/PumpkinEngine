#include <cstdint>
#include <vector>
#include <thread>
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// forces 8 bit colors
struct alignas(8) Color {
    uint8_t r {0};
    uint8_t g {0};
    uint8_t b {0};
    uint8_t a {255};
};

struct Framebuffer {
    std::vector<Color> pixels{};
	uint16_t width {800}, height {600};
};

struct DepthBuffer {
    std::vector<float> pixels{};
	uint16_t width {800}, height {600};
};

int main() {
    std::unique_ptr<Framebuffer> fb = std::make_unique<Framebuffer>();
	fb->pixels.reserve(static_cast<size_t>(fb->width * fb->height));
	std::unique_ptr<DepthBuffer> db = std::make_unique<DepthBuffer>();
	db->pixels.reserve(static_cast<size_t>(db->width * db->height));
	
	SDL_Init(SDL_INIT_VIDEO);

  SDL_Window* Window{SDL_CreateWindow(
    "Hello Window", 800, 300, 0
  )};

  bool IsRunning = true;
  SDL_Event Event;
  while (IsRunning) {
    while (SDL_PollEvent(&Event)) {
      if (Event.type == SDL_EVENT_QUIT) {
        IsRunning = false;
      }
    }
  }

  SDL_DestroyWindow(Window);
  SDL_Quit();
	
	std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";
	return 0;
}
