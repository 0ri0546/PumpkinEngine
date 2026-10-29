module;

#include <DLL/Export.hpp>

export module Pumpkin.UI.Loader;

import std;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

export namespace Pumpkin::UI
{
    struct PUMPKIN_API UIFontDefinition {
        FontId id = 0;
        std::string name;
        std::string path;
        float size = 24.0f;
    };

    struct PUMPKIN_API LoadedUI {
        Canvas canvas;
        UIStorage storage;
        std::vector<UIFontDefinition> fonts;
    };

    [[nodiscard]]
    PUMPKIN_API LoadedUI LoadUI(const YAML::Node& root);
    PUMPKIN_API void TestUILoader();
}
