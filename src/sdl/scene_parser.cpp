//
// Created by Mike Smith on 2021/12/21.
//

#include <fstream>
#include <streambuf>
#include <double-conversion/double-conversion.h>

#include <core/logging.h>
#include <sdl/scene_parser.h>

namespace luisa::render {

inline SceneParser::SceneParser(SceneDesc &desc, const std::filesystem::path &path) noexcept
    : _desc{desc},
      _location{desc.register_path(std::filesystem::canonical(path))},
      _cursor{0u} {}

inline void SceneParser::_report_error(std::string_view message) const noexcept {
    LUISA_ERROR("{} [{}]", message, _location.string());
}

inline void SceneParser::_report_warning(std::string_view message) const noexcept {
    LUISA_WARNING("{} [{}]", message, _location.string());
}

#define LUISA_SCENE_PARSER_ERROR(...) \
    _report_error(fmt::format(__VA_ARGS__))

#define LUISA_SCENE_PARSER_WARNING(...) \
    _report_warning(fmt::format(__VA_ARGS__))

inline void SceneParser::_parse() noexcept {
    std::ifstream file{*_location.file()};
    _source = {
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}};
    _parse_file();
    _source.clear();
    _source.shrink_to_fit();
    _import_parsing.clear();
    _import_parsing.shrink_to_fit();
}

inline void SceneParser::_parse_file() noexcept {
    _skip_blanks();
    while (!_eof()) {
        auto loc = _location;
        if (auto token = _read_identifier(); token == "import") {// import
            _skip_blanks();
            std::filesystem::path path{_read_string()};
            if (!path.is_absolute()) { path = _location.file()->parent_path() / path; }
            // TODO: parallel parsing using thread pool
            _import_parsing.emplace_back(std::async(
                std::launch::async | std::launch::deferred,
                [path = std::move(path), &desc = _desc] {
                    SceneParser{desc, path}._parse();
                }));
        } else if (token == SceneDesc::root_node_identifier) {// root node
            _parse_root_node(loc);
        } else [[likely]] {// scene node
            _parse_global_node(loc, token);
        }
        _skip_blanks();
    }
}

inline void SceneParser::_match(char c) noexcept {
    if (auto got = _get(); got != c) [[unlikely]] {
        LUISA_SCENE_PARSER_ERROR(
            "Invalid character '{}' (expected '{}').",
            got, c);
    }
}

void SceneParser::_skip() noexcept { static_cast<void>(_get()); }

inline char SceneParser::_peek() noexcept {
    if (_eof()) [[unlikely]] { LUISA_SCENE_PARSER_ERROR("Premature EOF."); }
    auto c = _source[_cursor];
    if (c == '\r') {
        if (_source[_cursor + 1u] == '\n') { _cursor++; }
        return '\n';
    }
    return c;
}

inline char SceneParser::_get() noexcept {
    if (_eof()) [[unlikely]] { LUISA_SCENE_PARSER_ERROR("Premature EOF."); }
    auto c = _source[_cursor++];
    if (c == '\r') {
        if (_source[_cursor] == '\n') { _cursor++; }
        _location.set_line(_location.line() + 1u);
        _location.set_column(0u);
        return '\n';
    }
    if (c == '\n') {
        _location.set_line(_location.line() + 1u);
        _location.set_column(0u);
    } else {
        _location.set_column(_location.column() + 1u);
    }
    return c;
}

inline bool SceneParser::_eof() const noexcept {
    return _cursor >= _source.size();
}

inline std::string_view SceneParser::_read_identifier() noexcept {
    auto offset = _cursor;
    if (auto c = _get();
        c != '_' && c != '$' && !isalpha(c)) [[unlikely]] {
        LUISA_SCENE_PARSER_ERROR(
            "Invalid character '{}' in identifier.", c);
    }
    auto is_ident_body = [](auto c) noexcept {
        return isalnum(c) || c == '_' ||
               c == '$' || c == '-';
    };
    while (is_ident_body(_peek())) { _skip(); }
    return std::string_view{_source}
        .substr(offset, _cursor - offset);
}

inline double SceneParser::_read_number() noexcept {
    static thread_local double_conversion::StringToDoubleConverter converter{
        double_conversion::StringToDoubleConverter::ALLOW_HEX |
            double_conversion::StringToDoubleConverter::ALLOW_OCTALS |
            double_conversion::StringToDoubleConverter::ALLOW_HEX_FLOATS |
            double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK |
            double_conversion::StringToDoubleConverter::ALLOW_TRAILING_SPACES |
            double_conversion::StringToDoubleConverter::ALLOW_SPACES_AFTER_SIGN |
            double_conversion::StringToDoubleConverter::ALLOW_CASE_INSENSIBILITY,
        0.0, 0.0, nullptr, nullptr};
    auto s = std::string_view{_source}.substr(_cursor);
    auto processed_count = 0;
    auto value = converter.StringToDouble(
        s.data(),
        static_cast<int>(s.size()),
        &processed_count);
    if (processed_count == 0) [[unlikely]] {
        LUISA_SCENE_PARSER_ERROR(
            "Invalid number string '{}...'.",
            s.substr(0, 4));
    }
    _cursor += processed_count;
    _location.set_column(_location.column() + processed_count);
    return value;
}

inline bool SceneParser::_read_bool() noexcept {
    using namespace std::string_view_literals;
    if (_peek() == 't') {
        for (auto x : "true"sv) { _match(x); }
        return true;
    }
    for (auto x : "false"sv) { _match(x); }
    return false;
}

inline luisa::string SceneParser::_read_string() noexcept {
    auto quote = _get();
    if (quote != '"' && quote != '\'') [[unlikely]] {
        LUISA_SCENE_PARSER_ERROR("Expected string but got {}.", quote);
    }
    luisa::string s;
    for (auto c = _get(); c != quote; c = _get()) {
        if (!isprint(c)) [[unlikely]] {
            LUISA_SCENE_PARSER_ERROR(
                "Unexpected non-printable character 0x{:02x}.",
                static_cast<int>(c));
        }
        if (c == '\\') [[unlikely]] {// escape
            c = [this, esc = _get()] {
                switch (esc) {
                    case 'b': return '\b';
                    case 'f': return '\f';
                    case 'n': return '\n';
                    case 'r': return '\r';
                    case 't': return '\t';
                    case '\\': return '\\';
                    case '\'': return '\'';
                    case '"': return '\"';
                    default: LUISA_SCENE_PARSER_ERROR(
                        "Invalid escaped character '{}'.", esc);
                }
            }();
        }
        s.push_back(c);
    }
    return s;
}

inline void SceneParser::_skip_blanks() noexcept {
    while (!_eof()) {
        if (auto c = _peek(); isblank(c) || c == '\n') {// blank
            _skip();
        } else if (c == '/') {// comment
            _skip();
            _match('/');
            while (!_eof() && _get() != '\n') {}
        } else {
            break;
        }
    }
}

inline void SceneParser::_parse_root_node(SceneNodeDesc::SourceLocation l) noexcept {
    _parse_node_body(_desc.define_root(l));
}

inline void SceneParser::_parse_global_node(SceneNodeDesc::SourceLocation l, std::string_view tag_desc) noexcept {
    using namespace std::string_view_literals;
    static const luisa::unordered_map<std::string_view, SceneNodeTag> desc_to_tag{
        {"Camera"sv, SceneNodeTag::CAMERA},
        {"Cam"sv, SceneNodeTag::CAMERA},
        {"Shape"sv, SceneNodeTag::SHAPE},
        {"Material"sv, SceneNodeTag::MATERIAL},
        {"Mat"sv, SceneNodeTag::MATERIAL},
        {"LightSource"sv, SceneNodeTag::LIGHT},
        {"Light"sv, SceneNodeTag::LIGHT},
        {"Transform"sv, SceneNodeTag::TRANSFORM},
        {"Xform"sv, SceneNodeTag::TRANSFORM},
        {"Film"sv, SceneNodeTag::FILM},
        {"Filter"sv, SceneNodeTag::FILTER},
        {"Sampler"sv, SceneNodeTag::SAMPLER},
        {"Integrator"sv, SceneNodeTag::INTEGRATOR},
        {"Environment"sv, SceneNodeTag::ENVIRONMENT},
        {"Env"sv, SceneNodeTag::ENVIRONMENT}};
    auto iter = desc_to_tag.find(tag_desc);
    if (iter == desc_to_tag.cend()) [[unlikely]] {
        LUISA_SCENE_PARSER_ERROR(
            "Invalid scene node type '{}'.",
            tag_desc);
    }
    auto tag = iter->second;
    _skip_blanks();
    auto name = _read_identifier();
    _skip_blanks();
    _match(':');
    _skip_blanks();
    auto impl_type = _read_identifier();
    _parse_node_body(_desc.define(name, tag, impl_type, l));
}

void SceneParser::_parse_node_body(SceneNodeDesc *node) noexcept {
    _skip_blanks();
    _match('{');
    _skip_blanks();
    while (_peek() != '}') {
        auto prop = _read_identifier();
        _skip_blanks();
        if (auto c = _peek(); c == ':') {// inline node
            _skip();
            _skip_blanks();
            auto loc = _location;
            auto impl_type = _read_identifier();
            auto internal_node = node->define_internal(impl_type, loc);
            _parse_node_body(internal_node);
            node->add_property(prop, internal_node);
        } else {
            node->add_property(prop, _parse_value_list(node));
        }
        _skip_blanks();
    }
    _match('}');
}

inline SceneNodeDesc::value_list SceneParser::_parse_value_list(SceneNodeDesc *node) noexcept {
    _match('{');
    _skip_blanks();
    auto value_list = [node, this]() noexcept -> SceneNodeDesc::value_list {
        auto c = _peek();
        if (c == '}') [[unlikely]] { LUISA_SCENE_PARSER_ERROR("Empty value list."); }
        if (c == '@' || isupper(c)) { return _parse_node_list_values(node); }
        if (c == '"' || c == '\'') { return _parse_string_list_values(); }
        if (c == 't' || c == 'f') { return _parse_bool_list_values(); }
        return _parse_number_list_values();
    }();
    _skip_blanks();
    _match('}');
    return value_list;
}

SceneNodeDesc::number_list SceneParser::_parse_number_list_values() noexcept {
    SceneNodeDesc::number_list list;
    list.emplace_back(_read_number());
    _skip_blanks();
    while (_peek() != '}') {
        _match(',');
        _skip_blanks();
        list.emplace_back(_read_number());
        _skip_blanks();
    }
    return list;
}

SceneNodeDesc::bool_list SceneParser::_parse_bool_list_values() noexcept {
    SceneNodeDesc::bool_list list;
    list.emplace_back(_read_bool());
    _skip_blanks();
    while (_peek() != '}') {
        _match(',');
        _skip_blanks();
        list.emplace_back(_read_bool());
        _skip_blanks();
    }
    return list;
}

SceneNodeDesc::node_list SceneParser::_parse_node_list_values(SceneNodeDesc *node) noexcept {

    auto parse_ref_or_def = [node, this]() noexcept -> const auto * {
        if (_peek() == '@') {// reference
            _skip();
            _skip_blanks();
            return _desc.reference(_read_identifier());
        }
        // inline definition
        auto loc = _location;
        auto impl_type = _read_identifier();
        auto internal_node = node->define_internal(impl_type, loc);
        _parse_node_body(internal_node);
        return internal_node;
    };

    SceneNodeDesc::node_list list;
    list.emplace_back(parse_ref_or_def());
    _skip_blanks();
    while (_peek() != '}') {
        _match(',');
        _skip_blanks();
        list.emplace_back(parse_ref_or_def());
        _skip_blanks();
    }
    return list;
}

SceneNodeDesc::string_list SceneParser::_parse_string_list_values() noexcept {
    SceneNodeDesc::string_list list;
    list.emplace_back(_read_string());
    _skip_blanks();
    while (_peek() != '}') {
        _match(',');
        _skip_blanks();
        list.emplace_back(_read_string());
        _skip_blanks();
    }
    return list;
}

luisa::unique_ptr<SceneDesc> SceneParser::parse(const path &entry_file) noexcept {
    auto desc = luisa::make_unique<SceneDesc>();
    SceneParser{*desc, entry_file}._parse();
    return desc;
}

#undef LUISA_SCENE_PARSER_ERROR
#undef LUISA_SCENE_PARSER_WARNING

}// namespace luisa::render
