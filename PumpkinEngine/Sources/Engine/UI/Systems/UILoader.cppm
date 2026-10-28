
export module Pumpkin.UI.Loader;

import std;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;

export namespace Pumpkin::UI
{
    struct LoadedUI
    {
        Canvas canvas;
        UIStorage storage;
    };

    [[nodiscard]]
    LoadedUI LoadUI(const YAML::Node& root);
    void TestUILoader();
}
