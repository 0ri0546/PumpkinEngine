
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

        const float scaleX =
            static_cast<float>(windowWidth) /
            static_cast<float>(canvas.referenceWidth);

        const float scaleY =
            static_cast<float>(windowHeight) /
            static_cast<float>(canvas.referenceHeight);

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

            float width = transform.size.width * scaleX;
            float height = transform.size.height * scaleY;

            const float positionX =
                transform.position.x / 100.0f *
                static_cast<float>(windowWidth);

            const float positionY =
                transform.position.y / 100.0f *
                static_cast<float>(windowHeight);

            float x =
                static_cast<float>(windowWidth) * anchor.x
                + positionX
                - width * anchor.x;

            float y =
                static_cast<float>(windowHeight) * anchor.y
                + positionY
                - height * anchor.y;

            if (transform.anchor == Anchor::Stretch)
            {
                x = 0.0f;
                y = 0.0f;
                width = static_cast<float>(windowWidth);
                height = static_cast<float>(windowHeight);
            }

            result.push_back({
                id,
                x,
                y,
                width,
                height
            });
        }

        return result;
    }
}
