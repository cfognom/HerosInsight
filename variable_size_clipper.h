#pragma once

#include <functional>
#include <update_manager.h>

namespace HerosInsight
{
    // Must not be stack allocated, contains state
    struct VariableSizeClipper
    {
        std::vector<uint32_t> item_end_positions; // TODO: Instead of storing float offsets it will prob be better to store uint16 sizes and keep a record of last scroll position + last item index
        float current_scroll = 0;
        float target_scroll = 0;
        int32_t target_scroll_index = 0;
        int32_t final_target_snap_index = 0;
        uint32_t visible_index_start = 0;
        bool snap_to_items = false;
        ImGuiWindow *window = nullptr;

        void Reset()
        {
            item_end_positions.clear();
            ResetScroll();
        }

        void ResetScroll()
        {
            if (!window)
                return;

            current_scroll = 0;
            target_scroll = 0;
            target_scroll_index = 0;
            ImGui::SetScrollY(window, 0);
        }

        void SetScrollToIndex(uint32_t index)
        {
            if (!window)
                return;

            if (index >= item_end_positions.size())
                return;

            ImGui::SetScrollY(window, get_start_pos(index));
        }

        void Draw(size_t size, float est_avg_height, bool snap_to_items, std::function<void(uint32_t)> draw_item)
        {
            this->window = ImGui::GetCurrentWindow();
            this->window->Flags |= ImGuiWindowFlags_NoScrollWithMouse;
            this->resize(size, est_avg_height);
            bool scroll_modified = false;
            bool started_snapping = snap_to_items && !this->snap_to_items;
            scroll_modified |= started_snapping;
            this->snap_to_items = snap_to_items;

            const auto view_start = ImGui::GetScrollY();
            const auto view_height = ImGui::GetWindowHeight();
            const auto view_end = view_start + view_height;

            const auto diff = view_start - current_scroll;
            if (std::abs(diff) > 1.f)
            {
                // Current scroll is not what we set it to last frame, it was modified elsewhere
                // Set it as the new target
                current_scroll = view_start;
                target_scroll = view_start;
                if (snap_to_items)
                    target_scroll_index = get_index_containing_pos(view_start, false);
                scroll_modified = true;
            }

            if (started_snapping)
            {
                target_scroll_index = get_index_containing_pos(target_scroll, false);
            }

            auto DrawItem = [&](uint32_t index)
            {
                ImGui::SetCursorPosY(get_start_pos(index));
                ImGui::BeginGroup();
                draw_item(index);
                ImGui::EndGroup();
                auto after = ImGui::GetCursorPosY();
                update_end_pos(index, after);
            };

            visible_index_start = get_index_containing_pos(current_scroll, false);
            auto draw_index = visible_index_start;
            const auto draw_cursor_start = get_start_pos(draw_index);
            auto draw_cursor = draw_cursor_start;
            ImGui::SetCursorPosY(draw_cursor);

            while (draw_index < size && draw_cursor < view_end)
            {
                DrawItem(draw_index);
                draw_cursor = ImGui::GetCursorPosY();
                draw_index++;
            }
            const auto draw_cursor_end = draw_cursor;
            const auto visible_index_end = draw_index;

            float wheel_input = get_wheel_input();

            // Handle mouse wheel input or other scroll modifications
            if (wheel_input != 0.f)
            {
                if (snap_to_items)
                {
                    const auto wheel_ticks = (int32_t)std::round(wheel_input);
                    target_scroll_index = target_scroll_index + wheel_ticks;
                    target_scroll_index = std::max(target_scroll_index, 0);
                }
                else
                {
                    target_scroll += wheel_input * 100.f;
                    target_scroll = std::max(target_scroll, 0.0f);
                }
            }

            if (wheel_input != 0.f || scroll_modified || current_scroll != target_scroll)
            {
                // Use clip rect to hide the following items, they are only drawn to get their sizes
                ImGui::PushClipRect(ImVec2(0, 0), ImVec2(0, 0), false);
                {
                    // Draw items below the visible range to get an accurate position of target_scroll
                    draw_index = visible_index_end;
                    while (draw_index < size && (get_start_pos(draw_index) < target_scroll || (snap_to_items && draw_index < target_scroll_index)))
                    {
                        DrawItem(draw_index);
                        draw_index++;
                    }

                    // Draw items above the visible range to get an accurate position of target_scroll
                    draw_index = visible_index_start;
                    while (draw_index > 0 && (target_scroll < get_start_pos(draw_index) || (snap_to_items && target_scroll_index < draw_index)))
                    {
                        draw_index--;
                        DrawItem(draw_index);
                    }

                    // Draw items at end to get an accurate position of scroll_max
                    draw_index = size;
                    while (draw_index > 0 && get_start_pos(size) - get_start_pos(draw_index) < view_height)
                    {
                        draw_index--;
                        DrawItem(draw_index);
                    }
                }
                ImGui::PopClipRect();
                const auto scroll_max = std::max(get_start_pos(size) - view_height, 0.f);

                if (snap_to_items)
                {
                    // Clamp target_scroll_index to valid range
                    final_target_snap_index = draw_index;
                    const auto end_target_snap_index = final_target_snap_index + 1;

                    if ((scroll_modified && target_scroll == scroll_max && target_scroll != get_start_pos(target_scroll_index)) ||
                        (target_scroll_index > end_target_snap_index))
                    {
                        target_scroll_index = end_target_snap_index;
                    }

                    // Update target_scroll based on the target snap index
                    target_scroll = get_start_pos(target_scroll_index);
                }

                target_scroll = std::min(target_scroll, scroll_max);

                // Do smooth scrolling towards target scroll
                const auto dt = ImGui::GetIO().DeltaTime;
                current_scroll = smooth_scroll(current_scroll, target_scroll, dt);
            }

            set_cursor_to_end();

            ImGui::SetScrollY(std::round(current_scroll));
            // auto window = ImGui::GetCurrentWindow();
            // window->Scroll.y = current_scroll;

#ifdef _DEBUG
            display_wheel_input(wheel_input);
#endif
        }

    private:
#ifdef _DEBUG
        int32_t wheel_display = 0;
        DWORD wheel_display_timestamp = 0;

        void display_wheel_input(float wheel_input)
        {
            auto window = ImGui::GetCurrentWindow();
            const auto timestamp = GetTickCount();
            if (wheel_input != 0.f)
            {
                wheel_display = wheel_input;
                wheel_display_timestamp = timestamp;
            }

            if (wheel_display != 0 && (timestamp - wheel_display_timestamp) < 100)
            {
                auto draw_list = ImGui::GetWindowDrawList();
                auto window_pos = ImGui::GetWindowPos();
                auto window_size = ImGui::GetWindowSize();
                auto str = std::to_string(wheel_display);
                auto text_size = ImGui::CalcTextSize(str.c_str());
                auto text_pos = ImVec2(window_pos.x + window_size.x / 2 - text_size.x / 2, window_pos.y + window_size.y / 2 - text_size.y / 2);
                ImGui::PushFont(Constants::Fonts::skill_thick_font_18);
                draw_list->AddText(text_pos, IM_COL32(0, 255, 0, 255), str.c_str());
                ImGui::PopFont();
            }
        }
#endif

        static float smooth_scroll(float current_scroll, float target_scroll, float dt)
        {
#ifdef _DEBUG
            // dt *= 0.1f; // Slow down the smooth scrolling for debugging purposes, so we can see the scroll position change
#endif
            const bool positive_sign = target_scroll > current_scroll;
            const auto sign = positive_sign ? 1.f : -1.f;
            const auto remaining = positive_sign ? target_scroll - current_scroll : current_scroll - target_scroll;

            float c = std::powf(2e-4, dt);

            auto new_remaining = sign * std::max(c * remaining - 100.f * dt, 0.f);

            auto new_scroll = target_scroll - new_remaining;
#ifdef _DEBUG
            SOFT_ASSERT(new_scroll == target_scroll || std::signbit(target_scroll - current_scroll) == std::signbit(target_scroll - new_scroll));
#endif
            return new_scroll;
        }

        uint32_t size()
        {
            return item_end_positions.size();
        }

        void set_cursor_to_end()
        {
            ImGui::SetCursorPosY(get_start_pos(size()));
        }

        uint32_t get_index_containing_pos(float pos, bool include_end = false)
        {
            // Perform binary search to find the current index based on scroll position
            auto it = std::lower_bound(item_end_positions.begin(), item_end_positions.end(), pos);
            if (it == item_end_positions.end())
                return item_end_positions.size();
            auto index = std::distance(item_end_positions.begin(), it);
            if (!include_end && *it == pos) // if the position is exactly at the end of an item, we treat it as the next item
                index++;
            return index;
        }

        void resize(size_t size, float est_avg_height)
        {
            uint32_t old_size = item_end_positions.size();

            if (old_size < size)
            {
                item_end_positions.reserve(size);
                float last_end = get_start_pos(old_size);
                for (uint32_t i = old_size; i < size; i++)
                {
                    last_end += est_avg_height;
                    item_end_positions.push_back(last_end);
                }
            }
            else if (old_size > size)
            {
                item_end_positions.resize(size);
            }
        }

        float get_wheel_input()
        {
            if (ImGui::IsWindowHovered())
            {
                const auto &io = ImGui::GetIO();
                return -io.MouseWheel; // Negate so positive is down.
            }
            return 0.f;
        }

        float get_start_pos(uint32_t index)
        {
            return index > 0 && index <= item_end_positions.size() ? item_end_positions[index - 1] : 0;
        }

        float get_end_pos(uint32_t index)
        {
            return item_end_positions[index];
        }

        float get_size(uint32_t index)
        {
            return get_end_pos(index) - get_start_pos(index);
        }

        void update_end_pos(uint32_t index, float new_end)
        {
            float old_end = item_end_positions[index];
            // if (old_end == new_end)
            if (std::abs(old_end - new_end) <= 1.f) // For some reason the same content has different sizes by 1 pixel
                return;

            float start_pos = get_start_pos(index);

            // auto old_size = get_size(index);
            item_end_positions[index] = new_end;
            // auto new_size = get_size(index);
            // Utils::FormatToChat(L"item {} size changed from {} to {}", index, old_size, new_size);

            float diff = new_end - old_end;
            // #ifdef _TIMING
            //             auto start_timestamp = std::chrono::high_resolution_clock::now();
            // #endif
            if (old_end <= current_scroll)
            {
                current_scroll += diff;
                target_scroll += diff;
            }

            for (uint32_t i = index + 1; i < item_end_positions.size(); i++)
            {
                item_end_positions[i] += diff;
            }
            // #ifdef _TIMING
            //             auto timestamp_end = std::chrono::high_resolution_clock::now();
            //             auto duration = std::chrono::duration_cast<std::chrono::microseconds>(timestamp_end - start_timestamp).count();
            //             Utils::FormatToChat(L"updating clipper ends took {} micro s", duration);
            // #endif
        }
    };
}