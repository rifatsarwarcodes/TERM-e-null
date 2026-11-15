#include "editor/Origin.hpp"

#include "common/Strings.hpp"

namespace ovc::editor {

namespace {

char kindLetter(OriginKind kind)
{
    switch (kind) {
    case OriginKind::Typed: return 'T';
    case OriginKind::Loaded: return 'L';
    case OriginKind::Inside: return 'I';
    case OriginKind::Self: return 'S';
    case OriginKind::Allowed: return 'A';
    case OriginKind::Outside: return 'O';
    }
    return 'T';
}

} // namespace

std::string Origin::encode() const
{
    std::string text(1, kindLetter(kind));
    if (isPaste())
        text += "#" + std::to_string(pasteId);
    return text;
}

Origin Origin::decode(std::string_view text)
{
    if (text.empty())
        return typed();
    Origin origin;
    switch (text[0]) {
    case 'L': origin.kind = OriginKind::Loaded; break;
    case 'I': origin.kind = OriginKind::Inside; break;
    case 'S': origin.kind = OriginKind::Self; break;
    case 'A': origin.kind = OriginKind::Allowed; break;
    case 'O': origin.kind = OriginKind::Outside; break;
    default: origin.kind = OriginKind::Typed; break;
    }
    const auto hash = text.find('#');
    if (hash != std::string_view::npos) {
        std::uint64_t id = 0;
        if (strings::parseUnsigned(text.substr(hash + 1), id))
            origin.pasteId = static_cast<int>(id);
    }
    return origin;
}

const char* originKindName(OriginKind kind)
{
    switch (kind) {
    case OriginKind::Typed: return "typed";
    case OriginKind::Loaded: return "loaded";
    case OriginKind::Inside: return "inside";
    case OriginKind::Self: return "self";
    case OriginKind::Allowed: return "allowed";
    case OriginKind::Outside: return "outside";
    }
    return "typed";
}

} // namespace ovc::editor
