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

static std::vector<Vec4f> toVec4(const std::vector<float>& f) {
    std::vector<Vec4f> res{};
    for (size_t i = 3; i < f.size() + 1; i += 3) {
        res.emplace_back(f[i - 3], f[i - 2], f[i - 1], 1.0f);
    }
    return res;
}

// Pipeline: input -> clip -> screen -> raster
static void DrawTriangle(
	const FrameBuffer& fb,
	DepthBuffer& db)
{
	// Triangle defined in NDC coordinates
	static std::vector<Vec4f> vertices = toVec4(LoadObj("Assets/cube.obj").first.vertices);
	static const std::vector<Vec4f> originalVertices = vertices;
    static constexpr float speed = 1.0f;

    const auto t = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // Toggles (make these members or UI checkboxes, whatever fits your app)
    bool rotateX = true;
    bool rotateY = true;
    bool rotateZ = false;

    // Rotation center. Your cube spans z 0..1, so its center is z = 0.5.
    constexpr float pivotX = 0.0f, pivotY = 0.0f, pivotZ = 0.5f;
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

	// BGRA8
	static const std::vector<Color> colors{
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
        Color{ 255, 0, 0, 127 }, Color{ 0, 255, 0, 127 }, Color{ 0, 0, 255, 127 },
    };
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
		fb->width = mode->h / 2, fb->height = mode->h / 2;
		db->width = mode->h / 2, db->height = mode->h / 2;
		db->pixels.resize(static_cast<size_t>(db->width) * db->height, 1.0f);
	    if (mode->refresh_rate != 0.0f) TARGET_FPS = mode->refresh_rate;
		SDL_free((void*)ids);
	}

	SDL_Window* window{ SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, SDL_WINDOW_TRANSPARENT) };
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
		}

		fb->pixels = static_cast<uint32_t*>(surface->pixels);
		if constexpr (std::endian::native == std::endian::little) {
			std::fill_n(fb->pixels, fb->width * fb->height, std::byteswap(0x000000FFu));
		}
		else {
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
