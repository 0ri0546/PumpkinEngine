module;

#include <SDL_FixedIncludes.h>

module Pumpkin.Utils.Events;

import std;

namespace Pumpkin {

    // ---------------------------------------------------------------------------
    //  EventDispatcher
    // ---------------------------------------------------------------------------

    namespace {
        /// Keeps the dispatch depth correct even if a handler throws.
        struct DepthGuard {
            explicit DepthGuard(int& depth) : m_depth(depth) { ++m_depth; }
            ~DepthGuard() { --m_depth; }
            DepthGuard(const DepthGuard&) = delete;
            DepthGuard& operator=(const DepthGuard&) = delete;
        private:
            int& m_depth;
        };
    }

    EventDispatcher::HandlerId EventDispatcher::Add(std::uint32_t eventType, Handler handler) {
        Entry entry{ .id = m_nextId++, .fn = std::move(handler), .dead = false };
        const HandlerId id = entry.id;

        if (m_dispatchDepth > 0) {
            // The handler vectors are being iterated: add once the dispatch is over.
            m_pending.emplace_back(eventType, std::move(entry));
        } else {
            m_handlers[eventType].push_back(std::move(entry));
        }
        return id;
    }

    void EventDispatcher::Off(HandlerId id) {
        if (id == kInvalidHandler) {
            return;
        }

        std::erase_if(m_pending, [id](const auto& pending) { return pending.second.id == id; });

        for (auto& [type, entries] : m_handlers) {
            for (Entry& entry : entries) {
                if (entry.id == id && !entry.dead) {
                    entry.dead = true;          // erased later: a dispatch may be iterating this vector
                    m_needsCompaction = true;
                }
            }
        }

        if (m_dispatchDepth == 0) {
            Flush();
        }
    }

    void EventDispatcher::Flush() {
        if (m_needsCompaction) {
            for (auto it = m_handlers.begin(); it != m_handlers.end();) {
                std::erase_if(it->second, [](const Entry& entry) { return entry.dead; });
                it = it->second.empty() ? m_handlers.erase(it) : std::next(it);
            }
            m_needsCompaction = false;
        }

        for (auto& [type, entry] : m_pending) {
            m_handlers[type].push_back(std::move(entry));
        }
        m_pending.clear();
    }

    void EventDispatcher::Dispatch(const SDL_Event& event) {
        {
            const auto found = m_handlers.find(event.type);
            if (found != m_handlers.end()) {
                const DepthGuard guard(m_dispatchDepth);

                // Safe to iterate: during a dispatch handlers are only flagged dead or queued, never moved.
                for (Entry& entry : found->second) {
                    if (entry.dead) {
                        continue;
                    }
                    if (entry.fn(event)) {
                        break;   // consumed
                    }
                }
            }
        }

        if (m_dispatchDepth == 0) {
            Flush();
        }
    }

    void EventDispatcher::PumpEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            Dispatch(event);
        }
    }

    std::size_t EventDispatcher::HandlerCount() const {
        std::size_t count = m_pending.size();
        for (const auto& [type, entries] : m_handlers) {
            count += static_cast<std::size_t>(std::ranges::count_if(
                entries, [](const Entry& entry) { return !entry.dead; }));
        }
        return count;
    }

    // ---------------------------------------------------------------------------
    //  CameraController
    // ---------------------------------------------------------------------------

    CameraController::CameraController(Camera& camera, CameraBindings bindings)
        : m_camera(camera)
        , m_bindings(bindings) {}

    CameraController::~CameraController() {
        Detach();
    }

    void CameraController::Attach(EventDispatcher& dispatcher) {
        Detach();
        m_dispatcher = &dispatcher;

        m_handlerIds.push_back(dispatcher.On(SDL_EVENT_MOUSE_BUTTON_DOWN, [this](const SDL_Event& event) {
            if (event.button.button == m_bindings.rotateButton) {
                m_rotating = true;
            }
        }));

        m_handlerIds.push_back(dispatcher.On(SDL_EVENT_MOUSE_BUTTON_UP, [this](const SDL_Event& event) {
            if (event.button.button == m_bindings.rotateButton) {
                m_rotating = false;
            }
        }));

        m_handlerIds.push_back(dispatcher.On(SDL_EVENT_MOUSE_MOTION, [this](const SDL_Event& event) {
            if (m_rotating) {
                m_camera.Rotate(event.motion.xrel, event.motion.yrel);
            }
        }));

        // The button-up may never arrive if the window loses focus mid-drag.
        m_handlerIds.push_back(dispatcher.On(SDL_EVENT_WINDOW_FOCUS_LOST, [this](const SDL_Event&) {
            m_rotating = false;
        }));
    }

    void CameraController::Detach() {
        if (m_dispatcher != nullptr) {
            for (const EventDispatcher::HandlerId id : m_handlerIds) {
                m_dispatcher->Off(id);
            }
        }
        m_handlerIds.clear();
        m_dispatcher = nullptr;
        m_rotating = false;
    }

    void CameraController::Update(float deltaTime) {
        const bool* keys = SDL_GetKeyboardState(nullptr);

        float forward = (keys[m_bindings.forward]  ? 1.0f : 0.0f) - (keys[m_bindings.backward] ? 1.0f : 0.0f);
        float right   = (keys[m_bindings.right]    ? 1.0f : 0.0f) - (keys[m_bindings.left]     ? 1.0f : 0.0f);

        if (forward == 0.0f && right == 0.0f) {
            return;
        }

        if (forward != 0.0f && right != 0.0f) {
            constexpr float kInvSqrt2 = 0.70710678f;   // keep the diagonal speed equal to the axis speed
            forward *= kInvSqrt2;
            right   *= kInvSqrt2;
        }

        m_camera.Move(forward, right, deltaTime);
    }
}
