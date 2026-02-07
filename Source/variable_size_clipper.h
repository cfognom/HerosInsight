#pragma once

#include <functional>
#include <numbers>
#include <update_manager.h>

// #define PRINT_VARIABLE_SIZE_CLIPPER_TIMING

namespace HerosInsight
{
    // Must not be stack allocated, contains state
    struct VariableSizeClipper
    {
        struct Position
        {
            uint32_t item_index;
            float pixel_offset;

            bool operator==(const Position &other) const
            {
                return item_index == other.item_index &&
                       pixel_offset == other.pixel_offset;
            }

            bool operator<(const Position &other) const
            {
                return item_index < other.item_index ||
                       (item_index == other.item_index && pixel_offset < other.pixel_offset);
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
            return this->scroll_current;
        }

        Position GetTargetScroll() const
        {
            return this->scroll_target;
        }

        void SetScroll(Position scroll)
        {
            this->scroll_target = scroll;
            this->scroll_current = scroll;
            this->scroll_set_by_api = true;
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
            this->scroll_current = Position::Min(this->scroll_current, this->scroll_max); // Safety if number of items changed or item sizes changed
            this->scroll_target = Position::Min(this->scroll_target, this->scroll_max);   // Safety if number of items changed or item sizes changed

            bool is_pressing_scrollbar = this->IsPressingScrollbar();
            if (is_pressing_scrollbar)
            {
                // When pressing scrollbar:
                // - Mouse pointer drives imgui_scroll
                // - imgui_scroll drives scroll_current + scroll_target

                auto imgui_scroll = ImGui::GetScrollY();
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
            else
            {
                // When not pressing scrollbar:
                // - Mouse wheel drives scroll_target
                // - scroll_target drives scroll_current
                // - scroll_current drives imgui_scroll

                auto measured_range = IndexRange::None(); // We keep track of which entries were measured this frame to avoid measuring them twice
                UpdateTargetWithInput(snap_to_items, measured_range);

                bool set_imgui_scroll = false;

                if (scroll_current != scroll_target)
                {
                    // Update sizes between scroll_current and scroll_target and calculate accurate distance
                    auto distance_to_target = CalcDistance(scroll_current, scroll_target, measured_range);

                    // Calculate new scroll_current
                    auto dt = ImGui::GetIO().DeltaTime;
                    // #ifdef _DEBUG
                    //                     dt *= 0.1f; // Slow motion scrolling for debugging purposes
                    // #endif
                    auto new_distance_to_target = DecayError(distance_to_target, dt);
                    if (new_distance_to_target == 0)
                    {
                        this->scroll_current = this->scroll_target;
                    }
                    else
                    {
                        auto scroll_delta = distance_to_target - new_distance_to_target;

                        // Apply scroll_delta to scroll_current
                        // We can skip measuring since we already did that when we calculated distance_to_target
                        if (scroll_delta >= 0)
                        {
                            this->scroll_current = WalkForwards(this->scroll_current, scroll_delta, IndexRange::All());
                            this->scroll_current = Position::Min(this->scroll_current, this->scroll_max);
                        }
                        else
                        {
                            this->scroll_current = WalkBackwards(this->scroll_current, -scroll_delta, IndexRange::All());
                            this->scroll_current = Position::Max(this->scroll_current, Position{0, 0});
                        }
                    }

                    set_imgui_scroll = true;
                }

                if (this->scroll_set_by_api)
                {
                    this->scroll_set_by_api = false;
                    set_imgui_scroll = true;
                }

                if (set_imgui_scroll)
                {
                    // Apply scroll_current to imgui_scroll
                    auto new_imgui_scroll = std::round(CalcDistance(Position{0, 0}, this->scroll_current, IndexRange::All())); // We skip measuring for performance
                    ImGui::SetScrollY(new_imgui_scroll);
                }
            }

            auto view_rect = window->InnerRect;
            float first_item_cursor_ss = view_rect.Min.y - std::round(this->scroll_current.pixel_offset);
            size_t i = this->scroll_current.item_index;
            ImGui::SetCursorScreenPos(ImVec2(view_rect.Min.x, first_item_cursor_ss));
            float item_cursor_ss = first_item_cursor_ss;
            for (; i < item_sizes.size() && item_cursor_ss < view_rect.Max.y; ++i)
            {
                DrawAndMeasureItem(i);
                item_cursor_ss = ImGui::GetCursorScreenPos().y;
            }
            auto trailing_height = CalcDistance(Position{i, 0}, Position{item_sizes.size(), 0}, IndexRange::All()); // We skip measuring for performance
            auto end_cursor_ss = item_cursor_ss + trailing_height;
            ImGui::SetCursorScreenPos(ImVec2(view_rect.Min.x, end_cursor_ss)); // This tricks ImGui into thinking we used all space up to end_cursor_ss

// #ifdef _DEBUG
//             DrawDebugInfo();
// #endif
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
        bool scroll_set_by_api = false;

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
                ImGui::PushFont(Constants::Fonts::skill_name_font);
                draw_list->AddText(text_pos, IM_COL32(0, 255, 0, 255), str.c_str());
                ImGui::PopFont();
            }
        }

        void DrawDebugInfo()
        {
            DisplayWheelInput();
            ImGui::Text("target_scroll_index: %d, target_scroll_offset: %f", this->scroll_target.item_index, this->scroll_target.pixel_offset);
        }
#endif

        bool IsPressingScrollbar()
        {
            auto scroll_bar_id = ImGui::GetWindowScrollbarID(this->window, ImGuiAxis_Y);
            return ImGui::GetCurrentContext()->ActiveId == scroll_bar_id;
        }

        void UpdateItemSize(uint32_t index, uint16_t size)
        {
            auto &old_size = this->item_sizes[index];
#ifdef _DEBUG
            assert(std::floor(size) == size);
            // if (old_size != size)
            // {
            //     Utils::FormatToChat(L"Item {} size changed from {} to {}", index, old_size, size);
            // }
#endif
            old_size = size;
        }

        void DrawItem(uint32_t index)
        {
            ImGui::BeginGroup();
            draw_item(index);
            ImGui::EndGroup();
        };

        void DrawAndMeasureItem(uint32_t index)
        {
            auto cursor = ImGui::GetCursorScreenPos().y;
            DrawItem(index);
            auto size = ImGui::GetCursorScreenPos().y - cursor;
            UpdateItemSize(index, size);
        }

        void MeasureItem(uint32_t index)
        {
            auto init_cursor = ImGui::GetCursorScreenPos();
            auto measure_pos = ImVec2(window->InnerClipRect.Min.x, 0.f);
            ImGui::SetCursorScreenPos(measure_pos);
            DrawAndMeasureItem(index);
            ImGui::SetCursorScreenPos(init_cursor); // Restore cursor
        }

        Position WalkBackwards(Position start, float distance, IndexRange skip_measure_range)
        {
            DisableDrawingGuard guard{};
            auto i = start.item_index;
            distance -= start.pixel_offset;
            while (i > 0 && distance > 0)
            {
                --i;
                if (!skip_measure_range.Contains(i))
                    MeasureItem(i);
                distance -= this->item_sizes[i];
            }
            return {
                .item_index = i,
                .pixel_offset = -distance,
            };
        }

        Position WalkForwards(Position start, float distance, IndexRange skip_measure_range)
        {
            DisableDrawingGuard guard{};
            auto i = start.item_index;
            distance += start.pixel_offset;
            while (i < this->item_sizes.size() && distance >= this->item_sizes[i])
            {
                if (!skip_measure_range.Contains(i))
                    MeasureItem(i);
                distance -= this->item_sizes[i];
                ++i;
            }
            return {
                .item_index = i,
                .pixel_offset = distance,
            };
        }

        struct DisableDrawingGuard
        {
            bool active;
            DisableDrawingGuard()
            {
                active = ImGui::GetCurrentWindowRead() != nullptr;
                if (active)
                    ImGui::PushClipRect(ImVec2(0, 0), ImVec2(0, 0), false);
            }
            ~DisableDrawingGuard()
            {
                if (active)
                    ImGui::PopClipRect();
            }
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
                auto target_index = (int32_t)this->scroll_target.item_index + delta_index;

                if (delta_index < 0 && scroll_target.pixel_offset > 0)
                {
                    // If we're scrolling backwards and we're not at the start of the item, scroll to the start of the item
                    ++target_index;
                }

                if (target_index < 0)
                {
                    this->scroll_target = {
                        .item_index = 0,
                        .pixel_offset = 0,
                    };
                }
                else if (target_index > this->scroll_max.item_index)
                {
                    // #ifdef _DEBUG
                    //                     Utils::FormatToChat(0xFFFFFFFF, L"Scroll max reached");
                    // #endif
                    this->scroll_target = this->scroll_max;
                }
                else
                {
                    this->scroll_target = {
                        .item_index = (uint32_t)target_index,
                        .pixel_offset = 0,
                    };
                }
            }
            else
            {
                // Update target with wheel input when scrolling by pixels
                auto delta_pixels = std::round(wheel_input * 100.f);

                // We make the start pos the start of the current item in case we have the wrong size
                auto pixel_offset = this->scroll_target.pixel_offset + delta_pixels;
                auto start_pos = Position{this->scroll_target.item_index, 0};
                auto distance = std::abs(pixel_offset);
                Position new_target;
                if (pixel_offset < 0)
                {
                    new_target = WalkBackwards(start_pos, distance, IndexRange::None());
                    new_target = Position::Max(new_target, Position{0, 0});
                    measured_range = IndexRange{new_target.item_index, this->scroll_target.item_index};
                }
                else
                {
                    new_target = WalkForwards(start_pos, distance, IndexRange::None());
                    new_target = Position::Min(new_target, this->scroll_max);
                    measured_range = IndexRange{this->scroll_target.item_index, new_target.item_index};
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
            for (uint32_t i = start.item_index; i < end.item_index; ++i)
            {
                if (!skip_measure_range.Contains(i))
                    MeasureItem(i);
                height_sum += item_sizes[i];
            }
            auto distance = (float)height_sum + end.pixel_offset - start.pixel_offset;
            if (is_backward)
            {
                distance = -distance;
            }
            return distance;
        }

        // Mixed exponential and linear decay
        static float DecayError(float error, float dt)
        {
            constexpr float hl = 0.0814f; // Half life (s)
            constexpr float k = std::numbers::ln2 / hl;
            float factor = std::exp(-k * dt);
            auto sign = error > 0.f ? 1.f : -1.f;
            constexpr float base_speed = 100.f;
            error = sign * std::max(std::abs(error) * factor - base_speed * dt, 0.f);
            return error;
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