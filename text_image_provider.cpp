#include <texture_module.h>
#include <utils.h>

#include "text_image_provider.h"

namespace HerosInsight::TextImageProvider
{
    using PacketArray = std::array<TextureModule::DrawPacket, Id::COUNT>;
    PacketArray CreatePackets()
    {
        namespace FileIDs = TextureModule::KnownFileIDs;

        PacketArray packets;
        const auto size = ImVec2(16, 16);
        packets[Id::MonsterSkull] = TextureModule::GetPacket_ImageInAtlas(FileIDs::UI_SkillStatsIcons, size, size, 5);
        packets[Id::EnergyOrb] = TextureModule::GetPacket_ImageInAtlas(FileIDs::UI_SkillStatsIcons, size, size, 1);

        return packets;
    }
    static PacketArray packets = CreatePackets();

    void Draw(ImVec2 pos, size_t id)
    {
        auto &packet = packets[id];
        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;
        packet.AddToDrawList(draw_list, pos);
    }

    float GetWidth(size_t id)
    {
        auto &packet = packets[id];
        return packet.size.x;
    }

    Utils::ImageDrawerFns GetImpl()
    {
        return Utils::ImageDrawerFns{
            .draw = Draw,
            .get_width = GetWidth,
        };
    }
}
