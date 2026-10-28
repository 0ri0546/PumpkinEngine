
export module Pumpkin.UI.Loader;

import std;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

export namespace Pumpkin::UI
{
    struct UIFontDefinition;

    struct LoadedUI {
        Canvas canvas;
        UIStorage storage;
        std::vector<UIFontDefinition> fonts;
    };

    struct UIFontDefinition {
        FontId id = 0;
        std::string name;
        std::string path;
        float size = 24.0f;
    };

    [[nodiscard]]
    LoadedUI LoadUI(const YAML::Node& root);
    void TestUILoader();
}
