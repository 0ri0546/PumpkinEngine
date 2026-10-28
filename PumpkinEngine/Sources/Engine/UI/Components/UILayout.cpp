
module Pumpkin.UI.Layout;

import std;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

namespace Pumpkin::UI
{
    namespace
    {
        struct AnchorFactors
        {
            float x;
            float y;
        };

        AnchorFactors GetAnchorFactors(Anchor anchor)
        {
            switch (anchor)
            {
            case Anchor::TopLeft:
                return {0.0f, 0.0f};

            case Anchor::TopCenter:
                return {0.5f, 0.0f};

            case Anchor::TopRight:
                return {1.0f, 0.0f};

            case Anchor::Center:
                return {0.5f, 0.5f};

            case Anchor::BottomLeft:
                return {0.0f, 1.0f};

            case Anchor::BottomCenter:
                return {0.5f, 1.0f};

            case Anchor::BottomRight:
                return {1.0f, 1.0f};

            case Anchor::Stretch:
                return {0.0f, 0.0f};
            }

            throw std::runtime_error("Ancre UI inconnue.");
        }
    }

    std::vector<UIRect> ComputeLayout(
        const Canvas& canvas,
        const UIStorage& storage,
        int windowWidth,
        int windowHeight)
    {
        if (windowWidth <= 0 || windowHeight <= 0)
        {
            throw std::invalid_argument(
                "La taille de la fenetre doit etre positive.");
        }

        if (storage.transformEntities.size() !=
            storage.transforms.size())
        {
            throw std::runtime_error(
                "Les identifiants et les transformations UI ne correspondent pas.");
        }

        const ScaleResult scale =
            canvas.CalculateScale(windowWidth, windowHeight);

        std::vector<UIRect> result;
        result.reserve(storage.transforms.size());

        for (std::size_t i = 0;
             i < storage.transforms.size();
             ++i)
        {
            const ElementId id = storage.transformEntities[i];
            const Transform& transform = storage.transforms[i];

            const AnchorFactors anchor =
                GetAnchorFactors(transform.anchor);

            float logicalX =
                static_cast<float>(canvas.referenceWidth) * anchor.x
                + transform.position.x
                - transform.size.width * anchor.x;

            float logicalY =
                static_cast<float>(canvas.referenceHeight) * anchor.y
                + transform.position.y
                - transform.size.height * anchor.y;

            float logicalWidth = transform.size.width;
            float logicalHeight = transform.size.height;

            if (transform.anchor == Anchor::Stretch)
            {
                logicalX = 0.0f;
                logicalY = 0.0f;
                logicalWidth =
                    static_cast<float>(canvas.referenceWidth);
                logicalHeight =
                    static_cast<float>(canvas.referenceHeight);
            }

            result.push_back({
                id,
                scale.offsetX + logicalX * scale.x,
                scale.offsetY + logicalY * scale.y,
                logicalWidth * scale.x,
                logicalHeight * scale.y
            });
        }

        return result;
    }
}
