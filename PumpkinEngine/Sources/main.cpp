#include <cstdint>
#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <numbers>
#include <algorithm>

#include <SDL3/SDL.h>

// forces 8 bit colors
struct alignas(8) Color {
	uint8_t r{ 0 };
	uint8_t g{ 0 };
	uint8_t b{ 0 };
	uint8_t a{ 255 };
};

struct Framebuffer {
	std::vector<Color> pixels{};
	uint16_t width{ 1920 }, height{ 1080 };
};

struct DepthBuffer {
	std::vector<float> pixels{};
	uint16_t width{ 1920 }, height{ 1080 };
};

static float GetEngineTime() {
	using namespace std::chrono;
	static const auto start = steady_clock::now();
	return duration_cast<milliseconds>(steady_clock::now() - start).count();
}

static uint32_t GetEngineSecond() {
	using namespace std::chrono;
	static const auto start = steady_clock::now();
	return duration_cast<seconds>(steady_clock::now() - start).count();
}

int main() {
	GetEngineTime();
	GetEngineSecond();
	std::unique_ptr<Framebuffer> fb = std::make_unique<Framebuffer>();
	fb->pixels.reserve(static_cast<size_t>(fb->width * fb->height));
	std::unique_ptr<DepthBuffer> db = std::make_unique<DepthBuffer>();
	db->pixels.reserve(static_cast<size_t>(db->width * db->height));

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window{ SDL_CreateWindow(
	  "Hello Window", fb->width, fb->height, SDL_WINDOW_FULLSCREEN
	) };
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	memset(surface->pixels, 0xFF, fb->width * fb->height * sizeof(uint32_t));
	auto pixels = reinterpret_cast<uint32_t*>(surface->pixels);
	std::cout << "Pixel 1 color = (R:" << (pixels[0] & 0xFF) << " G:" << (pixels[0] & 0x00FF) << " B:" << (pixels[0] & 0x0000FF) << ")\n";

	bool IsRunning = true;
	SDL_Event Event;

	auto wave = [](const float x) -> uint32_t
		{
			float twoThirdsPi = 2.0f * std::numbers::pi_v<float> / 3.0f;

			const auto channel = [](const float v) -> uint32_t {
				return static_cast<uint32_t>(0xFF * (0.5f + 0.5f * std::sin(v)));
				};

			const uint32_t r = channel(x);
			const uint32_t g = channel(x + twoThirdsPi);
			const uint32_t b = channel(x + 2.0f * twoThirdsPi);
			const uint32_t a = static_cast<uint32_t>(0xFF * (0.6f + 0.4f * std::sin(x * 0.5f)));

			return (r << 24) | (g << 16) | (b << 8) | a; // 0xRRGGBBAA, opaque
		};

	while (IsRunning) {
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_EVENT_QUIT) {
				IsRunning = false;
			}
		}
		std::fill_n(static_cast<uint32_t*>(surface->pixels), fb->width * fb->height, wave(GetEngineTime() * 0.001));
		SDL_UpdateWindowSurface(window);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";
	return 0;
}
