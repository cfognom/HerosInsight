#pragma once

#include <d3dx9math.h>
#include <format>
#include <string>
#include <utils.h>

struct IDirect3DDevice9;

namespace HerosInsight::DebugDisplay
{
    void ClearDisplay(std::string filter);
    void PushToDisplay(const std::string &key, const wchar_t *value);
    void PushToDisplay(const std::string &key, const uint32_t value);
    void PushToDisplay(const std::string &key, const std::string &value);
    void PushToDisplay(const std::string &key, const D3DXMATRIX &matrix);
    void PushToDisplay(const std::string &key, const D3DXVECTOR2 &vector);
    void PushToDisplay(const std::string &key, const D3DXVECTOR3 &vector);
    template <typename... Args>
    void PushToDisplay(const std::wstring &format_str, Args &&...args)
    {
        const auto message = std::vformat(format_str, std::make_wformat_args(args...));
        PushToDisplay(Utils::WStrToStr(message.c_str()), "");
    }
    template <typename... Args>
    void PushToDisplay(const std::string &key, const std::wstring &format_str, Args &&...args)
    {
        const auto message = std::vformat(format_str, std::make_wformat_args(std::forward<Args>(args)...));
        PushToDisplay(key, Utils::WStrToStr(message.c_str()));
    }

    void Draw(IDirect3DDevice9 *device);
}