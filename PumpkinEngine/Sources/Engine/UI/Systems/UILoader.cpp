//
// Created by lmarcucci on 28/10/2026.
//

module Pumpkin.UI.Loader;

import std;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Components;
import Pumpkin.UI.Layout;

namespace
{
    using Node = Pumpkin::UI::YAML::Node;
    using NodeType = Pumpkin::UI::YAML::NodeType;

    const Node& Require(
        const Node& node,
        const std::string& key)
    {
        const Node* value = node.Find(key);

        if (value == nullptr)
        {
            throw std::runtime_error(
                "Champ YAML manquant : " + key);
        }

        return *value;
    }

    std::string ReadString(
        const Node& node,
        const std::string& key)
    {
        const Node& value = Require(node, key);

        if (value.type != NodeType::Scalar)
        {
            throw std::runtime_error(
                "Le champ '" + key + "' doit etre un scalaire.");
        }

        return value.value;
    }

    float ReadFloat(
        const Node& node,
        const std::string& key,
        float defaultValue)
    {
        const Node* value = node.Find(key);

        if (value == nullptr)
        {
            return defaultValue;
        }

        if (value->type != NodeType::Scalar)
        {
            throw std::runtime_error(
                "Le champ '" + key + "' doit etre numerique.");
        }

        std::size_t parsed = 0;
        const float result = std::stof(value->value, &parsed);

        if (parsed != value->value.size())
        {
            throw std::runtime_error(
                "Valeur numerique invalide : " + value->value);
        }

        return result;
    }

    Pumpkin::UI::Anchor ReadAnchor(const Node& node)
    {
        const Node* value = node.Find("anchor");

        if (value == nullptr)
        {
            return Pumpkin::UI::Anchor::TopLeft;
        }

        if (value->type != NodeType::Scalar)
        {
            throw std::runtime_error(
                "L'ancre doit etre un scalaire.");
        }

        using Pumpkin::UI::Anchor;

        if (value->value == "top-left")     return Anchor::TopLeft;
        if (value->value == "top-center")   return Anchor::TopCenter;
        if (value->value == "top-right")    return Anchor::TopRight;
        if (value->value == "center")       return Anchor::Center;
        if (value->value == "bottom-left")  return Anchor::BottomLeft;
        if (value->value == "bottom-center")return Anchor::BottomCenter;
        if (value->value == "bottom-right") return Anchor::BottomRight;
        if (value->value == "stretch")      return Anchor::Stretch;

        throw std::runtime_error(
            "Ancre inconnue : " + value->value);
    }

    Pumpkin::UI::Vec2 ReadPosition(const Node& node)
    {
        const Node* position = node.Find("position");

        if (position == nullptr)
        {
            return {};
        }

        return {
            ReadFloat(*position, "x", 0.0f),
            ReadFloat(*position, "y", 0.0f)
        };
    }

    Pumpkin::UI::Size2 ReadSize(const Node& node)
    {
        const Node* size = node.Find("size");

        if (size == nullptr)
        {
            return {};
        }

        return {
            ReadFloat(*size, "width", 0.0f),
            ReadFloat(*size, "height", 0.0f)
        };
    }

    std::array<std::uint8_t, 4> ReadColor(
    const Node& node,
    const std::string& key,
    std::array<std::uint8_t, 4> defaultColor)
    {
        const Node* color = node.Find(key);

        if (color == nullptr)
            return defaultColor;

        if (color->type != NodeType::Sequence ||
            color->sequence.size() != 4)
        {
            throw std::runtime_error(
                "La couleur '" + key +
                "' doit contenir quatre composantes RGBA.");
        }

        for (std::size_t i = 0; i < 4; ++i)
        {
            if (color->sequence[i].type != NodeType::Scalar)
                throw std::runtime_error(
                    "Composante invalide pour la couleur '" + key + "'.");

            const int component = std::stoi(color->sequence[i].value);

            if (component < 0 || component > 255)
                throw std::runtime_error(
                    "Les composantes RGBA doivent etre comprises entre 0 et 255.");

            defaultColor[i] = static_cast<std::uint8_t>(component);
        }

        return defaultColor;
    }

    // Conserve la lecture actuelle des images et des textes.
    std::array<std::uint8_t, 4> ReadColor(
        const Node& node,
        std::array<std::uint8_t, 4> defaultColor)
    {
        return ReadColor(node, "color", defaultColor);
    }

    template<typename T>
    Pumpkin::UI::ElementId NextId(const std::vector<T>& values)
    {
        if (values.size() >=
            static_cast<std::size_t>(
                std::numeric_limits<Pumpkin::UI::ElementId>::max()))
        {
            throw std::runtime_error(
                "Trop d'elements UI.");
        }

        return static_cast<Pumpkin::UI::ElementId>(
            values.size() + 1);
    }
}

namespace Pumpkin::UI
{
    LoadedUI LoadUI(const YAML::Node& root)
    {
        LoadedUI result;
        result.canvas = Canvas::FromYAML(root);

        std::unordered_map<std::string, TextureId> textures;

        std::unordered_map<std::string, UIFontDefinition> fonts;
        std::vector<UIFontDefinition> loadedFonts;

        TextureId nextTextureId = 1;
        FontId nextFontId = 1;

        if (const Node* textureList = root.Find("textures"))
        {
            if (textureList->type != NodeType::Sequence)
            {
                throw std::runtime_error(
                    "'textures' doit etre une sequence YAML.");
            }

            for (const Node& texture : textureList->sequence)
            {
                const std::string id = ReadString(texture, "id");

                if (!textures.emplace(id, nextTextureId).second)
                {
                    throw std::runtime_error(
                        "Texture declaree plusieurs fois : " + id);
                }

                ++nextTextureId;
            }
        }

        if (const Node* fontList = root.Find("fonts"))
        {
            if (fontList->type != NodeType::Sequence)
            {
                throw std::runtime_error(
                    "'fonts' doit etre une sequence YAML.");
            }

            for (const Node& font : fontList->sequence)
            {
                const std::string name = ReadString(font, "id");
                const std::string path = ReadString(font, "path");
                const float size = ReadFloat(font, "size", 24.0f);

                UIFontDefinition definition{
                    nextFontId,
                    name,
                    path,
                    size
                };

                if (!fonts.emplace(name, definition).second) {
                    throw std::runtime_error(
                        "Police declaree plusieurs fois : " + name
                    );
                }

                loadedFonts.push_back(definition);
                ++nextFontId;
            }
        }

        const Node& elements = Require(root, "elements");

        if (elements.type != NodeType::Sequence)
        {
            throw std::runtime_error(
                "'elements' doit etre une sequence YAML.");
        }

        std::unordered_set<ElementId> elementIds;

        for (const Node& element : elements.sequence)
        {
            const std::string type = ReadString(element, "type");
            const std::string name = ReadString(element, "id");

            const ElementId id =
                static_cast<ElementId>(elementIds.size() + 1);

            if (!elementIds.insert(id).second)
            {
                throw std::runtime_error(
                    "Identifiant UI duplique.");
            }

            Transform transform;
            transform.position = ReadPosition(element);
            transform.size = ReadSize(element);
            transform.anchor = ReadAnchor(element);

            if (type == "Image")
            {
                const std::string textureName =
                    ReadString(element, "texture");

                const auto texture = textures.find(textureName);

                if (texture == textures.end())
                {
                    throw std::runtime_error(
                        "Texture inconnue '" + textureName +
                        "' pour l'element '" + name + "'.");
                }

                Image image;
                image.id = id;
                image.texture = texture->second;
                image.color = ReadColor(
                    element, {255, 255, 255, 255});

                result.storage.transformEntities.push_back(id);
                result.storage.transforms.push_back(transform);
                result.storage.images.push_back(image);
            }
            else if (type == "Text")
            {
                const std::string fontName = ReadString(element, "font");
                const auto font = fonts.find(fontName);

                if (font == fonts.end())
                {
                    throw std::runtime_error(
                        "Police introuvable : " + fontName
                    );
                }

                Text text;
                text.id = id;
                text.font = font->second.id;
                text.fontSize = font->second.size;
                text.content = ReadString(element, "text");
                text.color = ReadColor(
                    element,
                    {255, 255, 255, 255}
                );

                result.storage.transformEntities.push_back(id);
                result.storage.transforms.push_back(transform);
                result.storage.texts.push_back(std::move(text));
            }
            else if (type == "Button")
            {
                Button button;
                button.id = id;
                button.text = ReadString(element, "text");

                button.normalColor = ReadColor(
                    element, "normalColor", {70, 70, 70, 255});

                button.hoveredColor = ReadColor(
                    element, "hoveredColor", {100, 100, 100, 255});

                button.pressedColor = ReadColor(
                    element, "pressedColor", {45, 45, 45, 255});

                button.disabledColor = ReadColor(
                    element, "disabledColor", {35, 35, 35, 180});

                result.storage.buttons.push_back(button);

                result.storage.transformEntities.push_back(id);
                result.storage.transforms.push_back(transform);
            }
            else
            {
                throw std::runtime_error(
                    "Type d'element UI non supporte : " + type);
            }
        }

        return LoadedUI{
            std::move(result.canvas),
            std::move(result.storage),
            std::move(loadedFonts)
        };
    }

    void TestUILoader()
    {
        std::ifstream file("Assets/UI/MainMenu.yaml");

        if (!file)
        {
            throw std::runtime_error(
                "impossible to open yaml file");
        }

        const std::string source{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        const auto root = YAML::Parse(source);
        const auto loaded = LoadUI(root);
        const auto rects = Pumpkin::UI::ComputeLayout(
            loaded.canvas,
            loaded.storage,
            1280,
            800
        );

        for (const auto& rect : rects)
        {
            std::println(
                "Element {}: x={}, y={}, w={}, h={}",
                rect.id,
                rect.x,
                rect.y,
                rect.width,
                rect.height
            );
        }

        std::println("Canvas: {}", loaded.canvas.name);
        std::println(
            "Transforms: {}",
            loaded.storage.transforms.size());
        std::println(
            "Images: {}",
            loaded.storage.images.size());
        std::println(
            "Texts: {}",
            loaded.storage.texts.size());
    }

}
