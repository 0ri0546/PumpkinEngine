module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>
#include <SDL3/SDL_ttf.h>

export module Pumpkin.UI.Systems.Render;

import std;
import Pumpkin.UI.Loader;
import Pumpkin.UI.Layout;
import Pumpkin.UI.YAML;
import Pumpkin.Renderer.Types;
import Pumpkin.UI.Components;
import Pumpkin.Renderer.Texture;
import Pumpkin.Renderer.Passes.Image;

export namespace Pumpkin::UI
{
    class PUMPKIN_API UIRenderSystem
    {
    public:
        explicit UIRenderSystem(LoadedUI& ui)
            : m_ui(ui)
        {
            LoadFonts();
            LoadTextures();
            BuildImages();
            MeasureTexts();
            BuildTexts();
        }

        UIRenderSystem(const UIRenderSystem&) = delete;
        UIRenderSystem& operator=(const UIRenderSystem&) = delete;

        ~UIRenderSystem()
        {
            for (auto& text : m_texts)
            {
                if (text.surface)
                    SDL_DestroySurface(text.surface);

                text.surface = nullptr;
            }

            for (auto& [id, font] : m_fonts)
            {
                if (font)
                    TTF_CloseFont(font);
            }
        }

        void UpdateLayout(int windowWidth, int windowHeight)
        {
            const auto rects = ComputeLayout(
                m_ui.canvas,
                m_ui.storage,
                windowWidth,
                windowHeight
            );

            for (const auto& rect : rects)
            {
                const auto it = m_textIndices.find(rect.id);

                if (it == m_textIndices.end())
                    continue;

                auto& text = m_texts[it->second];

                text.SetPosition(
                    static_cast<int>(std::lround(rect.x)),
                    static_cast<int>(std::lround(rect.y))
                );
            }

            for (const auto& rect : rects)
            {
                const auto it = m_imageIndices.find(rect.id);

                if (it == m_imageIndices.end())
                    continue;

                auto& image = m_images[it->second];

                image.x = static_cast<int>(std::lround(rect.x));
                image.y = static_cast<int>(std::lround(rect.y));
                image.width = static_cast<int>(std::lround(rect.width));
                image.height = static_cast<int>(std::lround(rect.height));
            }
        }

        [[nodiscard]]
        std::span<Pumpkin::Text> Texts()
        {
            return m_texts;
        }

        [[nodiscard]]
        std::span<const Pumpkin::UIImageDraw> Images() const
        {
            return m_images;
        }

    private:
        void LoadFonts()
        {
            for (const auto& definition : m_ui.fonts)
            {
                TTF_Font* font = TTF_OpenFont(
                    definition.path.c_str(),
                    definition.size
                );

                if (!font)
                {
                    throw std::runtime_error(
                        "Impossible de charger la police '" +
                        definition.name + "' : " + SDL_GetError()
                    );
                }

                if (!m_fonts.emplace(definition.id, font).second)
                {
                    TTF_CloseFont(font);
                    throw std::runtime_error(
                        "Identifiant de police UI duplique."
                    );
                }
            }
        }

        void LoadTextures()
        {
            const auto root = Pumpkin::UI::YAML::Parse(
                [&]()
                {
                    std::ifstream file("Assets/UI/MainMenu.yaml");

                    if (!file)
                        throw std::runtime_error("Impossible d'ouvrir MainMenu.yaml");

                    return std::string{
                        std::istreambuf_iterator<char>(file),
                        std::istreambuf_iterator<char>()
                    };
                }()
            );

            const auto* textureList = root.Find("textures");

            if (!textureList)
                return;

            Pumpkin::UI::TextureId id = 1;

            for (const auto& definition : textureList->sequence)
            {
                const auto* nameNode = definition.Find("id");
                const auto* pathNode = definition.Find("path");

                if (!nameNode || !pathNode)
                    throw std::runtime_error(
                        "Chaque texture UI doit posseder un id et un path."
                    );

                Pumpkin::Texture texture;

                if (!Pumpkin::LoadTexture(texture, pathNode->value))
                    throw std::runtime_error(
                        "Impossible de charger la texture UI : " + pathNode->value
                    );

                m_textures.emplace(id++, std::move(texture));
            }
        }

        void BuildImages()
        {
            m_images.reserve(m_ui.storage.images.size());

            for (const auto& image : m_ui.storage.images)
            {
                const auto texture = m_textures.find(image.texture);

                if (texture == m_textures.end())
                    throw std::runtime_error("Texture manquante pour une image UI.");

                m_imageIndices.emplace(image.id, m_images.size());

                m_images.push_back(Pumpkin::UIImageDraw{
                    .texture = &texture->second,
                    .color = image.color
                });
            }
        }

        void MeasureTexts()
        {
            for (const auto& uiText : m_ui.storage.texts)
            {
                const auto fontIt = m_fonts.find(uiText.font);

                if (fontIt == m_fonts.end())
                {
                    throw std::runtime_error(
                        "Police absente pour le texte UI."
                    );
                }

                int width = 0;
                int height = 0;

                if (!TTF_GetStringSize(
                    fontIt->second,
                    uiText.content.c_str(),
                    uiText.content.size(),
                    &width,
                    &height))
                {
                    throw std::runtime_error(
                        "Impossible de mesurer un texte UI."
                    );
                }

                for (std::size_t i = 0;
                     i < m_ui.storage.transformEntities.size();
                     ++i)
                {
                    if (m_ui.storage.transformEntities[i] == uiText.id)
                    {
                        auto& size = m_ui.storage.transforms[i].size;

                        if (size.width <= 0.0f)
                            size.width = static_cast<float>(width);

                        if (size.height <= 0.0f)
                            size.height = static_cast<float>(height);

                        break;
                    }
                }
            }
        }

        void BuildTexts()
        {
            const auto& uiTexts = m_ui.storage.texts;

            // Stabilise les chaînes avant de créer les objets Text,
            // car Pumpkin::Text conserve un const char*.
            m_contents.reserve(uiTexts.size());

            for (const auto& uiText : uiTexts)
                m_contents.push_back(uiText.content);

            m_texts.reserve(uiTexts.size());

            for (std::size_t i = 0; i < uiTexts.size(); ++i)
            {
                const auto& uiText = uiTexts[i];
                const auto fontIt = m_fonts.find(uiText.font);

                const SDL_Color color{
                    uiText.color[0],
                    uiText.color[1],
                    uiText.color[2],
                    uiText.color[3]
                };

                m_texts.emplace_back(
                    fontIt->second,
                    m_contents[i].c_str(),
                    uiText.fontSize,
                    color,
                    0,
                    0
                );

                m_textIndices.emplace(
                    uiText.id,
                    i
                );

                if (!m_texts.back().surface)
                {
                    throw std::runtime_error(
                        "Impossible de generer la surface d'un texte UI."
                    );
                }
            }
        }

        LoadedUI& m_ui;

        std::unordered_map<Pumpkin::UI::FontId, TTF_Font*> m_fonts;
        std::vector<std::string> m_contents;
        std::vector<Pumpkin::Text> m_texts;
        std::unordered_map<Pumpkin::UI::ElementId, std::size_t> m_textIndices;
        std::unordered_map<Pumpkin::UI::TextureId, Pumpkin::Texture> m_textures;
        std::vector<Pumpkin::UIImageDraw> m_images;
        std::unordered_map<Pumpkin::UI::ElementId, std::size_t> m_imageIndices;
    };
}
