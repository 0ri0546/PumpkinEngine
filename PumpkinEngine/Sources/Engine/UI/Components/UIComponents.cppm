
export module Pumpkin.UI.Components;

import std;

export namespace Pumpkin::UI
{
    using ElementId = std::uint32_t;
    using TextureId = std::uint32_t;
    using FontId = std::uint32_t;

    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Size2
    {
        float width = 0.0f;
        float height = 0.0f;
    };

    enum class Anchor : std::uint8_t
    {
        TopLeft,
        TopCenter,
        TopRight,
        Center,
        BottomLeft,
        BottomCenter,
        BottomRight,
        Stretch
    };

    struct Transform
    {
        Vec2 position;
        Size2 size;
        Anchor anchor = Anchor::TopLeft;
    };

    struct Image
    {
        ElementId id = 0;
        TextureId texture = 0;
        std::array<std::uint8_t, 4> color{
            255, 255, 255, 255
        };
    };

    struct Text
    {
        ElementId id = 0;
        FontId font = 0;
        std::string content;
        float fontSize = 24.0f;
        std::array<std::uint8_t, 4> color{255, 255, 255, 255};
    };


    enum class ButtonState : std::uint8_t
    {
        Normal,
        Hovered,
        Pressed,
        Disabled
    };

    struct Button
    {
        ElementId id = 0;

        std::string text;

        std::array<std::uint8_t, 4> normalColor{70, 70, 70, 255};
        std::array<std::uint8_t, 4> hoveredColor{100, 100, 100, 255};
        std::array<std::uint8_t, 4> pressedColor{45, 45, 45, 255};
        std::array<std::uint8_t, 4> disabledColor{35, 35, 35, 180};

        ButtonState state = ButtonState::Normal;
        bool enabled = true;
    };

    struct UIButtonDraw
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        float scale = 1.0f;

        std::array<std::uint8_t, 4> color{
            70, 70, 70, 255
        };
    };

    struct UIStorage
    {
        std::vector<ElementId> transformEntities;
        std::vector<Transform> transforms;

        std::vector<Image> images;
        std::vector<Text> texts;
        std::vector<Button> buttons;
    };
}
