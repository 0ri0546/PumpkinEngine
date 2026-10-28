module Pumpkin.UI.Canvas;

import std;
import Pumpkin.UI.YAML;

namespace Pumpkin::UI
{

    ScaleResult Canvas::CalculateScale(
        int windowWidth,
        int windowHeight
    ) const
    {
        if (windowWidth <= 0 || windowHeight <= 0)
        {
            throw std::invalid_argument(
                "La taille de la fenetre doit etre positive."
            );
        }

        if (referenceWidth <= 0 || referenceHeight <= 0)
        {
            throw std::invalid_argument(
                "La resolution de reference doit etre positive."
            );
        }

        const float scaleX =
            static_cast<float>(windowWidth) / referenceWidth;

        const float scaleY =
            static_cast<float>(windowHeight) / referenceHeight;

        ScaleResult result;

        switch (scaleMode)
        {
        case CanvasScaleMode::Fit:
        {
            const float scale = std::min(scaleX, scaleY);

            result.x = scale;
            result.y = scale;

            result.offsetX =
                (windowWidth - referenceWidth * scale) * 0.5f;

            result.offsetY =
                (windowHeight - referenceHeight * scale) * 0.5f;

            break;
        }

        case CanvasScaleMode::Stretch:
        {
            result.x = scaleX;
            result.y = scaleY;
            break;
        }

        case CanvasScaleMode::ConstantPixelSize:
        {
            result.x = 1.0f;
            result.y = 1.0f;

            result.offsetX =
                (windowWidth - referenceWidth) * 0.5f;

            result.offsetY =
                (windowHeight - referenceHeight) * 0.5f;

            break;
        }
        }

        return result;
    }

    Canvas Canvas::FromYAML(const YAML::Node& root)
    {
        Canvas canvas;

        const YAML::Node* canvasNode = root.Find("canvas");

        if (!canvasNode)
            throw std::runtime_error("Section 'canvas' absente du YAML.");

        const YAML::Node* nameNode = canvasNode->Find("name");

        if (nameNode)
            canvas.name = nameNode->value;

        const YAML::Node* resolutionNode =
            canvasNode->Find("referenceResolution");

        if (resolutionNode)
        {
            const YAML::Node* widthNode =
                resolutionNode->Find("width");

            const YAML::Node* heightNode =
                resolutionNode->Find("height");

            if (widthNode)
                canvas.referenceWidth = std::stoi(widthNode->value);

            if (heightNode)
                canvas.referenceHeight = std::stoi(heightNode->value);
        }

        const YAML::Node* scaleModeNode =
            canvasNode->Find("scaleMode");

        if (scaleModeNode)
        {
            if (scaleModeNode->value == "fit")
                canvas.scaleMode = CanvasScaleMode::Fit;
            else if (scaleModeNode->value == "stretch")
                canvas.scaleMode = CanvasScaleMode::Stretch;
            else if (scaleModeNode->value == "constantPixelSize")
                canvas.scaleMode = CanvasScaleMode::ConstantPixelSize;
            else
                throw std::runtime_error(
                    "Mode de mise a l'echelle du Canvas inconnu : " +
                    scaleModeNode->value
                );
        }

        if (canvas.referenceWidth <= 0 ||
            canvas.referenceHeight <= 0)
        {
            throw std::runtime_error(
                "La resolution de reference du Canvas doit etre positive."
            );
        }

        return canvas;
    }

    UIPoint Canvas::TransformPosition(
        float x,
        float y,
        int windowWidth,
        int windowHeight
    ) const
    {
        const ScaleResult scale =
            CalculateScale(windowWidth, windowHeight);

        return {
            .x = x * scale.x + scale.offsetX,
            .y = y * scale.y + scale.offsetY
        };
    }
}