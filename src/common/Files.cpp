#include "common/Files.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace ovc::files {

std::optional<std::string> read(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool writeAtomically(const fs::path& path, std::string_view content)
{
    fs::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out)
            return false;
    }
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error) {
        // Some file systems refuse to rename over an existing file.
        fs::remove(path, error);
        fs::rename(temporary, path, error);
    }
    return !error;
}

bool ensureDirectory(const fs::path& path)
{
    std::error_code error;
    if (fs::is_directory(path, error))
        return true;
    return fs::create_directories(path, error) && !error;
}

} // namespace ovc::files
