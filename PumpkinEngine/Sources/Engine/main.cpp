#include <cstdint>
#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <numbers>
#include <algorithm>
#include <bit>
#include <string>

#include <SDL3/SDL.h>

// forces 8 bit colors
struct alignas(4) Color {
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
	return static_cast<float>(std::chrono::duration_cast<milliseconds>(steady_clock::now() - start).count());
}

using float2 = std::pair<float, float>;
using uint32_t2 = std::pair<uint32_t, uint32_t>;

static float Dot(const float2& a, const float2& b) {
	return a.first * b.first + a.second * b.second;
}

static float Distance(const float2& a, const float2& b) {
	return static_cast<float>(std::sqrt(std::pow(b.first - a.first, 2) + std::pow(b.second - a.second, 2)));
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
	const std::vector<Color>& colors,
	uint32_t* pixels)
{
	auto edge = [](const uint32_t2& a, const uint32_t2& b, float px, float py) {
		return (px - a.first) * ((float)b.second - a.second)
			- (py - a.second) * ((float)b.first - a.first);
		};

	const float area = edge(vertices[0], vertices[1], (float)vertices[2].first, (float)vertices[2].second);

	for (uint32_t y = group.first.second; y < group.second.second; ++y) {
		for (uint32_t x = group.first.first; x < group.second.first; ++x) {
#pragma warning(push)
#pragma warning(disable : 4244)
			if (PointOnTriangle(vertices[0], vertices[1], vertices[2], {x, y})) {
#pragma warning(pop)
				// how much each vertex contributes at this pixel (sums to 1)
				const float w0 = edge(vertices[1], vertices[2], (float)x, (float)y) / area;
				const float w1 = edge(vertices[2], vertices[0], (float)x, (float)y) / area;
				const float w2 = 1.0f - w0 - w1;

				pixels[y * 1920 + x] = ToRGBA(
					(w0 * colors[0].r + w1 * colors[1].r + w2 * colors[2].r) / 255.0f,
					(w0 * colors[0].g + w1 * colors[1].g + w2 * colors[2].g) / 255.0f,
					(w0 * colors[0].b + w1 * colors[1].b + w2 * colors[2].b) / 255.0f);
			}
		}
	}
}

void PixelShader(const std::unique_ptr<Framebuffer>& fb, const std::vector<uint32_t2>& vertices, const std::vector<Color>& colors, uint32_t* pixels) {
	std::vector<std::jthread> threads{};
	const unsigned threadCount = std::max(1u, std::thread::hardware_concurrency()) - 2;

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
		threads.emplace_back([group, &vertices, &pixels, &colors]() {
			Rasterize(group, vertices, colors, pixels);
		});
	}
	for (auto& thread : threads) {
		thread.join();
	}
}

void VertexShader(const std::unique_ptr<Framebuffer>& fb, std::vector<uint32_t2>& vertices, const std::vector<Color>& colors, uint32_t* pixels) {
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
	PixelShader(fb, vertices, colors, pixels);
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
	std::vector<Color> colors{
		{ 0, 0, 255, 255 },  // top
		{ 0, 255, 0, 255 },  // bottom left
		{ 255, 0, 0, 255 }   // bottom right
	};

	std::cout << "Max threads: " << std::thread::hardware_concurrency() << "\n";

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window{ SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, 0) };
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	surface->format = SDL_PIXELFORMAT_RGBA32;
	memset(surface->pixels, 0xFF, fb->width * fb->height * sizeof(uint32_t));


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

	bool IsRunning = true;
	SDL_Event Event;
	uint64_t lastTime = SDL_GetTicks();
	uint32_t frames = 0;
	while (IsRunning) {
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_EVENT_QUIT) {
				IsRunning = false;
			}
		}
		std::fill_n(static_cast<uint32_t*>(surface->pixels), fb->width * fb->height, wave(GetEngineTime() * 0.001f));
		VertexShader(fb, vertices, colors, static_cast<uint32_t*>(surface->pixels));
		SDL_UpdateWindowSurface(window);

		++frames;
		const uint64_t now = SDL_GetTicks();
		if (now - lastTime >= 1000) {
			const std::string title = "Pumpkin Engine | FPS: " + std::to_string(frames);
			SDL_SetWindowTitle(window, title.c_str());
			frames = 0;
			lastTime = now;
		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
