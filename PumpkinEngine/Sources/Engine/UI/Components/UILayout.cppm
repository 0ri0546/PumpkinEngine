
export module Pumpkin.UI.Layout;

import std;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

export namespace Pumpkin::UI
{
    struct UIRect
    {
        ElementId id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    [[nodiscard]]
    std::vector<UIRect> ComputeLayout(
        const Canvas& canvas,
        const UIStorage& storage,
        int windowWidth,
        int windowHeight
    );
}
