#pragma once

#include <functional>
#include <update_manager.h>

// #define PRINT_VARIABLE_SIZE_CLIPPER_TIMING

namespace HerosInsight
{
    // Must not be stack allocated, contains state
    struct VariableSizeClipper
    {
        struct Position
        {
            uint32_t entry_index;
            float pixel_offset;

            bool operator==(const Position &other) const
            {
                return entry_index == other.entry_index &&
                       pixel_offset == other.pixel_offset;
            }

            bool operator<(const Position &other) const
            {
                return entry_index < other.entry_index ||
                       (entry_index == other.entry_index && pixel_offset < other.pixel_offset);
            }

            static Position Min(Position a, Position b)
            {
                return a < b ? a : b;
            }

            static Position Max(Position a, Position b)
            {
                return a < b ? b : a;
            }
        };

        VariableSizeClipper() {}

        void Reset()
        {
            item_sizes.clear();
            ResetScroll();
        }

        void ResetScroll()
        {
            if (!window)
                return;

            scroll_current = scroll_target = scroll_max = {0, 0};
            ImGui::SetScrollY(window, 0);
        }

        Position GetCurrentScroll() const
        {
            return scroll_current;
        }

        void Draw(size_t size, float est_avg_height, bool snap_to_items, std::function<void(uint32_t)> draw_item)
        {
#ifdef PRINT_VARIABLE_SIZE_CLIPPER_TIMING
            auto start = std::chrono::high_resolution_clock::now();
#endif
            this->window = ImGui::GetCurrentWindow();
            this->window->Flags |= ImGuiWindowFlags_NoScrollWithMouse;
            this->item_sizes.resize(size, est_avg_height);
            this->draw_item = draw_item;

            this->scroll_max = CalcScrollMax();

            auto imgui_scroll = ImGui::GetScrollY();
            auto view_height = GetViewHeight();
            auto view_end = imgui_scroll + view_height;

            bool is_pressing_scrollbar = this->IsPressingScrollbar();
            if (is_pressing_scrollbar)
            {
                // When pressing scrollbar:
                // - Mouse pointer drives imgui_scroll
                // - imgui_scroll drives scroll_current + scroll_target

                auto jumped = WalkForwards(Position{0, 0}, imgui_scroll, IndexRange::All());
                bool at_end = !(jumped < this->scroll_max);
                jumped = at_end ? this->scroll_max : jumped;
                this->scroll_current = jumped;
                this->scroll_target = jumped;
                if (snap_to_items && !at_end)
                {
                    this->scroll_target.pixel_offset = 0;
                }
            }

            float cursor = imgui_scroll - this->scroll_current.pixel_offset;
            size_t i = this->scroll_current.entry_index;
            ImGui::SetCursorPosY(cursor);
            for (; i < item_sizes.size() && cursor < view_end; ++i)
            {
                DrawAndMeasureEntry(i);
                cursor += item_sizes[i];
            }
#ifdef _DEBUG
            // Just a check to make sure our recorded sizes matches with how much imgui advanced the cursor
            SOFT_ASSERT(cursor == ImGui::GetCursorPosY(), L"cursor != ImGui::GetCursorPosY() ({} != {})", cursor, ImGui::GetCursorPosY());
#endif
            auto trailing_height = CalcDistance(Position{i, 0}, Position{item_sizes.size(), 0}, IndexRange::All()); // We skip measuring for performance
            cursor += trailing_height;
            ImGui::SetCursorPosY(cursor); // To trick ImGui into thinking we used all that space

            if (!is_pressing_scrollbar)
            {
                // When not pressing scrollbar:
                // - Mouse wheel drives scroll_target
                // - scroll_target drives scroll_current
                // - scroll_current drives imgui_scroll

                auto measured_range = IndexRange::None(); // We keep track of which entries were measured this frame to avoid measuring them twice
                UpdateTargetWithInput(snap_to_items, measured_range);

                if (scroll_current != scroll_target)
                {
                    // Update sizes between scroll_current and scroll_target and calculate accurate distance
                    auto distance_to_target = CalcDistance(scroll_current, scroll_target, measured_range);

                    // Calculate scroll_delta
                    const auto dt = ImGui::GetIO().DeltaTime;
                    auto scroll_delta = SmoothScroll(0, distance_to_target, dt);

                    // Apply scroll_delta to scroll_current
                    // We can skip measuring since we already did that when we calculated distance_to_target
                    if (scroll_delta >= 0)
                    {
                        this->scroll_current = WalkForwards(this->scroll_current, scroll_delta, IndexRange::All());
                    }
                    else
                    {
                        this->scroll_current = WalkBackwards(this->scroll_current, -scroll_delta, IndexRange::All());
                    }

                    // Apply scroll_current to imgui_scroll
                    auto new_imgui_scroll = CalcDistance(Position{0, 0}, this->scroll_current, IndexRange::All()); // We skip measuring for performance
                    ImGui::SetScrollY(new_imgui_scroll);
                }
            }

#ifdef _DEBUG
            DrawDebugInfo();
#endif
#ifdef PRINT_VARIABLE_SIZE_CLIPPER_TIMING
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Utils::FormatToChat(0xFFFFFFFF, L"VariableSizeClipper Draw took {} ms", duration);
#endif
        }

    private:
        ImGuiWindow *window = nullptr;
        std::vector<uint16_t> item_sizes;
        Position scroll_current = {0, 0};
        Position scroll_target = {0, 0};
        Position scroll_max = {0, 0};
        std::function<void(uint32_t)> draw_item;

        struct IndexRange
        {
            uint32_t start;
            uint32_t end;

            bool Contains(uint32_t index) const { return start <= index && index < end; }

            static constexpr IndexRange All() { return {0, std::numeric_limits<uint32_t>::max()}; }
            static constexpr IndexRange None() { return {0, 0}; }
        };

#ifdef _DEBUG
        int32_t wheel_display = 0;
        DWORD wheel_display_timestamp = 0;

        void DisplayWheelInput()
        {
            auto window = ImGui::GetCurrentWindow();
            const auto timestamp = GetTickCount();
            auto wheel_input = GetWheelInput();
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

        void DrawDebugInfo()
        {
            DisplayWheelInput();
            ImGui::SetCursorPosY(ImGui::GetScrollY());
            ImGui::Text("target_scroll_index: %d, target_scroll_offset: %f", this->scroll_target.entry_index, this->scroll_target.pixel_offset);
        }
#endif

        bool IsPressingScrollbar()
        {
            auto scroll_bar_id = ImGui::GetWindowScrollbarID(this->window, ImGuiAxis_Y);
            return ImGui::GetCurrentContext()->ActiveId == scroll_bar_id;
        }

        void DrawEntry(uint32_t index)
        {
            ImGui::BeginGroup();
            draw_item(index);
            ImGui::EndGroup();
        };

        void DrawAndMeasureEntry(uint32_t index)
        {
            auto cursor = ImGui::GetCursorPosY();
            DrawEntry(index);
            auto size = ImGui::GetCursorPosY() - cursor;
#ifdef _DEBUG
            assert(std::floor(size) == size);
#endif
            this->item_sizes[index] = size;
        }

        void MeasureEntry(uint32_t index)
        {
            auto current_cursor = ImGui::GetCursorPosY();
            auto scroll_y = ImGui::GetScrollY();
            ImGui::SetCursorPosY(scroll_y); // If we dont do this, it grows weirdly each frame
            DrawEntry(index);
            auto size = ImGui::GetCursorPosY() - scroll_y;
            auto &old_size = this->item_sizes[index];
#ifdef _DEBUG
            assert(std::floor(size) == size);
            // if (old_size != size)
            // {
            //     Utils::FormatToChat(0xFFFFFFFF, L"Index {} size changed from {} to {}", index, old_size, size);
            // }
#endif
            old_size = size;
            ImGui::SetCursorPosY(current_cursor); // Restore cursor
        }

        Position WalkBackwards(Position start, float distance, IndexRange skip_measure_range)
        {
            DisableDrawingGuard guard{};
            auto i = start.entry_index;
            distance -= start.pixel_offset;
            while (i > 0 && distance > 0)
            {
                --i;
                if (!skip_measure_range.Contains(i))
                    MeasureEntry(i);
                distance -= this->item_sizes[i];
            }
            return {
                .entry_index = i,
                .pixel_offset = -distance,
            };
        }

        Position WalkForwards(Position start, float distance, IndexRange skip_measure_range)
        {
            DisableDrawingGuard guard{};
            auto i = start.entry_index;
            distance += start.pixel_offset;
            while (i < this->item_sizes.size() && distance >= this->item_sizes[i])
            {
                if (!skip_measure_range.Contains(i))
                    MeasureEntry(i);
                distance -= this->item_sizes[i];
                ++i;
            }
            return {
                .entry_index = i,
                .pixel_offset = distance,
            };
        }

        struct DisableDrawingGuard
        {
            DisableDrawingGuard() { ImGui::PushClipRect(ImVec2(0, 0), ImVec2(0, 0), false); }
            ~DisableDrawingGuard() { ImGui::PopClipRect(); }
        };

        Position CalcScrollMax()
        {
            // Figure out the furthest down position we can scroll to
            DisableDrawingGuard guard{};
            float view_height = GetViewHeight();
            auto pos = WalkBackwards(Position{this->item_sizes.size(), 0}, view_height, IndexRange{0, 0});
            return Position::Max(pos, Position{0, 0});
        }

        void UpdateTargetWithInput(bool snap_to_items, IndexRange &measured_range)
        {
            measured_range = IndexRange::None();

            float wheel_input = GetWheelInput();

            if (wheel_input == 0.f)
                return;

            if (snap_to_items)
            {
                // Update target with wheel input when scrolling by items
                auto delta_index = (int32_t)std::round(wheel_input);
                auto target_index = (int32_t)this->scroll_target.entry_index + delta_index;

                if (delta_index < 0 && scroll_target.pixel_offset > 0)
                {
                    // If we're scrolling backwards and we're not at the start of the entry, scroll to the start of the entry
                    ++target_index;
                }

                if (target_index < 0)
                {
                    this->scroll_target = {
                        .entry_index = 0,
                        .pixel_offset = 0,
                    };
                }
                else if (target_index > this->scroll_max.entry_index)
                {
#ifdef _DEBUG
                    Utils::FormatToChat(0xFFFFFFFF, L"Scroll max reached");
#endif
                    this->scroll_target = this->scroll_max;
                }
                else
                {
                    this->scroll_target = {
                        .entry_index = (uint32_t)target_index,
                        .pixel_offset = 0,
                    };
                }
            }
            else
            {
                // Update target with wheel input when scrolling by pixels
                auto delta_pixels = (int32_t)std::round(wheel_input * 100.f);
                auto pixel_offset = (int32_t)this->scroll_target.pixel_offset + delta_pixels;
                auto distance = std::abs(pixel_offset);
                Position new_target;
                if (pixel_offset < 0)
                {
                    new_target = WalkBackwards(this->scroll_target, distance, IndexRange::None());
                    new_target = Position::Max(new_target, Position{0, 0});
                    measured_range = IndexRange{new_target.entry_index, this->scroll_target.entry_index};
                }
                else
                {
                    new_target = WalkForwards(this->scroll_target, distance, IndexRange::None());
                    new_target = Position::Min(new_target, this->scroll_max);
                    measured_range = IndexRange{this->scroll_target.entry_index, new_target.entry_index};
                }
                this->scroll_target = new_target;
            }
        }

        float CalcDistance(Position start, Position end, IndexRange skip_measure_range)
        {
            bool is_backward = end < start;
            if (is_backward)
            {
                std::swap(start, end);
            }
            DisableDrawingGuard guard{};
            uint32_t height_sum = 0;
            for (uint32_t i = start.entry_index; i < end.entry_index; ++i)
            {
                if (!skip_measure_range.Contains(i))
                    MeasureEntry(i);
                height_sum += item_sizes[i];
            }
            auto distance = (float)height_sum + end.pixel_offset - start.pixel_offset;
            if (is_backward)
            {
                distance = -distance;
            }
            return distance;
        }

        static float SmoothScroll(float current_scroll, float target_scroll, float dt)
        {
            assert(dt >= 0.f);
#ifdef _DEBUG
            dt *= 0.1f; // Slow down the smooth scrolling for debugging purposes, so we can see the scroll position change
#endif
            const bool positive_sign = target_scroll > current_scroll;
            const auto sign = positive_sign ? 1.f : -1.f;
            const auto dist = std::abs(target_scroll - current_scroll);

            float c = std::powf(2e-4, dt);
            assert(c >= 0.f && c <= 1.f);

            auto dist_eased = std::max(c * dist - 100.f * dt, 0.f);
            // auto dist_eased = std::max(dist - 100.f * dt, 0.f);

            auto new_scroll = target_scroll - sign * dist_eased;
#ifdef _DEBUG
            SOFT_ASSERT(new_scroll == target_scroll || std::signbit(target_scroll - current_scroll) == std::signbit(target_scroll - new_scroll));
#endif
            return new_scroll;
        }

        uint32_t Size()
        {
            return item_sizes.size();
        }

        float GetViewHeight()
        {
            return this->window->Size.y;
        }

        float GetWheelInput()
        {
            if (ImGui::IsWindowHovered())
            {
                const auto &io = ImGui::GetIO();
                return -io.MouseWheel; // Negate, so that positive is down.
            }
            return 0.f;
        }
    };
}