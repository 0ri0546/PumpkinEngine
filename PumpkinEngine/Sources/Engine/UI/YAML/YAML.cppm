module;

#include <DLL/Export.hpp>

export module Pumpkin.UI.YAML;

import std;

export namespace Pumpkin::UI::YAML
{
    enum class NodeType
    {
        Null,
        Scalar,
        Sequence,
        Mapping
    };

    struct PUMPKIN_API Node
    {
        NodeType type = NodeType::Null;

        // Valeur d'un noeud scalaire.
        std::string value;

        // Elements d'une sequence.
        std::vector<Node> sequence;

        // Paires cle/valeur d'un mapping.
        std::vector<std::pair<std::string, Node>> mapping;

        [[nodiscard]] const Node* Find(const std::string& key) const;
    };

    PUMPKIN_API [[nodiscard]] Node Parse(std::string_view source);

    // Fonction temporaire de test.
    PUMPKIN_API void TestParser();
}