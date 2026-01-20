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

    void ConnectHighlighting(std::string_view text, std::span<uint16_t> &hl)
    {
        if (hl.size() < 4)
            return;

        size_t dst_idx = 1;
        size_t i = 1;
        for (; i + 1 < hl.size(); i += 2)
        {
            auto begin = hl[i];
            auto end = hl[i + 1];
            auto between = text.substr(begin, end - begin);
            for (auto c : between)
            {
                if (c != ' ')
                {
                    hl[dst_idx++] = begin;
                    hl[dst_idx++] = end;
                    break;
                }
            }
        }
        hl[dst_idx++] = hl[i];
        hl = hl.subspan(0, dst_idx);
    }
}
