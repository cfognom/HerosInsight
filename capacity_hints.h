#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <windows.h>

namespace HerosInsight::CapacityHints
{
    // Helper to get and set capacity hints
    // Expected to be used together with vector like storage
    size_t GetHint(const std::string &id);
    void UpdateHint(const std::string &id, size_t capacity);
    void LoadHints();
    void SaveHints();

    template <typename Container>
    class CapacityGuard
    {
    public:
        CapacityGuard(Container &c, std::string_view id)
            : c_(c), id_(id)
        {
            c_.reserve(CapacityHints::GetHint(id_));
        }

        ~CapacityGuard()
        {
            CapacityHints::UpdateHint(id_, c_.size());
        }

    private:
        Container &c_;
        std::string_view id_;
    };

#define CAPACITY_ID std::format("{}:{}", __FILE__, __LINE__)

#define GET_VARIABLE_NAME(Variable) (#Variable)
}