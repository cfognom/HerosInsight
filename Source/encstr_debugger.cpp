#ifdef _DEBUG

#include <GWCA/Context/TextParser.h>
#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <span_vector.h>
#include <update_manager.h>
#include <utils.h>
#include <variable_size_clipper.h>

#include "encstr_debugger.h"

namespace HerosInsight::EncstrDebugger
{
    /*
        Notable gw str id values:
        0x0: Misc
        0x885: Interesting names + descriptions for heros/builds/characters
        0x2621: Proph item names
        ???: Proph outpost names + descriptions
        0x5382: Canthan item names
        0x5630: Canthan outpost names
        0x607c: More outpost/region names
        0x6087: Skill names + descriptions
        0x6eeb: More armor names
        0x8095: Misc
        0x81fa: Skill names + descriptions
        0x84a6: Skill names + descriptions
        0x8dd5: Skill names + descriptions
        0xaa88: Misc
        0xabd1: Skill names + descriptions
        0xb027: Outpost names + descriptions
        0xbde4: Monument/sign names + descriptions?
        ...
        0x19000: Crash
    */
    constexpr uint32_t max_str_id = 0x187ca; // ???

    wchar_t hexToWchar(const char *&p)
    {
        // Skip past '\x'
        p += 2;
        wchar_t value = 0;
        while (isxdigit(*p))
        {
            char c = *p;
            value *= 16;
            if (c >= '0' && c <= '9')
                value += c - '0';
            else if (c >= 'a' && c <= 'f')
                value += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                value += c - 'A' + 10;
            ++p;
        }
        --p; // adjust because caller will ++
        return value;
    }

    void convertCharBufferToWCharBuffer(const char *input, OutBuf<wchar_t> out)
    {
        for (const char *p = input; *p != '\0'; ++p)
        {
            if (*p == '\\' && *(p + 1) == 'x')
            {
                out.push_back(hexToWchar(p));
            }
            else
            {
                out.push_back(static_cast<wchar_t>(*p));
            }
        }
        out.push_back(L'\0');
    }

    void DrawEncStrBuilder()
    {
        ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
        ImGui::Text("Builder");
        ImGui::PopFont();

        static char input_buffer[1024] = {'\0'};
        static FixedVector<wchar_t, 1024> enc_buffer;
        static std::string feedback;
        static bool decode_requested = false;

        ImGui::InputText(
            "Encoded Input",
            input_buffer, sizeof(input_buffer),
            ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AutoSelectAll,
            [](ImGuiInputTextCallbackData *data)
            {
                enc_buffer.clear();
                convertCharBufferToWCharBuffer(data->Buf, enc_buffer);

                auto end = enc_buffer.data() + enc_buffer.size();
                bool is_valid = GW::UI::IsValidEncStr(enc_buffer.data(), end);

                if (is_valid)
                {
                    feedback = "Decoding...";
                    decode_requested = true;
                }
                else
                {
                    feedback = "Invalid Input";
                }

                return 0;
            },
            nullptr
        );

        if (decode_requested)
        {
            auto result = Utils::TryDecodeString(enc_buffer.data());
            if (result.has_value())
            {
                auto &decoded = result.value();
                auto str = Utils::WStrToStr(decoded.data(), decoded.data() + decoded.size());
                feedback = std::format("Decoded Output: '{}'", str);
                decode_requested = false;
            }
        }

        ImGui::Text(feedback.data());
    }

    void DrawExplorer()
    {
        static SpanVector<wchar_t> decoded_strings;
        static VariableSizeClipper clipper{};

        struct Entry
        {
            size_t vec_index = -1;
            uint16_t wLength;
            uint16_t wType;
            uint16_t wValueLength;

            bool IsInit() const { return vec_index != -1; }

            std::string GetStr() const
            {
                std::wstring_view wstr(decoded_strings[vec_index]);
                return Utils::WStrToStr(wstr.data(), wstr.data() + wstr.size());
            }
        };

        static std::vector<Entry> entries;

        ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
        ImGui::Text("Explorer");
        ImGui::PopFont();

        auto count = GW::AssetMgr::GetStringIdEnd(GW::Constants::Language::English);
        entries.resize(count);

        static char hex_buffer[9]{'\0'};
        if (ImGui::InputText("Jump to (hex)", hex_buffer, sizeof(hex_buffer), ImGuiInputTextFlags_CharsHexadecimal))
        {
            auto input_value = static_cast<uint32_t>(strtoul(hex_buffer, nullptr, 16));
            clipper.SetScroll({input_value, 0.f});
        }

        static auto InitEntry = [](size_t i)
        {
            auto cache = GW::AssetMgr::GetTextCache();
            auto file_index = i / cache->m_stringsPerFile;
            auto files = cache->GetFiles(GW::Constants::Language::English);
            if (file_index < files.size())
            {
                auto &file = files[file_index];
                auto readable = GW::AssetMgr::ReadableFile(file.path);
                if (readable)
                {
                    size_t i = file_index * cache->m_stringsPerFile;
                    for (auto &header : readable.Strings())
                    {
                        assert(*(wchar_t *)&header != L'\0');
                        decoded_strings.Elements().append_range(header.GetStr());
                        auto vec_index = decoded_strings.CommitWritten();
                        auto &entry = entries[i++];
                        assert(!entry.IsInit());
                        entry.vec_index = vec_index;
                        entry.wLength = header.wLength;
                        entry.wType = header.wType;
                        entry.wValueLength = header.wValueLength;
                    }
                }
                ++file_index;
            }
        };

        ImGui::Separator();

        if (ImGui::BeginChild("EncStrs"))
        {
            clipper.Draw(
                entries.size(), 100, true,
                [](uint32_t i)
                {
                    auto &entry = entries[i];

                    if (!entry.IsInit())
                    {
                        InitEntry(i);
                        assert(entry.IsInit());
                    }

                    ImGui::Text("GW str id: %#x", i);
                    ImGui::Text("wLength: %u", entry.wLength);
                    ImGui::Text("wType: %u", entry.wType);
                    ImGui::Text("wValueLength: %u", entry.wValueLength);
                    ImGui::Text("string: '%s'", entry.GetStr().data());
                    ImGui::Separator();
                }
            );
        }
        ImGui::EndChild();
    }

    void Draw()
    {
        ImGui::SetNextWindowPos(ImVec2(600, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Encoded String Debugger", &UpdateManager::open_encstr_debugger))
        {
            DrawEncStrBuilder();
            ImGui::Separator();
            DrawExplorer();
        }

        ImGui::End();
    }
}

#endif