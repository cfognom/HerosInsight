#include "scanner_tool.h"
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <imgui.h>
#include <windows.h>
#include <utils.h>

void ScannerTool::Initialize()
{
    GW::Scanner::Initialize();
}

bool ScannerTool::TryScanRange(uint32_t value, uintptr_t start, uintptr_t end)
{
    if (start >= end)
        return false;

    __try
    {
        uintptr_t found = GW::Scanner::FindInRange(reinterpret_cast<const char *>(&value), "xxxx", 0, start, end);
        while (found)
        {
            results.push_back(found);
            auto start_new = found + 1;
            if (start_new >= end)
                break;
            found = GW::Scanner::FindInRange(reinterpret_cast<const char *>(&value), "xxxx", 0, start_new, end);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void GetAddressRange(uintptr_t &start, uintptr_t &end)
{
    auto section = GW::ScannerSection::Section_DATA;
    GW::Scanner::GetSectionAddressRange(section, &start, &end);
    // The GetSectionAddressRange is bugged and returns the wrong values because
    // it doesn't subtract the section_offset_from_disk, we have to do it manually,
    // but it is prone to errors if the section changes
    auto section_offset_from_disk = 2772107264;
    start -= section_offset_from_disk;
    end -= section_offset_from_disk;
    SOFT_ASSERT(GW::Scanner::IsValidPtr(start + 1, section), L"Invalid start address");
    SOFT_ASSERT(GW::Scanner::IsValidPtr(end - 1, section), L"Invalid end address");
}

bool ScannerTool::TryScan(uint32_t value)
{
    results.clear();
    success = std::nullopt;
    GW::Scanner::Initialize("Gw.exe");

    uintptr_t start, end;
    GetAddressRange(start, end);

    bool success;
    if (previous_results.empty())
    {
        success = TryScanRange(value, start, end);
    }
    else
    {
        for (auto address : previous_results)
        {
            __try
            {
                if (*reinterpret_cast<uint32_t *>(address) == value)
                {
                    results.push_back(address);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                // Skip over problematic address
            }
        }
        success = true;
    }

    previous_results = results;
    return success;
}

void ScannerTool::Reset()
{
    results.clear();
    previous_results.clear();
    success = std::nullopt;
}

void ScannerTool::DrawImGui()
{
    ImGui::Begin("Scanner Tool");
    ImGui::InputScalar("Value to scan", ImGuiDataType_U32, &scan_value);
    if (ImGui::Button("First Scan"))
    {
        Reset();
        success = TryScan(scan_value);
    }
    ImGui::SameLine();
    if (ImGui::Button("Next Scan"))
    {
        success = TryScan(scan_value);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        Reset();
    }
    ImGui::Text("Results:");
    if (success.has_value())
    {
        if (success.value())
        {
            ImGui::Text("Scan successful");
        }
        else
        {
            ImGui::Text("Scan failed");
        }
    }
    uint32_t i = 0;
    for (auto address : results)
    {
        ImGui::Text("0x%08X", address);
        if (i++ > 100)
        {
            ImGui::Text("Too many results, stopping here");
            break;
        }
        i++;
    }
    ImGui::End();
}
