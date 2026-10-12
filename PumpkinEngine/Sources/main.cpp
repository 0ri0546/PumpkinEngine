#include <cstdint>
#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <numbers>
#include <algorithm>
#include <bit>

#include <SDL3/SDL.h>

// forces 8 bit colors
struct alignas(4) Pixel {
	uint8_t r{ 0 };
	uint8_t g{ 0 };
	uint8_t b{ 0 };
	uint8_t a{ 255 };
};

struct Framebuffer {
	std::vector<Pixel> pixels{};
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

using float2 = std::pair<float, float>;
using uint32_t2 = std::pair<uint32_t, uint32_t>;

static float Dot(const float2& a, const float2& b) {
	return a.first * b.first + a.second * b.second;
}

static float2 Perpendicular(const float2& v) {
	return float2(v.second, -v.first);
}

static bool OnRightSide(const float2& a, const float2& b, const float2& p) {
	float2 ap = float2(p.first - a.first, p.second - a.second);
	float2 abPerp = Perpendicular(float2(b.first - a.first, b.second - a.second));
	return Dot(ap, abPerp) >= 0;
}

static bool PointOnTriangle(const float2& a, const float2& b, const float2& c, const float2& p) {
	return OnRightSide(a, b, p) && OnRightSide(b, c, p) && OnRightSide(c, a, p);
}

static uint32_t ToRGBA(float r, float g, float b, float a = 1.0f) {
	return std::byteswap(
		(static_cast<uint32_t>(r * 255) << 24) + 
		(static_cast<uint32_t>(g * 255) << 16) + 
		(static_cast<uint32_t>(b * 255) << 8) + 
		(static_cast<uint32_t>(a * 255))
	);
}

void Rasterize(
	const std::pair<uint32_t2, uint32_t2>& group,
	const std::vector<uint32_t2>& vertices,
	uint32_t* pixels)
{
	for (uint32_t y = group.first.second; y < group.second.second; ++y) {
		for (uint32_t x = group.first.first; x < group.second.first; ++x) {
			if (PointOnTriangle(vertices[0], vertices[1], vertices[2], { x, y })) {
				pixels[y * 1920 + x] = ToRGBA(x / 1920.0f, y / 1080.0f, 0.0f);
			}
		}
	}
	const uint32_t p = pixels[1080 / 2 * 1920 + 1920 / 2] << '\n';
	std::cout << "AAAAA(R: " << (p & 0xFF) << " G: " << (p & 0x00FF) << " B: " << (p & 0x0000FF) << ")\n";
}

void PixelShader(const std::unique_ptr<Framebuffer>& fb, const std::vector<uint32_t2>& vertices, uint32_t* pixels) {
	// rasterize
	std::vector<std::jthread> threads{};
	const size_t threadCount = std::max(1u, std::thread::hardware_concurrency()) - 2;

	std::vector<std::pair<uint32_t2, uint32_t2>> groups{};
	groups.reserve(threadCount);

	const uint32_t rowsPerGroup = fb->height / threadCount;
	const uint32_t remainder = fb->height % threadCount;

	uint32_t y = 0;

	for (size_t i = 0; i < threadCount; ++i) {
		const uint32_t rows = rowsPerGroup + (i < remainder ? 1 : 0);

		groups.push_back({
			{ 0, y },
			{ fb->width, y + rows }
		});

		y += rows;
	}

	for (const auto group : groups) {
		threads.emplace_back([group, &vertices, &pixels]() {
			Rasterize(group, vertices, pixels);
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
}

void VertexShader(const std::unique_ptr<Framebuffer>& fb, std::vector<uint32_t2>& vertices, uint32_t* pixels) {
	static constexpr float a = 16.0f / 9.0f;
	std::vector<std::pair<float, float>> result{};
	result.reserve(vertices.size());
	int c = 0;
	for (const auto& vertex : vertices) {
		result.emplace_back(
			static_cast<float>(vertex.first) / (fb->width / 2.0f) - 1.0f, 
			static_cast<float>(vertex.second) / (fb->height / 2.0f) - 1.0f
		);
	}
	PixelShader(fb, vertices, pixels);
}

int main() {
	GetEngineTime();
	std::unique_ptr<Framebuffer> fb = std::make_unique<Framebuffer>();
	fb->pixels.reserve(static_cast<size_t>(fb->width * fb->height));
	std::unique_ptr<DepthBuffer> db = std::make_unique<DepthBuffer>();
	db->pixels.reserve(static_cast<size_t>(db->width * db->height));

	std::vector<std::pair<uint32_t, uint32_t>> vertices{
		{fb->width / 2, 0},
		{0, fb->height},
		{fb->width, fb->height}
	};

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window{ SDL_CreateWindow(
	  "Hello Window", fb->width, fb->height, SDL_WINDOW_FULLSCREEN
	) };
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	surface->format = (SDL_PixelFormat)SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBA, SDL_PACKEDLAYOUT_8888, 32, 4);
	memset(surface->pixels, 0xFF, fb->width * fb->height * sizeof(uint32_t));
	auto pixels = static_cast<uint32_t*>(surface->pixels);
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

			return std::byteswap((r << 24) | (g << 16) | (b << 8) | a); // 0xRRGGBBAA
		};

	while (IsRunning) {
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_EVENT_QUIT) {
				IsRunning = false;
			}
		}
		//std::fill_n(static_cast<uint32_t*>(surface->pixels), fb->width * fb->height, wave(GetEngineTime() * 0.001));

		//std::fill_n(static_cast<uint32_t*>(surface->pixels), fb->width * 200, std::byteswap(0x00FF00FFu));

		VertexShader(fb, vertices, static_cast<uint32_t*>(surface->pixels));
		const uint32_t p = static_cast<uint32_t*>(surface->pixels)[fb->height / 2 * 1920 + fb->width / 2] << '\n';
		std::cout << "(R: " << (p & 0xFF) << " G: " << (p & 0x00FF) << " B: " << (p & 0x0000FF) << ")\n";
		SDL_UpdateWindowSurface(window);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";
	return 0;
}
