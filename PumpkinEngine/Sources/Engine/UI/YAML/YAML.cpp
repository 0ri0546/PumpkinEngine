
module Pumpkin.UI.YAML;

import Pumpkin.UI.Canvas;

import std;

namespace Pumpkin::UI::YAML
{

    const Node* Node::Find(const std::string& key) const
    {
        if (type != NodeType::Mapping)
            return nullptr;

        for (const auto& [entryKey, entryValue] : mapping)
        {
            if (entryKey == key)
                return &entryValue;
        }

        return nullptr;
    }
    namespace
    {
        struct Line
        {
            std::size_t indent;
            std::string text;
            std::size_t number;
        };

        std::string Trim(std::string_view text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");

            if (first == std::string_view::npos)
                return {};

            const auto last = text.find_last_not_of(" \t\r\n");

            return std::string(text.substr(first, last - first + 1));
        }

        std::string RemoveComment(std::string_view text)
        {
            char quote = '\0';
            int depth = 0;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (quote != '\0')
                {
                    if (c == quote && (i == 0 || text[i - 1] != '\\'))
                        quote = '\0';

                    continue;
                }

                if (c == '\'' || c == '"')
                {
                    quote = c;
                    continue;
                }

                if (c == '[' || c == '{')
                    ++depth;
                else if (c == ']' || c == '}')
                    --depth;
                else if (c == '#' && depth >= 0 &&
                    (i == 0 || text[i - 1] == ' ' || text[i - 1] == '\t'))
                    return Trim(text.substr(0, i));
            }

            return Trim(text);
        }

        std::size_t FindSeparator(std::string_view text, char separator)
        {
            char quote = '\0';
            int depth = 0;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (quote != '\0')
                {
                    if (c == quote && (i == 0 || text[i - 1] != '\\'))
                        quote = '\0';

                    continue;
                }

                if (c == '\'' || c == '"')
                    quote = c;
                else if (c == '[' || c == '{')
                    ++depth;
                else if (c == ']' || c == '}')
                    --depth;
                else if (c == separator && depth == 0)
                    return i;
            }

            return std::string_view::npos;
        }

        std::vector<std::string> SplitInline(std::string_view text)
        {
            std::vector<std::string> parts;
            std::size_t start = 0;

            while (start < text.size())
            {
                const auto relative = FindSeparator(text.substr(start), ',');
                if (relative == std::string_view::npos)
                {
                    parts.push_back(Trim(text.substr(start)));
                    break;
                }

                parts.push_back(Trim(text.substr(start, relative)));
                start += relative + 1;
            }

            return parts;
        }

        Node ParseInline(std::string_view text);

        void AddMappingEntry(Node& node, std::string_view text)
        {
            const auto separator = FindSeparator(text, ':');

            if (separator == std::string_view::npos)
                throw std::runtime_error("Expected key: value");

            const std::string key = Trim(text.substr(0, separator));

            if (key.empty())
                throw std::runtime_error("Empty mapping key");

            node.mapping.emplace_back(
                key,
                ParseInline(Trim(text.substr(separator + 1)))
            );
        }

        Node ParseInline(std::string_view source)
        {
            const std::string text = Trim(source);
            Node node;

            if (text.empty() || text == "null" || text == "~")
                return node;

            if (text.front() == '"' || text.front() == '\'')
            {
                if (text.size() < 2 || text.back() != text.front())
                    throw std::runtime_error("Unterminated quoted string");

                node.type = NodeType::Scalar;
                node.value = text.substr(1, text.size() - 2);
                return node;
            }

            if (text.front() == '[')
            {
                if (text.back() != ']')
                    throw std::runtime_error("Unclosed inline sequence");

                node.type = NodeType::Sequence;
                const auto inside = std::string_view(text).substr(1, text.size() - 2);

                if (!Trim(inside).empty())
                {
                    for (const auto& part : SplitInline(inside))
                        node.sequence.push_back(ParseInline(part));
                }

                return node;
            }

            if (text.front() == '{')
            {
                if (text.back() != '}')
                    throw std::runtime_error("Unclosed inline mapping");

                node.type = NodeType::Mapping;
                const auto inside = std::string_view(text).substr(1, text.size() - 2);

                if (!Trim(inside).empty())
                {
                    for (const auto& part : SplitInline(inside))
                        AddMappingEntry(node, part);
                }

                return node;
            }

            node.type = NodeType::Scalar;
            node.value = text;
            return node;
        }

        class Parser
        {
        public:
            explicit Parser(std::string_view source)
            {
                std::size_t start = 0;
                std::size_t lineNumber = 1;

                while (start <= source.size())
                {
                    const auto end = source.find('\n', start);
                    const auto length = end == std::string_view::npos
                        ? source.size() - start
                        : end - start;

                    const auto raw = source.substr(start, length);
                    std::size_t indent = 0;

                    while (indent < raw.size() && raw[indent] == ' ')
                        ++indent;

                    if (indent < raw.size() && raw[indent] == '\t')
                        throw std::runtime_error(
                            "Tabs are not supported for indentation at line " +
                            std::to_string(lineNumber)
                        );

                    const std::string content =
                        RemoveComment(raw.substr(indent));

                    if (!content.empty())
                        lines_.push_back({ indent, content, lineNumber });

                    if (end == std::string_view::npos)
                        break;

                    start = end + 1;
                    ++lineNumber;
                }
            }

            Node Run()
            {
                if (lines_.empty())
                    return {};

                Node root = ParseBlock(lines_[0].indent);

                if (index_ != lines_.size())
                    Error("Unexpected indentation");

                return root;
            }

        private:
            std::vector<Line> lines_;
            std::size_t index_ = 0;

            [[noreturn]] void Error(const std::string& message) const
            {
                const auto line = index_ < lines_.size()
                    ? lines_[index_].number
                    : (lines_.empty() ? 1 : lines_.back().number);

                throw std::runtime_error(
                    "YAML line " + std::to_string(line) + ": " + message
                );
            }

            bool IsSequenceLine(const Line& line) const
            {
                return line.text == "-" ||
                    (line.text.size() >= 2 &&
                        line.text[0] == '-' &&
                        line.text[1] == ' ');
            }

            Node ParseBlock(std::size_t indent)
            {
                if (index_ >= lines_.size())
                    return {};

                if (lines_[index_].indent != indent)
                    Error("Unexpected indentation");

                if (IsSequenceLine(lines_[index_]))
                    return ParseSequence(indent);

                return ParseMapping(indent);
            }

            Node ParseMapping(std::size_t indent)
            {
                Node node;
                node.type = NodeType::Mapping;

                while (index_ < lines_.size() &&
                    lines_[index_].indent == indent &&
                    !IsSequenceLine(lines_[index_]))
                {
                    const Line line = lines_[index_++];
                    const auto separator = FindSeparator(line.text, ':');

                    if (separator == std::string_view::npos)
                        Error("Expected key: value");

                    const std::string key = Trim(
                        std::string_view(line.text).substr(0, separator)
                    );

                    if (key.empty())
                        Error("Empty mapping key");

                    const std::string value = Trim(
                        std::string_view(line.text).substr(separator + 1)
                    );

                    Node child;

                    if (!value.empty())
                    {
                        child = ParseInline(value);
                    }
                    else if (index_ < lines_.size() &&
                        lines_[index_].indent > indent)
                    {
                        child = ParseBlock(lines_[index_].indent);
                    }

                    node.mapping.emplace_back(key, std::move(child));
                }

                return node;
            }

            Node ParseSequence(std::size_t indent)
            {
                Node node;
                node.type = NodeType::Sequence;

                while (index_ < lines_.size() &&
                    lines_[index_].indent == indent &&
                    IsSequenceLine(lines_[index_]))
                {
                    const Line line = lines_[index_++];
                    const std::string item = line.text == "-"
                        ? ""
                        : Trim(std::string_view(line.text).substr(2));

                    if (item.empty())
                    {
                        if (index_ < lines_.size() &&
                            lines_[index_].indent > indent)
                        {
                            node.sequence.push_back(
                                ParseBlock(lines_[index_].indent)
                            );
                        }
                        else
                        {
                            node.sequence.emplace_back();
                        }

                        continue;
                    }

                    const auto separator = FindSeparator(item, ':');

                    if (separator == std::string_view::npos)
                    {
                        node.sequence.push_back(ParseInline(item));
                        continue;
                    }

                    Node object;
                    object.type = NodeType::Mapping;
                    AddMappingEntry(object, item);

                    if (index_ < lines_.size() &&
                        lines_[index_].indent > indent)
                    {
                        Node continuation = ParseBlock(lines_[index_].indent);

                        if (continuation.type != NodeType::Mapping)
                            Error("Expected mapping after sequence item");

                        for (auto& entry : continuation.mapping)
                        {
                            object.mapping.push_back(std::move(entry));
                        }
                    }

                    node.sequence.push_back(std::move(object));
                }

                return node;
            }
        };
    }

    Node Parse(std::string_view source)
    {
        return Parser(source).Run();
    }
}

// Temporary parser test
namespace Pumpkin::UI::YAML
{
    void TestParser()
    {
        std::ifstream file("Assets/UI/MainMenu.yaml");

        if (!file)
        {
            std::println("Impossible d'ouvrir MainMenu.yaml");
            return;
        }

        const std::string source{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        const Node root = Parse(source);

        auto canvas = Pumpkin::UI::Canvas::FromYAML(root);

        std::println("Canvas: {}", canvas.name);
        std::println(
            "Resolution: {}x{}",
            canvas.referenceWidth,
            canvas.referenceHeight
        );

        auto testScale = [&](Pumpkin::UI::CanvasScaleMode mode)
            {
                canvas.scaleMode = mode;

                const auto scale = canvas.CalculateScale(1280, 800);

                std::println(
                    "Scale: X={}, Y={}, OffsetX={}, OffsetY={}",
                    scale.x,
                    scale.y,
                    scale.offsetX,
                    scale.offsetY
                );
            };

        testScale(Pumpkin::UI::CanvasScaleMode::Fit);
        testScale(Pumpkin::UI::CanvasScaleMode::Stretch);
        testScale(Pumpkin::UI::CanvasScaleMode::ConstantPixelSize);

        canvas.scaleMode = Pumpkin::UI::CanvasScaleMode::Fit;

        const auto position = canvas.TransformPosition(
            100.0f,
            100.0f,
            1280,
            800
        );

        std::println(
            "Position transformee: X={}, Y={}",
            position.x,
            position.y
        );

        std::println("YAML parser OK");

        if (const Node* canvas = root.Find("canvas"))
        {
            if (const Node* name = canvas->Find("name"))
                std::println("Canvas: {}", name->value);
        }

        if (const Node* textures = root.Find("textures"))
            std::println("Textures: {}", textures->sequence.size());

        if (const Node* fonts = root.Find("fonts"))
            std::println("Fonts: {}", fonts->sequence.size());

        if (const Node* elements = root.Find("elements"))
            std::println("Elements: {}", elements->sequence.size());
    }
}