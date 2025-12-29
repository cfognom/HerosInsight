#pragma once

#include "filtering.h"
#include <span>
#include <vector>

namespace HerosInsight::Filtering
{
    void SortHighlighting(std::span<uint16_t> &hl)
    {
        assert(hl.size() % 2 == 0);

        // Add a least significant bit to each hl value and mark the closings (every other value)
        for (size_t i = 0; i < hl.size(); ++i)
        {
            hl[i] = (hl[i] << 1) | (i & 1);
        }

        // Sort the values
        std::sort(hl.begin(), hl.end());

        // Erase bad openings and closings
        size_t hl_level = 0;
        auto Remover = [&hl_level](uint16_t v)
        {
            bool is_closing = v & 1;
            hl_level += is_closing ? -1 : 1;
#ifdef _DEBUG
            assert(hl_level != -1);
#endif
            bool keep = is_closing ? hl_level == 0 : hl_level == 1;
            return !keep;
        };
        auto it = std::remove_if(hl.begin(), hl.end(), Remover);
        hl = std::span<uint16_t>(hl.data(), it - hl.begin());

        // Remove the least significant bit
        for (auto &v : hl)
        {
            v >>= 1;
        }
    }

    void SortHighlighting(std::vector<uint16_t> &hl)
    {
        std::span<uint16_t> span = hl;
        SortHighlighting(span);
        hl.resize(span.size());
    }

}
