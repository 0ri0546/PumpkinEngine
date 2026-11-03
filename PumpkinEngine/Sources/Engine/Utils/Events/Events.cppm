/// @file Events.cppm
/// @brief Module Pumpkin.Utils.Events: SDL event dispatching and a free-fly camera controller.
///
/// The module only knows about SDL and the Camera: application-specific reactions (resize, UI, quit...)
/// are registered by the application through EventDispatcher::On().

module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>

export module Pumpkin.Utils.Events;

import Pumpkin.Renderer.Camera;
import std;

export namespace Pumpkin {

    /// @brief Routes SDL events to the handlers registered for their type.
    ///
    /// Handlers are called in registration order. A handler may return `bool`: returning true *consumes*
    /// the event, later handlers of the same type are skipped (e.g. a UI that swallows a click before the
    /// camera sees it). Handlers returning `void` never consume.
    ///
    /// Handlers may safely subscribe or unsubscribe from inside a handler: additions are applied after the
    /// current dispatch finishes, and removals take effect immediately for the *next* handler.
    ///
    /// @note Events are dispatched on the thread that calls PumpEvents() / Dispatch(), normally the main
    ///       thread. SDL_AddEventWatch callbacks can run on other threads, so it is deliberately not used here.
    /// @thread_safety Not thread-safe: use it from the thread that owns the SDL window.
    class PUMPKIN_API EventDispatcher {
    public:
        /// @brief Identifies one subscription (see On() / Off()).
        using HandlerId = std::uint32_t;

        /// @brief Handler signature stored internally; return true to consume the event.
        using Handler = std::function<bool(const SDL_Event&)>;

        /// @brief Value never returned by On(); safe to use as "no subscription".
        static constexpr HandlerId kInvalidHandler = 0;

        EventDispatcher() = default;
        EventDispatcher(const EventDispatcher&) = delete;
        EventDispatcher& operator=(const EventDispatcher&) = delete;

        /// @brief Subscribes @p handler to one SDL event type.
        ///
        /// @tparam F Callable invocable with `const SDL_Event&`, returning `void` (never consumes) or
        ///           something convertible to `bool` (true consumes). Must be copyable.
        /// @param eventType An SDL event type, e.g. `SDL_EVENT_QUIT`.
        /// @param handler   Callback. Whatever it captures by reference must outlive the subscription.
        /// @return Id to pass to Off().
        template <std::invocable<const SDL_Event&> F>
        HandlerId On(std::uint32_t eventType, F&& handler) {
            if constexpr (std::is_void_v<std::invoke_result_t<F&, const SDL_Event&>>) {
                return Add(eventType,
                    [fn = std::forward<F>(handler)](const SDL_Event& event) mutable {
                        fn(event);
                        return false;
                    });
            } else {
                return Add(eventType, Handler(std::forward<F>(handler)));
            }
        }

        /// @brief Removes a subscription.
        /// @param id Id returned by On(). Unknown or already removed ids are ignored.
        void Off(HandlerId id);

        /// @brief Drains the SDL event queue and dispatches every event.
        void PumpEvents();

        /// @brief Dispatches one event to the handlers of its type (also usable with synthetic events).
        /// @param event Event to deliver.
        void Dispatch(const SDL_Event& event);

        /// @brief Number of active subscriptions.
        [[nodiscard]] std::size_t HandlerCount() const;

    private:
        struct Entry {
            HandlerId id = kInvalidHandler;
            Handler   fn;
            bool      dead = false;
        };

        HandlerId Add(std::uint32_t eventType, Handler handler);
        void      Flush();

        std::unordered_map<std::uint32_t, std::vector<Entry>>           m_handlers;
        std::vector<std::pair<std::uint32_t, Entry>>                    m_pending;   // added during a dispatch
        HandlerId                                                       m_nextId = 1;
        int                                                             m_dispatchDepth = 0;
        bool                                                            m_needsCompaction = false;
    };

    /// @brief Keys and mouse button used by CameraController.
    ///
    /// Keys are SDL *scancodes* (physical positions), so the defaults are the WASD cluster whatever the layout.
    struct CameraBindings {
        SDL_Scancode forward      = SDL_SCANCODE_W;
        SDL_Scancode backward     = SDL_SCANCODE_S;
        SDL_Scancode left         = SDL_SCANCODE_A;
        SDL_Scancode right        = SDL_SCANCODE_D;
        std::uint8_t rotateButton = SDL_BUTTON_LEFT;   ///< Hold it and move the mouse to look around.
    };

    /// @brief Free-fly camera input: WASD to move (polled every frame), mouse drag to rotate (event driven).
    ///
    /// Call Attach() once to receive mouse events, and Update() once per frame for the keyboard. Diagonal
    /// movement is normalised, so it is not faster than moving along one axis.
    ///
    /// @warning The controller keeps a reference to the Camera and to the dispatcher it is attached to: both
    ///          must outlive it (declare the dispatcher before the controller). It is neither copyable nor movable.
    class PUMPKIN_API CameraController {
    public:
        /// @brief Creates a controller for @p camera.
        /// @param camera   Camera to move and rotate.
        /// @param bindings Keys / mouse button to use.
        explicit CameraController(Camera& camera, CameraBindings bindings = {});
        ~CameraController();

        CameraController(const CameraController&) = delete;
        CameraController& operator=(const CameraController&) = delete;

        /// @brief Subscribes to mouse button, mouse motion and focus-lost events.
        /// @param dispatcher Dispatcher to listen to. Replaces a previous Attach().
        void Attach(EventDispatcher& dispatcher);

        /// @brief Unsubscribes from the dispatcher (done automatically by the destructor).
        void Detach();

        /// @brief Applies the keyboard movement for this frame.
        /// @param deltaTime Frame time in seconds (forwarded to Camera::Move()).
        void Update(float deltaTime);

        /// @brief True while the rotate button is held down.
        [[nodiscard]] bool IsRotating() const { return m_rotating; }

    private:
        Camera&         m_camera;
        CameraBindings  m_bindings;
        EventDispatcher* m_dispatcher = nullptr;
        std::vector<EventDispatcher::HandlerId> m_handlerIds;
        bool            m_rotating = false;
    };
}
