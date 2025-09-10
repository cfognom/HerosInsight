#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <windows.h>

#include "capacity_hints.h"

std::string GetDefaultFilePath()
{
    constexpr auto filename = "capacity_hints.txt";

    // Get the DLL handle based on this function's address
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetDefaultFilePath),
            &hMod))
    {
        return filename; // fallback to CWD
    }

    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(hMod, path, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return filename; // fallback

    std::filesystem::path dllPath(path);
    return (dllPath.parent_path() / filename).string();
}

void LoadHints(const std::string &filename, std::unordered_map<std::string, size_t> &hints)
{
    std::ifstream f(filename);
    if (!f) return;

    std::string id;
    size_t cap;
    while (f >> id >> cap)
    {
        hints[id] = cap;
    }
}

void SaveHints(const std::string &filename, std::unordered_map<std::string, size_t> &hints)
{
    std::ofstream f(filename, std::ios::trunc);
    if (!f) return;

    std::vector<std::pair<std::string, size_t>> items(hints.begin(), hints.end());
    std::sort(items.begin(), items.end(),
        [](auto &a, auto &b)
        {
            return a.first < b.first;
        });

    for (auto &[id, cap] : items)
    {
        f << id << " " << cap << "\n";
    }
}

HerosInsight::CapacityHints::CapacityHints()
{
    filename_ = GetDefaultFilePath();
    LoadHints(filename_, hints_);
}

HerosInsight::CapacityHints::~CapacityHints()
{
    SaveHints(filename_, hints_);
}

size_t HerosInsight::CapacityHints::get(const std::string &id) const
{
    auto it = hints_.find(id);
    return it != hints_.end() ? it->second : 0;
}

void HerosInsight::CapacityHints::update(const std::string &id, size_t capacity)
{
    hints_[id] = capacity;
}
