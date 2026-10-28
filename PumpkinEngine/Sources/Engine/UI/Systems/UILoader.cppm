module;

#include <DLL/Export.hpp>

export module Pumpkin.UI.Loader;

import std;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

export namespace Pumpkin::UI
{
    struct UIFontDefinition;

    struct PUMPKIN_API LoadedUI {
        Canvas canvas;
        UIStorage storage;
        std::vector<UIFontDefinition> fonts;
    };

    struct PUMPKIN_API UIFontDefinition {
        FontId id = 0;
        std::string name;
        std::string path;
        float size = 24.0f;
    };

    [[nodiscard]]
    PUMPKIN_API LoadedUI LoadUI(const YAML::Node& root);
    PUMPKIN_API void TestUILoader();
}
