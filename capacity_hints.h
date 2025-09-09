#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <windows.h>

namespace HerosInsight
{
    // Helper to get and set capacity hints
    // Expected to be used together with vector like storage
    class CapacityHints
    {
    public:
        CapacityHints();
        ~CapacityHints();

        size_t get(const std::string &id) const;
        void update(const std::string &id, size_t capacity);

    private:
        std::unordered_map<std::string, size_t> hints_;
        std::string filename_;
    };

    inline CapacityHints g_capacityHints;

    template <typename Container>
    class CapacityGuard
    {
    public:
        CapacityGuard(Container &c, std::string_view id)
            : c_(c), id_(id)
        {
            c_.reserve(g_capacityHints.get(id_));
        }

        ~CapacityGuard()
        {
            g_capacityHints.update(id_, c_.size());
        }

    private:
        Container &c_;
        std::string_view id_;
    };

#define CAPACITY_ID __FILE__ ":" + std::to_string(__LINE__)

#define GET_VARIABLE_NAME(Variable) (#Variable)
}