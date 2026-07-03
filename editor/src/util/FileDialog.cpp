#include "util/FileDialog.hpp"

#ifdef _WIN32
  #include <windows.h>
  #include <commdlg.h>
  #include <filesystem>
#endif

namespace editor
{

#ifdef _WIN32

std::string openFileDialog(const char* filter, const std::string& startDir)
{
    char fileBuf[1024] = {0};

    // Risolve la directory iniziale in percorso assoluto
    std::string absDir;
    if (!startDir.empty())
    {
        std::error_code ec;
        std::filesystem::path p = std::filesystem::absolute(startDir, ec);
        if (!ec) absDir = p.string();
    }

    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = fileBuf;
    ofn.nMaxFile    = sizeof(fileBuf);
    ofn.lpstrInitialDir = absDir.empty() ? nullptr : absDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
        return std::string(fileBuf);
    return std::string();
}

#else

std::string openFileDialog(const char*, const std::string&)
{
    return std::string(); // non implementato su piattaforme non-Windows
}

#endif

} // namespace editor
