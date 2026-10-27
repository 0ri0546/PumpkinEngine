export module Pumpkin.UI.Canvas;

import std;
import Pumpkin.UI.YAML;

export namespace Pumpkin::UI
{
    enum class CanvasScaleMode
    {
        Fit,
        Stretch,
        ConstantPixelSize
    };

    struct ScaleResult
    {
        float x = 1.0f;
        float y = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
    };

    struct UIPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    class Canvas
    {
    public:
        std::string name;

        int referenceWidth = 1920;
        int referenceHeight = 1080;

        CanvasScaleMode scaleMode = CanvasScaleMode::Fit;

        [[nodiscard]]
        ScaleResult CalculateScale(
            int windowWidth,
            int windowHeight
        ) const;

        [[nodiscard]]
        static Canvas FromYAML(const YAML::Node& root);

        [[nodiscard]]
        UIPoint TransformPosition(
            float x,
            float y,
            int windowWidth,
            int windowHeight
        ) const;
    };
}