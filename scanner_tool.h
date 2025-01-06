#pragma once

#include <vector>
#include <cstdint>

class ScannerTool
{
public:
    void Initialize();
    bool TryScan(uint32_t value);
    void Reset();
    void DrawImGui();

private:
    bool TryScanRange(uint32_t value, uintptr_t start, uintptr_t end);
    std::optional<bool> success;
    std::vector<uintptr_t> results;
    std::vector<uintptr_t> previous_results;
    uint32_t scan_value;
};
