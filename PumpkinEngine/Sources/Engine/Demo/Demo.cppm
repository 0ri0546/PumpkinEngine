/// @file Demo.cppm
/// @brief Umbrella module Pumpkin.Demo: re-exports every demo module.
///
/// @note Only Application::RunApplication is exported from the engine DLL (PUMPKIN_API): the executable
///       must not call the other functions directly.

export module Pumpkin.Demo;

export import Pumpkin.Demo.AppEvents;
export import Pumpkin.Demo.Application;
export import Pumpkin.Demo.Assets;
export import Pumpkin.Demo.Components;
export import Pumpkin.Demo.Config;
export import Pumpkin.Demo.Diagnostics;
export import Pumpkin.Demo.Helpers;
export import Pumpkin.Demo.ParticlePresets;
export import Pumpkin.Demo.SceneExtraction;
export import Pumpkin.Demo.Systems;
export import Pumpkin.Demo.WorldSetup;
