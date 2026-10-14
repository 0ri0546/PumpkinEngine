#include <algorithm>
#include <bit>
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
#include <Utils/ObjParser.hpp>

using namespace Pumpkin;

static bool ToScreen(const std::vector<Vec4f>& clip, const FrameBuffer& fb, std::vector<Vec4f>& out) {
	out.clear();
	out.reserve(clip.size());
	for (const auto& c : clip) {
		if (c[3] <= 0.0f) {
			return false; // behind the camera, reject until real clipping exists
		}
		const float invW = 1.0f / c[3];

		// NDC: x,y in [-1, 1], z in [0, 1]
		const float ndcX = c[0] * invW;
		const float ndcY = c[1] * invW;
		const float ndcZ = c[2] * invW;

		// viewport transform
		out.emplace_back(
			(ndcX * 0.5f + 0.5f) * fb.width,
			(0.5f - ndcY * 0.5f) * fb.height, // Y flip: NDC is up, pixels are down
			ndcZ,
			invW
		);
	}
	return true;
}

static std::vector<Vec4f> toVec4(const std::vector<float>& f, const std::vector<index_t>& indices, const float scale = 1.0f)
{
	std::vector<Vec4f> res;
	res.reserve(indices.size());

	for (const auto& idx : indices) {
		size_t base = static_cast<size_t>(idx.vertex_index) * 3;
		if (base + 2 >= f.size()) continue; // safety check
		res.emplace_back(f[base], f[base + 1] - scale / 2.0f, f[base + 2], scale);
	}

	return res;
}

static std::vector<Color> GenerateRNGRGB(size_t count) {
	std::vector<Color> colors;
	colors.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		colors.emplace_back(
			127,
			127,
			127,
			127
		);
	}
	return colors;
}

// Pipeline: input -> clip -> screen -> raster
static void DrawWhatever(
	const FrameBuffer& fb,
	DepthBuffer& db)
{
	static const auto [attribs, indices] = LoadObj("Assets/pioche.obj");
	static std::vector<Vec4f> vertices = toVec4(attribs.vertices, indices, 2.0f);

	// BGRA8
	static const std::vector<Color> colors = GenerateRNGRGB(vertices.size());

	static const std::vector<Vec4f> originalVertices = vertices;
    static constexpr float speed = 1.0f;

    const auto t = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // Toggle
    bool rotateX = false;
    bool rotateY = true;
    bool rotateZ = false;

    // Rotation center. Your cube spans z 0..1, so its center is z = 0.5.
    constexpr float pivotX = 0.0f, pivotY = -1.0f, pivotZ = 0.5f;
    constexpr float zScale = 0.5f / 0.8660254f;

    const float angle = t * speed;

    // Angle is 0 for a disabled axis, which makes that rotation a no-op.
    const float angleX = rotateX ? angle : 0.0f;
    const float angleY = rotateY ? angle : 0.0f;
    const float angleZ = rotateZ ? angle : 0.0f;

    const float cx = std::cos(angleX), sx = std::sin(angleX);
    const float cy = std::cos(angleY), sy = std::sin(angleY);
    const float cz = std::cos(angleZ), sz = std::sin(angleZ);

    for (size_t i = 0; i < vertices.size(); ++i) {
        // Move so the pivot is at the origin
        const float x = originalVertices[i][0] - pivotX;
        const float y = originalVertices[i][1] - pivotY;
        const float z = originalVertices[i][2] - pivotZ;

        // Rotate around X
        const float x1 = x;
        const float y1 = y * cx - z * sx;
        const float z1 = y * sx + z * cx;

        // Rotate around Y
        const float x2 = x1 * cy + z1 * sy;
        const float y2 = y1;
        const float z2 = -x1 * sy + z1 * cy;

        // Rotate around Z
        const float x3 = x2 * cz - y2 * sz;
        const float y3 = x2 * sz + y2 * cz;
        const float z3 = z2;

        // Move back
        vertices[i][0] = x3 + pivotX;
        vertices[i][1] = y3 + pivotY;
        vertices[i][2] = z3 * zScale + pivotZ;
		vertices[i][3] = originalVertices[i][3]; // keep w / k untouched
    }

	const std::vector<Vec4f> clip = VertexShader(vertices);

	std::vector<Vec4f> screen{};
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
    uint64_t TARGET_FPS = 120;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::println("SDL could not initialize! SDL Error: {}", SDL_GetError());
		return 1;
	}

	{
		const SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
		fb->width = mode->h - 200, fb->height = mode->h - 200;
		db->width = mode->h - 200, db->height = mode->h - 200;
		db->pixels.resize(static_cast<size_t>(db->width) * db->height, 1.0f);
		if (mode->refresh_rate != 0.0f) TARGET_FPS = 9999999;//mode->refresh_rate;
		SDL_free((void*)ids);
	}

	static const auto flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_TRANSPARENT;
	SDL_Window* window{ SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, flags) };
	SetMultiResWindowIcon(window);
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	surface->format = SDL_PIXELFORMAT_RGBA32;

	bool IsRunning = true;
	SDL_Event Event;
	uint64_t lastTime = SDL_GetTicks();
	uint32_t frames = 0;
	const auto FRAME_TIME = std::chrono::nanoseconds(1'000'000'000 / TARGET_FPS);
	auto next = std::chrono::steady_clock::now();
	while (IsRunning) {
		next += FRAME_TIME;
		uint64_t frameStart = SDL_GetTicks();
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_EVENT_QUIT) {
				IsRunning = false;
			}
			if (Event.type == SDL_EVENT_WINDOW_RESIZED) {
				SDL_UpdateWindowSurface(window);
				surface = SDL_GetWindowSurface(window);
				fb->width = Event.window.data1;
				fb->height = Event.window.data2;
				db->width = Event.window.data1;
				db->height = Event.window.data2;
				db->pixels.resize(static_cast<size_t>(db->width * db->height), 1.0f);
				fb->pixels = static_cast<uint32_t*>(surface->pixels);
			}
		}

		fb->pixels = static_cast<uint32_t*>(surface->pixels);
		std::ranges::fill(db->pixels, 1.0f);
		DrawWhatever(*fb, *db);
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
