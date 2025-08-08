#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/GameEntities/Skill.h>
#include <d3d9.h>
#include <imgui.h>

namespace TextureModule
{
    void Initialize();
    void Terminate();
    void DxUpdate(IDirect3DDevice9 *device);

    D3DSURFACE_DESC GetTextureDesc(IDirect3DTexture9 *texture);
    IDirect3DTexture9 **LoadTextureFromFileId(uint32_t file_id);
    IDirect3DTexture9 **GetSkillImage(GW::Constants::SkillID skill_id);
    IDirect3DTexture9 **GetResourceTexture(const char *filename);
    bool DrawSkill(const GW::Skill &skill, ImVec2 pos, float icon_size, bool as_effect = false, bool as_hovered = false, ImDrawList *draw_list = nullptr);
    void GetImageUVsInAtlas(IDirect3DTexture9 *texture, ImVec2 image_size, uint32_t index, ImVec2 &uv0, ImVec2 &uv1);
    void OffsetUVsByPixels(ImVec2 source_texture_size, ImVec2 &uv, ImVec2 offset);

    struct DrawPacket
    {
        IDirect3DTexture9 **tex;
        ImVec2 size;
        ImVec2 uv0;
        ImVec2 uv1;
        ImVec4 tint_col;
        ImVec4 border_col;

        bool CanDraw()
        {
            return tex && *tex;
        }

        bool DrawOnWindow();
        bool AddToDrawList(ImDrawList *draw_list, ImVec2 position, float rot_rads = 0.0f);
    };

    DrawPacket GetPacket_Image(
        uint32_t file_id,
        ImVec2 size,
        ImVec2 uv0 = ImVec2(0, 0),
        ImVec2 uv1 = ImVec2(1, 1),
        ImVec4 tint_col = ImVec4(1, 1, 1, 1),
        ImVec4 border_col = ImVec4(0, 0, 0, 0));

    DrawPacket GetPacket_ImageInAtlas(
        uint32_t file_id,
        ImVec2 size,
        ImVec2 atlas_image_size,
        uint32_t atlas_index,
        ImVec2 uv0_pixel_offset = ImVec2(0, 0),
        ImVec2 uv1_pixel_offset = ImVec2(0, 0),
        ImVec4 tint_col = ImVec4(1, 1, 1, 1),
        ImVec4 border_col = ImVec4(0, 0, 0, 0));

    enum struct DamageNumberColor
    {
        Red,
        Green,
        Blue,
        Yellow,
        Pink,
    };

    ImVec2 CalculateDamageNumberSize(int32_t number, float scale);
    void DrawDamageNumber(int32_t number, ImVec2 pos, float scale, DamageNumberColor color = DamageNumberColor::Yellow, ImDrawList *draw_list = nullptr);

    /* ------------- Known file_id's -------------

    8620 - UI Green liquid dripping animation
    61201 - Frames/Borders of some kind
    93156/151243 - Called Target UI icons
    95099 - Worldspace health bar
    104587 - Worldspace target marker
    104946/153580 - Skill borders/overlays (includes elite border)
    107278 - Stack size numbers (0-9, Checkbox and diamond shape)
    8957 - Cursor
    9075 - Cursor (green outline)
    9077 - Cursor (orange outline)
    9085 - Cursor (mini)
    107948 - Cursor (Resize window)
    111825 - Cursor (Salvage hammer)
    122693 - Cursor (Trident, zoom/rotate)
    134682 - Cursor (Hand pointing)
    134684 - Cursor (Hand picking/ok sign)
    134684 - Cursor (Hand picking/ok sign)
    134686 - Cursor (Typing)
    134688 - Cursor (Resize window l/r)
    134690 - Cursor (Resize window u/d)
    134692 - Cursor (Resize window /)
    134694 - Cursor (Resize window \)
    8976/277146 - Lightning symbol that appears on ping
    9125 - Blue guy showing how many players in party
    9131 - Skill stats icons (energy, recharge, ...)
    108152/143008 - Item rarity border/background
    108154/143009 - (Unused?) item overlays (border and disabled icon)
    111776 - Some UI buttons
    111795 - UI checkboxes
    111804 - Small UI box
    111805 - Large UI box
    112323 - Large UI box
    111829 - Quest destination markers (star and arrow)
    122674/143010 - Platinum and gold UI icons
    122676 - Inventory Bin icon
    122679/151234 - Dark UI square
    122681/151235 - UI square
    122683/151236 - UI square
    122685 - Skill locked overlay
    134696 - UI Indentation
    134697 - UI Indentation
    134698 - UI Indentation (Used for money in inventory)
    134699 - UI Checkboxes with multiple states (These are used in the "Enable Weapon Sets" menu)
    134702 - Green UI button (styled as the "Weapon Sets" button)
    134703 - Disabled UI button (styled as the "Weapon Sets" button)
    134704 - Blue UI button (styled as the "Weapon Sets" button)
    134705 - Saturated Blue UI button (styled as the "Weapon Sets" button)
    134707 - Big Disabled UI button (styled as the "Weapon Sets" button)
    134708 - Big Blue UI button (styled as the "Weapon Sets" button)
    134709 - Saturated Blue UI button (styled as the "Weapon Sets" button)
    134710 - UI Window borders
    134711 - UI Window borders 2
    134712 - UI Window borders 3
    134713 - UI Blue dropdown (district selection)
    134714 - UI border
    134715 - UI Light blue dropdown (district selection)
    134716 - UI thing
    134717 - UI thing
    134720 - UI thing
    134721 - UI tab?
    134722 - UI tab?
    134723 - UI vertical scrollbar caps
    134724 - UI horizontal scrollbar caps
    134725 - UI vertical mid
    134726 - UI horizontal mid
    134727 - UI vertical scroll handle
    134728 - UI horizontal scroll handle
    134729 - UI thing
    134730 - UI checkbox?
    134731 - UI Window header
    134732 - UI Inventory bag header
    134757 - UI Inventory equpiment background
    143004 - UI Effect symbol (the little icon in the top right of the skill icon for enchantments/conditions/hexes/weapon spells)
    143005 - UI Skill hover window border and background
    143014 - UI Chat open/close button
    143015 - UI Compass N/S/E/W
    143016 - UI Compass border and aggro range
    143017 - UI Compass lens
    143021-24 - UI things
    143027 - Small material icon atlas
    143031 - Skill bound key (appears on skills in the skillbar)
    143032 - Skill bound key but larger
    143762 - UI buttons close/makewindow/minimize
    143778 - UI Menu background
    143780 - UI Open Menu button
    143781/294790 - UI Menu buttons
    144524 - UI Skill border?
    144533 - UI Checkboxes with states
    144536 - Profession icon atlas
    144537 - UI Small symbols
    144538 - UI Worldspace chat message background
    144758 - UI Weapon set bg
    151271 - UI Arrows u/d multiple states
    151300 - UI "Can not salvage this item" icon
    151338 - UI Friend'slist friend online/offline icon
    151387 - UI Inventory bag icons
    151391 - UI Mission map crosshair
    151817-18 - UI Blue bg and x buttons
    151819-20 - UI Red bg and x buttons
    152635-36 - UI weapon icons
    152638-39 - UI profession icons
    152641 - UI Round green selector
    153534-153567 - UI Stuff
    158160/265558 - UI Skill weapon req
    158162/164479 - UI Skill weapon req not satisfied (the red stripe)
    158269-70 - UI stuff
    158484-7 - UI stuff
    164476 - UI Skill lead/offhand/enc/dual/hex/weapon spell icons
    164477 - UI Skill missing req lead/offhand/enc/dual/hex icons
    164481 - UI lead/offhand/dual icons
    175149 - UI outpost map icon
    175151 - UI city map icon
    191456-191492 - UI Small mission map icons
    205419-22 - UI Cast bars
    205423-25 - UI Skill hover bg but purple/white/gold
    205426 - UI Bleeding bar
    205427 - UI Hexed bar
    205428 - UI Grey bar
    205429 - UI Energy bar
    205430 - UI Experience bar
    205431 - UI Health bar
    205432 - UI Purple bar
    205433 - UI Overcast bar
    205434 - UI Poisoned bar
    205435 - UI Bleeding bar?
    205436 - UI Empty bar?
    205437 - UI Bar border
    256553 - UI Compass thingy?
    256680 - UI Compass hero panel start
    256681 - UI Compass hero buttons with states
    256682 - UI Map hero flag icons
    257172/265561 - UI Skillbar slot with hourglass
    265534 - Cursor (Salvage inspect)
    265536 - Cursor (Flag all heroes)
    265538 - Cursor (Flag hero 1)
    265540 - Cursor (Flag hero 2)
    265542 - Cursor (Flag hero 3)
    265547 - Cursor small?
    265549 - UI Skill hover bg?
    265556/368769 - UI Skill/effect borders (including elite)
    265559 - UI Skill slot empty
    265562 - UI Skill slot selected
    265563 - UI Hero behaviour icons
    265564 - UI Hero lock target icons
    265565 - UI Blue numbers (healing)
    265566 - UI Green numbers
    265567/293024 - UI Red numbers
    265568 - UI Yellow numbers (damage)
    265569 - UI Pink numbers
    265573 - UI Floppy disk background without corner
    265577 - UI Floppy disk background with corner
    265575 - UI Swords clashing icon
    265576/277151 - UI Green chat bubble with "!"/ Grey chat bubble with "?" / Checkmark
    265579 - UI Save/load skill builds icons (floppy disk etc...)
    265587 - UI Weapon icons with round background Enabled/Disabled
    277144 - UI Arrows u/d
    277149 - UI Roman numerals (Storage panes)
    277152 - UI Expand/Collapse equipped skills, multiple states
    277156 - UI Skills sort and display options icons
    293019 - UI Cancel/Disable buttons
    293020-1 - UI Windows with arrows
    293022 - UI Buttons with small arrow bottom right corner
    293024 - UI u/d arrows and "{" "}" brackets
    293027 - UI HM/NM selection icons
    294240 - UI Big "{"
    294789 - UI Horizontal bar
    301079 - UI Party management icons
    302777 - UI Mission map red key icon
    302779 - UI Mission map boss/skull icon
    302781 - UI Mission map area map icon
    302783 - UI Mission map stairs up icon
    302784 - UI Mission map stairs down icon
    329741 - UI "Play" button/Gears button/Checkmark/"+" button
    329767 - UI Banners with x button
    329770 - UI Arrows u/d buttons
    329771 - UI Scroll bar?
    329772 - UI Box
    329774 - UI Skill point star icon
    332770 - UI Corner handle?
    332772 - UI Banners with settings icon
    332780 - UI PvP flags icons
    337391 - UI Dollar sign
    381174 - Heroic Refrain icon (last item it seems)

    */

    namespace KnownFileIDs // AI generated, beware
    {
        constexpr uint32_t MAX = 381175;

        constexpr uint32_t UI_SkillHoverOverlay = 9087;
        constexpr uint32_t UI_SkillEquipTip = 24254;
        constexpr uint32_t UI_GreenLiquidDripping = 8620;
        constexpr uint32_t UI_FramesBorders = 61201;
        constexpr uint32_t UI_PvPFlagsIcons = 332780;
        constexpr uint32_t UI_BannersSettingsIcon = 332772;
        constexpr uint32_t UI_CornerHandle = 332770;
        constexpr uint32_t UI_SkillPointStarIcon = 329774;
        constexpr uint32_t UI_Box = 329772;
        constexpr uint32_t UI_ScrollBar = 329771;
        constexpr uint32_t UI_ArrowsUDButtons = 329770;
        constexpr uint32_t UI_BannersXButton = 329767;
        constexpr uint32_t UI_PlusButton = 329744;
        constexpr uint32_t UI_CheckmarkButton = 329743;
        constexpr uint32_t UI_GearsButton = 329742;
        constexpr uint32_t UI_PlayButton = 329741;
        constexpr uint32_t UI_MissionMapStairsDownIcon = 302784;
        constexpr uint32_t UI_MissionMapStairsUpIcon = 302783;
        constexpr uint32_t UI_MissionMapAreaMapIcon = 302781;
        constexpr uint32_t UI_MissionMapBossSkullIcon = 302779;
        constexpr uint32_t UI_MissionMapRedKeyIcon = 302777;
        constexpr uint32_t UI_PartyManagementIcons = 301079;
        constexpr uint32_t UI_HorizontalBar = 294789;
        constexpr uint32_t UI_BigCurlyBracket = 294240;
        constexpr uint32_t UI_HMNMSelectionIcons = 293027;
        constexpr uint32_t UI_ArrowsBrackets = 293024;
        constexpr uint32_t UI_ButtonsSmallArrowCorner = 293022;
        constexpr uint32_t UI_WindowsWithArrows2 = 293021;
        constexpr uint32_t UI_WindowsWithArrows1 = 293020;
        constexpr uint32_t UI_CancelDisableButtons = 293019;
        constexpr uint32_t UI_SkillsSortDisplayOptionsIcons = 277156;
        constexpr uint32_t UI_ExpandCollapseEquippedSkills = 277152;
        constexpr uint32_t UI_RomanNumeralsStoragePanes = 277149;
        constexpr uint32_t UI_ArrowsUD = 277144;
        constexpr uint32_t UI_WeaponIconsRoundBgDisabled = 265588;
        constexpr uint32_t UI_WeaponIconsRoundBgEnabled = 265587;
        constexpr uint32_t UI_SaveLoadSkillBuildsIcons = 265579;
        constexpr uint32_t UI_ChatBubbleIcons2 = 277151;
        constexpr uint32_t UI_ChatBubbleIcons1 = 265576;
        constexpr uint32_t UI_SwordsClashingIcon = 265575;
        constexpr uint32_t UI_FloppyDiskBgWithCorner = 265577;
        constexpr uint32_t UI_FloppyDiskBgNoCorner = 265573;
        constexpr uint32_t UI_PinkNumbers = 265569;
        constexpr uint32_t UI_YellowNumbers = 265568;
        constexpr uint32_t UI_RedNumbers1 = 265567;
        constexpr uint32_t UI_RedNumbers2 = 293024;
        constexpr uint32_t UI_GreenNumbers = 265566;
        constexpr uint32_t UI_BlueNumbers = 265565;
        constexpr uint32_t UI_HeroLockTargetIcons = 265564;
        constexpr uint32_t UI_HeroBehaviourIcons = 265563;
        constexpr uint32_t UI_SkillSlotSelected = 265562;
        constexpr uint32_t UI_SkillSlotEmpty = 265559;
        constexpr uint32_t UI_SkillEffectBorders2 = 368769;
        constexpr uint32_t UI_SkillEffectBorders1 = 265556;
        constexpr uint32_t UI_SkillHoverBg = 265549;
        constexpr uint32_t UI_CursorSmall = 265547;
        constexpr uint32_t UI_CursorFlagHero3 = 265542;
        constexpr uint32_t UI_CursorFlagHero2 = 265540;
        constexpr uint32_t UI_CursorFlagHero1 = 265538;
        constexpr uint32_t UI_CursorFlagAllHeroes = 265536;
        constexpr uint32_t UI_CursorSalvageInspect = 265534;
        constexpr uint32_t UI_SkillbarSlotHourglass2 = 265561;
        constexpr uint32_t UI_SkillbarSlotHourglass1 = 257172;
        constexpr uint32_t UI_MapHeroFlagIcons = 256682;
        constexpr uint32_t UI_CompassHeroButtonsStates = 256681;
        constexpr uint32_t UI_CompassHeroPanelStart = 256680;
        constexpr uint32_t UI_CompassThingy = 256553;
        constexpr uint32_t UI_BarBorder = 205437;
        constexpr uint32_t UI_EmptyBar = 205436;
        constexpr uint32_t UI_BleedingBar2 = 205435;
        constexpr uint32_t UI_PoisonedBar = 205434;
        constexpr uint32_t UI_OvercastBar = 205433;
        constexpr uint32_t UI_PurpleBar = 205432;
        constexpr uint32_t UI_HealthBar = 205431;
        constexpr uint32_t UI_ExperienceBar = 205430;
        constexpr uint32_t UI_EnergyBar = 205429;
        constexpr uint32_t UI_GreyBar = 205428;
        constexpr uint32_t UI_HexedBar = 205427;
        constexpr uint32_t UI_BleedingBar1 = 205426;
        constexpr uint32_t UI_SkillHoverBgGold = 205425;
        constexpr uint32_t UI_SkillHoverBgWhite = 205424;
        constexpr uint32_t UI_SkillHoverBgPurple = 205423;
        constexpr uint32_t UI_CastBars4 = 205422;
        constexpr uint32_t UI_CastBars3 = 205421;
        constexpr uint32_t UI_CastBars2 = 205420;
        constexpr uint32_t UI_CastBars1 = 205419;
        constexpr uint32_t UI_SmallMissionMapIcons37 = 191492;
        constexpr uint32_t UI_SmallMissionMapIcons36 = 191491;
        constexpr uint32_t UI_SmallMissionMapIcons35 = 191490;
        constexpr uint32_t UI_SmallMissionMapIcons34 = 191489;
        constexpr uint32_t UI_SmallMissionMapIcons33 = 191488;
        constexpr uint32_t UI_SmallMissionMapIcons32 = 191487;
        constexpr uint32_t UI_SmallMissionMapIcons31 = 191486;
        constexpr uint32_t UI_SmallMissionMapIcons30 = 191485;
        constexpr uint32_t UI_SmallMissionMapIcons29 = 191484;
        constexpr uint32_t UI_SmallMissionMapIcons28 = 191483;
        constexpr uint32_t UI_SmallMissionMapIcons27 = 191482;
        constexpr uint32_t UI_SmallMissionMapIcons26 = 191481;
        constexpr uint32_t UI_SmallMissionMapIcons25 = 191480;
        constexpr uint32_t UI_SmallMissionMapIcons24 = 191479;
        constexpr uint32_t UI_SmallMissionMapIcons23 = 191478;
        constexpr uint32_t UI_SmallMissionMapIcons22 = 191477;
        constexpr uint32_t UI_SmallMissionMapIcons21 = 191476;
        constexpr uint32_t UI_SmallMissionMapIcons20 = 191475;
        constexpr uint32_t UI_SmallMissionMapIcons19 = 191474;
        constexpr uint32_t UI_SmallMissionMapIcons18 = 191473;
        constexpr uint32_t UI_SmallMissionMapIcons17 = 191472;
        constexpr uint32_t UI_SmallMissionMapIcons16 = 191471;
        constexpr uint32_t UI_SmallMissionMapIcons15 = 191470;
        constexpr uint32_t UI_SmallMissionMapIcons14 = 191469;
        constexpr uint32_t UI_SmallMissionMapIcons13 = 191468;
        constexpr uint32_t UI_SmallMissionMapIcons12 = 191467;
        constexpr uint32_t UI_SmallMissionMapIcons11 = 191466;
        constexpr uint32_t UI_SmallMissionMapIcons10 = 191465;
        constexpr uint32_t UI_SmallMissionMapIcons9 = 191464;
        constexpr uint32_t UI_SmallMissionMapIcons8 = 191463;
        constexpr uint32_t UI_SmallMissionMapIcons7 = 191462;
        constexpr uint32_t UI_SmallMissionMapIcons6 = 191461;
        constexpr uint32_t UI_SmallMissionMapIcons5 = 191460;
        constexpr uint32_t UI_SmallMissionMapIcons4 = 191459;
        constexpr uint32_t UI_SmallMissionMapIcons3 = 191458;
        constexpr uint32_t UI_SmallMissionMapIcons2 = 191457;
        constexpr uint32_t UI_SmallMissionMapIcons1 = 191456;
        constexpr uint32_t UI_CityMapIcon = 175151;
        constexpr uint32_t UI_OutpostMapIcon = 175149;
        constexpr uint32_t UI_LeadOffhandDualIcons = 164481;
        constexpr uint32_t UI_SkillMissingReqLeadOffhandEncDualHexIcons = 164477;
        constexpr uint32_t UI_SkillLeadOffhandEncDualHexWepSpIcons = 164476;
        constexpr uint32_t UI_Stuff40 = 158487;
        constexpr uint32_t UI_Stuff39 = 158486;
        constexpr uint32_t UI_Stuff38 = 158485;
        constexpr uint32_t UI_Stuff37 = 158484;
        constexpr uint32_t UI_Stuff36 = 158270;
        constexpr uint32_t UI_Stuff35 = 158269;
        constexpr uint32_t UI_SkillWeaponReqNotSatisfied = 158162;
        constexpr uint32_t UI_SkillWeaponReq = 158160;
        constexpr uint32_t UI_Stuff34 = 153567;
        constexpr uint32_t UI_Stuff33 = 153566;
        constexpr uint32_t UI_Stuff32 = 153565;
        constexpr uint32_t UI_Stuff31 = 153564;
        constexpr uint32_t UI_Stuff30 = 153563;
        constexpr uint32_t UI_Stuff29 = 153562;
        constexpr uint32_t UI_Stuff28 = 153561;
        constexpr uint32_t UI_Stuff27 = 153560;
        constexpr uint32_t UI_Stuff26 = 153559;
        constexpr uint32_t UI_Stuff25 = 153558;
        constexpr uint32_t UI_Stuff24 = 153557;
        constexpr uint32_t UI_Stuff23 = 153556;
        constexpr uint32_t UI_Stuff22 = 153555;
        constexpr uint32_t UI_Stuff21 = 153554;
        constexpr uint32_t UI_Stuff20 = 153553;
        constexpr uint32_t UI_Stuff19 = 153552;
        constexpr uint32_t UI_Stuff18 = 153551;
        constexpr uint32_t UI_Stuff17 = 153550;
        constexpr uint32_t UI_Stuff16 = 153549;
        constexpr uint32_t UI_Stuff15 = 153548;
        constexpr uint32_t UI_Stuff14 = 153547;
        constexpr uint32_t UI_Stuff13 = 153546;
        constexpr uint32_t UI_Stuff12 = 153545;
        constexpr uint32_t UI_Stuff11 = 153544;
        constexpr uint32_t UI_Stuff10 = 153543;
        constexpr uint32_t UI_Stuff9 = 153542;
        constexpr uint32_t UI_Stuff8 = 153541;
        constexpr uint32_t UI_Stuff7 = 153540;
        constexpr uint32_t UI_Stuff6 = 153539;
        constexpr uint32_t UI_Stuff5 = 153538;
        constexpr uint32_t UI_Stuff4 = 153537;
        constexpr uint32_t UI_Stuff3 = 153536;
        constexpr uint32_t UI_Stuff2 = 153535;
        constexpr uint32_t UI_Stuff1 = 153534;
        constexpr uint32_t UI_RoundGreenSelector = 152641;
        constexpr uint32_t UI_ProfessionIconsDisabled = 152639;
        constexpr uint32_t UI_ProfessionIcons = 152638;
        constexpr uint32_t UI_WeaponIconsDisabled = 152636;
        constexpr uint32_t UI_WeaponIcons = 152635;
        constexpr uint32_t UI_RedBackgroundXButtons2 = 151820;
        constexpr uint32_t UI_RedBackgroundXButtons1 = 151819;
        constexpr uint32_t UI_BlueBackgroundXButtons2 = 151818;
        constexpr uint32_t UI_BlueBackgroundXButtons1 = 151817;
        constexpr uint32_t UI_MissionMapCrosshair = 151391;
        constexpr uint32_t UI_InventoryBagIcons = 151387;
        constexpr uint32_t UI_FriendsListFriendOnlineOfflineIcon = 151338;
        constexpr uint32_t UI_CannotSalvageIcon = 151300;
        constexpr uint32_t UI_ArrowsUDMultipleStates = 151271;
        constexpr uint32_t UI_WeaponSetBackground = 144758;
        constexpr uint32_t UI_WorldspaceChatMessageBackground = 144538;
        constexpr uint32_t UI_SmallSymbols = 144537;
        constexpr uint32_t UI_ProfessionIconAtlas = 144536;
        constexpr uint32_t UI_CheckboxesStates = 144533;
        constexpr uint32_t UI_SkillBorder = 144524;
        constexpr uint32_t UI_MenuButtons2 = 294790;
        constexpr uint32_t UI_MenuButtons1 = 143781;
        constexpr uint32_t UI_OpenMenuButton = 143780;
        constexpr uint32_t UI_MenuBackground = 143778;
        constexpr uint32_t UI_ButtonsCloseMakeWindowMinimize = 143762;
        constexpr uint32_t UI_SkillBoundKeyLarge = 143032;
        constexpr uint32_t UI_SkillBoundKey = 143031;
        constexpr uint32_t UI_SmallMaterialIconAtlas = 143027;
        constexpr uint32_t UI_Thing8 = 143024;
        constexpr uint32_t UI_Thing7 = 143023;
        constexpr uint32_t UI_Thing6 = 143022;
        constexpr uint32_t UI_Thing5 = 143021;
        constexpr uint32_t UI_CompassLens = 143017;
        constexpr uint32_t UI_CompassBorderAggroRange = 143016;
        constexpr uint32_t UI_CompassDirections = 143015;
        constexpr uint32_t UI_ChatOpenCloseButton = 143014;
        constexpr uint32_t UI_SkillHoverWindowBorder = 143005;
        constexpr uint32_t UI_EffectSymbol = 143004;
        constexpr uint32_t UI_InventoryEquipmentBackground = 134757;
        constexpr uint32_t UI_InventoryBagHeader = 134732;
        constexpr uint32_t UI_WindowHeader = 134731;
        constexpr uint32_t UI_Checkbox = 134730;
        constexpr uint32_t UI_Thing4 = 134729;
        constexpr uint32_t UI_HorizontalScrollHandle = 134728;
        constexpr uint32_t UI_VerticalScrollHandle = 134727;
        constexpr uint32_t UI_HorizontalMid = 134726;
        constexpr uint32_t UI_VerticalMid = 134725;
        constexpr uint32_t UI_HorizontalScrollbarCaps = 134724;
        constexpr uint32_t UI_VerticalScrollbarCaps = 134723;
        constexpr uint32_t UI_Tab2 = 134722;
        constexpr uint32_t UI_Tab1 = 134721;
        constexpr uint32_t UI_Thing3 = 134720;
        constexpr uint32_t UI_Thing2 = 134717;
        constexpr uint32_t UI_Thing1 = 134716;
        constexpr uint32_t UI_LightBlueDropdown = 134715;
        constexpr uint32_t UI_Border = 134714;
        constexpr uint32_t UI_BlueDropdown = 134713;
        constexpr uint32_t UI_WindowBorders3 = 134712;
        constexpr uint32_t UI_WindowBorders2 = 134711;
        constexpr uint32_t UI_WindowBorders1 = 134710;
        constexpr uint32_t UI_BigSaturatedBlueButton = 134709;
        constexpr uint32_t UI_BigBlueButton = 134708;
        constexpr uint32_t UI_BigDisabledButton = 134707;
        constexpr uint32_t UI_SaturatedBlueButton = 134705;
        constexpr uint32_t UI_BlueButton = 134704;
        constexpr uint32_t UI_DisabledButton = 134703;
        constexpr uint32_t UI_GreenButton = 134702;
        constexpr uint32_t UI_CheckboxesMultipleStates = 134699;
        constexpr uint32_t UI_IndentationMoney = 134698;
        constexpr uint32_t UI_Indentation2 = 134697;
        constexpr uint32_t UI_Indentation1 = 134696;
        constexpr uint32_t UI_SkillLockedOverlay = 122685;
        constexpr uint32_t UI_Square4 = 151236;
        constexpr uint32_t UI_Square3 = 122683;
        constexpr uint32_t UI_Square2 = 151235;
        constexpr uint32_t UI_Square1 = 122681;
        constexpr uint32_t UI_DarkSquare2 = 151234;
        constexpr uint32_t UI_DarkSquare1 = 122679;
        constexpr uint32_t UI_InventoryBinIcon = 122676;
        constexpr uint32_t UI_PlatinumGoldIcons2 = 143010;
        constexpr uint32_t UI_PlatinumGoldIcons1 = 122674;
        constexpr uint32_t UI_QuestMarkers = 111829;
        constexpr uint32_t UI_LargeBox2 = 112323;
        constexpr uint32_t UI_LargeBox1 = 111805;
        constexpr uint32_t UI_SmallBox = 111804;
        constexpr uint32_t UI_Checkboxes = 111795;
        constexpr uint32_t UI_Buttons = 111776;
        constexpr uint32_t UI_ItemOverlays2 = 143009;
        constexpr uint32_t UI_ItemOverlays1 = 108154;
        constexpr uint32_t UI_ItemRarityBorder2 = 143008;
        constexpr uint32_t UI_ItemRarityBorder1 = 108152;
        constexpr uint32_t UI_SkillStatsIcons = 9131;
        constexpr uint32_t UI_BlueGuyPartyCount = 9125;
        constexpr uint32_t UI_LightningSymbolPing2 = 277146;
        constexpr uint32_t UI_LightningSymbolPing = 8976;
        constexpr uint32_t UI_CursorResizeWindowBackslash = 134694;
        constexpr uint32_t UI_CursorResizeWindowSlash = 134692;
        constexpr uint32_t UI_CursorResizeWindowUD = 134690;
        constexpr uint32_t UI_CursorResizeWindowLR = 134688;
        constexpr uint32_t UI_CursorTyping = 134686;
        constexpr uint32_t UI_CursorHandPicking = 134684;
        constexpr uint32_t UI_CursorHandPointing = 134682;
        constexpr uint32_t UI_CursorTrident = 122693;
        constexpr uint32_t UI_CursorSalvageHammer = 111825;
        constexpr uint32_t UI_CursorResizeWindow = 107948;
        constexpr uint32_t UI_CursorMini = 9085;
        constexpr uint32_t UI_CursorOrangeOutline = 9077;
        constexpr uint32_t UI_CursorGreenOutline = 9075;
        constexpr uint32_t UI_Cursor = 8957;
        constexpr uint32_t UI_StackSizeNumbers = 107278;
        constexpr uint32_t UI_SkillBordersOverlays2 = 153580;
        constexpr uint32_t UI_SkillBordersOverlays1 = 104946;
        constexpr uint32_t UI_WorldspaceTargetMarker = 104587;
        constexpr uint32_t UI_WorldspaceHealthBar = 95099;
        constexpr uint32_t UI_CalledTargetIcons2 = 151243;
        constexpr uint32_t UI_CalledTargetIcons1 = 93156;
    }
}