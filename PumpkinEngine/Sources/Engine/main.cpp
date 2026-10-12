#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include <Renderer/Rasterizer.hpp>
#include <Renderer/Shaders.hpp>
#include <Renderer/Types.hpp>
#include <Utils/Utils.hpp>

using namespace Pumpkin;

static bool ToScreen(const std::vector<Vec4>& clip, const FrameBuffer& fb, std::vector<ScreenVertex>& out) {
	out.clear();
	out.reserve(clip.size());
	for (const auto& c : clip) {
		if (c.w <= 0.0f) {
			return false; // behind the camera, reject until real clipping exists
		}
		const float invW = 1.0f / c.w;

		// NDC: x,y in [-1, 1], z in [0, 1]
		const float ndcX = c.x * invW;
		const float ndcY = c.y * invW;
		const float ndcZ = c.z * invW;

		// viewport transform
		out.push_back({
			(ndcX * 0.5f + 0.5f) * fb.width,
			(0.5f - ndcY * 0.5f) * fb.height, // Y flip: NDC is up, pixels are down
			ndcZ,
			invW
		});
	}
	return true;
}

// Pipeline: input -> clip -> screen -> raster
static void DrawTriangle(
	const FrameBuffer& fb,
	DepthBuffer& db)
{
	// Triangle defined in NDC coordinates
	static const std::vector<Vec4> vertices{
		{  0.0f,  1.0f, 0.0f, 1.0f },  // top
		{ -1.0f, -1.0f, 0.0f, 1.0f },  // bottom left
		{  1.0f, -1.0f, 0.0f, 1.0f }   // bottom right
	};
	// BGRA8
	static const std::vector<Color> colors{
		{ 0, 0, 255, 255 },  // top
		{ 0, 255, 0, 255 },  // bottom left
		{ 255, 0, 0, 255 }   // bottom right
	};
	const std::vector<Vec4> clip = VertexShader(vertices);

	std::vector<ScreenVertex> screen{};
	if (!ToScreen(clip, fb, screen)) {
		return;
	}

	PixelShader(fb, db, screen, colors);
}

static void SetMultiResWindowIcon(SDL_Window* window) {
	SDL_Surface* baseIcon = SDL_LoadPNG("Assets/Pumpkin@32.png");
	SDL_Surface* icon16 = SDL_LoadPNG("Assets/Pumpkin@16.png");
	SDL_Surface* icon64 = SDL_LoadPNG("Assets/Pumpkin@64.png");

	if (!baseIcon || !icon16 || !icon64) {
		SDL_Log("Couldn't load icons : %s", SDL_GetError());
		return;
	}

	SDL_AddSurfaceAlternateImage(baseIcon, icon16);
	SDL_DestroySurface(icon16);
	SDL_AddSurfaceAlternateImage(baseIcon, icon64);
	SDL_DestroySurface(icon64);

	if (!SDL_SetWindowIcon(window, baseIcon)) {
		SDL_Log("Error while setting the window icon : %s", SDL_GetError());
	}

	SDL_DestroySurface(baseIcon);
}

int main() {
	const auto fb = std::make_unique<FrameBuffer>();
	const auto db = std::make_unique<DepthBuffer>();

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::println("SDL could not initialize! SDL Error: {}", SDL_GetError());
		return 1;
	}

	{
		const SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
		fb->width = mode->w / 2, fb->height = mode->h / 2;
		db->width = mode->w / 2, db->height = mode->h / 2;
		db->pixels.resize(static_cast<size_t>(db->width) * db->height, 1.0f);
		SDL_free((void*)ids);
	}

	SDL_Window* window{ SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, 0) };
	SetMultiResWindowIcon(window);
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	surface->format = SDL_PIXELFORMAT_RGBA32;

	bool IsRunning = true;
	SDL_Event Event;
	uint64_t lastTime = SDL_GetTicks();
	uint32_t frames = 0;
	constexpr uint64_t TARGET_FPS = 120;
	constexpr auto FRAME_TIME = std::chrono::nanoseconds(1'000'000'000 / TARGET_FPS);
	auto next = std::chrono::steady_clock::now();
	while (IsRunning) {
		next += FRAME_TIME;
		uint64_t frameStart = SDL_GetTicks();
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_EVENT_QUIT) {
				IsRunning = false;
			}
		}

		fb->pixels = static_cast<uint32_t*>(surface->pixels);
		if constexpr (std::endian::native == std::endian::little) {
			std::fill_n(fb->pixels, fb->width * fb->height, std::byteswap(0x000000FFu));
		}
		else if constexpr (std::endian::native == std::endian::big) {
			std::fill_n(fb->pixels, fb->width * fb->height, 0x000000FFu);
		}
		std::ranges::fill(db->pixels, 1.0f);
		DrawTriangle(*fb, *db);
		SDL_UpdateWindowSurface(window);

		++frames;
		const uint64_t now = SDL_GetTicks();
		if (now - lastTime >= 1000) {
			SDL_SetWindowTitle(window, std::format("Pumpkin Engine | FPS: {}", frames).c_str());
			frames = 0;
			lastTime = now;
		}
		std::this_thread::sleep_until(next);
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
