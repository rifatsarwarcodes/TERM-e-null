#include "platform/Console.hpp"

#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace ovc::platform {

void setInputEcho(bool enabled)
{
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode))
        return;
    mode = enabled ? (mode | ENABLE_ECHO_INPUT) : (mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    SetConsoleMode(input, mode);
#else
    termios settings{};
    if (tcgetattr(STDIN_FILENO, &settings) != 0)
        return;
    if (enabled)
        settings.c_lflag |= ECHO;
    else
        settings.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &settings);
#endif
}

std::string readSecretLine(std::istream& in, std::ostream& out)
{
    const bool interactive = &in == &std::cin;
    if (interactive)
        setInputEcho(false);
    std::string line;
    std::getline(in, line);
    if (interactive) {
        setInputEcho(true);
        out << "\n";
    }
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

} // namespace ovc::platform
