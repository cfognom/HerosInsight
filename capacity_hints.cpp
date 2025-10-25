#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <windows.h>

#include "capacity_hints.h"
#include <constants.h>

std::unordered_map<std::string, size_t> hints_map;

std::filesystem::path GetFilePath()
{
    return Constants::paths.cache() / "capacity_hints.txt";
}

void HerosInsight::CapacityHints::LoadHints()
{
    auto filename = GetFilePath();
    std::ifstream f(filename);
    if (!f) return;

    std::string id;
    size_t cap;
    while (f >> id >> cap)
    {
        hints_map[id] = cap;
    }
}

void HerosInsight::CapacityHints::SaveHints()
{
    auto filename = GetFilePath();
    std::ofstream f(filename, std::ios::trunc);
    if (!f) return;

    std::vector<std::pair<std::string, size_t>> items(hints_map.begin(), hints_map.end());
    std::sort(items.begin(), items.end(), [](auto &a, auto &b)
              { return a.first < b.first; });

    for (auto &[id, cap] : items)
    {
        f << id << " " << cap << "\n";
    }
}

size_t HerosInsight::CapacityHints::GetHint(const std::string &id)
{
    auto it = hints_map.find(id);
    return it != hints_map.end() ? it->second : 0;
}

void HerosInsight::CapacityHints::UpdateHint(const std::string &id, size_t capacity)
{
    auto &stored = hints_map[id];
    if (stored < capacity)
        stored = capacity;
}
