#pragma once

#include <source_location>
#include <utils.h>
#include <vector>

#include <GWCA/Managers/MemoryMgr.h>

namespace HerosInsight
{
#ifdef _PROFILING
    class ProfilingScope
    {
        struct ActiveMeasurement
        {
            uint64_t start_cycles;
            DWORD start_ms;

            ActiveMeasurement()
            {
                _mm_lfence();
                this->start_cycles = __rdtsc();
                this->start_ms = GW::MemoryMgr::GetSkillTimer();
            }
        };

        struct Measurement
        {
            float cycles;
            float seconds;

            Measurement(ActiveMeasurement &active)
            {
                uint32_t cpu_id;
                uint64_t end_cycles = __rdtscp(&cpu_id);
                DWORD end_ms = GW::MemoryMgr::GetSkillTimer();
                _mm_lfence();
                this->cycles = float(end_cycles - active.start_cycles);
                this->seconds = float(end_ms - active.start_ms) / 1000.f;
            }
        };

        struct Stat
        {
            float total = 0;
            float max = 0;
            float min = std::numeric_limits<float>::max();

            void Assimilate(float value)
            {
                total += value;
                max = std::max(max, value);
                min = std::min(min, value);
            }
        };

        struct Measurements
        {
            Stat seconds{};
            Stat cycles{};
            uint32_t count = 0;

            void Assimilate(Measurement &measurement)
            {
                seconds.Assimilate(measurement.seconds);
                cycles.Assimilate(measurement.cycles);
                count++;
            }
            float AvgSeconds() const { return seconds.total / count; }
            float AvgCycles() const { return cycles.total / count; }
        };

        struct SrcLocHash
        {
            size_t value;

            constexpr size_t CalcHash(const std::source_location &loc)
            {
                return size_t(loc.file_name()) >> 4 ^
                       size_t(loc.function_name()) >> 4 ^
                       size_t(loc.line() * 1024 + loc.column());
            }

            constexpr SrcLocHash(const std::source_location &loc) : value(CalcHash(loc)) {}
        };

        struct ProfilerKey
        {
            std::source_location loc;
            SrcLocHash loc_hash;
            std::string_view context;

            std::string_view FindFuncName() const
            {
                auto raw = loc.function_name();
                auto p = raw;
                while (*p != '\0' && *p != '(')
                    ++p;
                auto end = p;
                while (p > raw && p[-1] != ':')
                    --p;
                return std::string_view{p, end};
            }

            bool operator==(const ProfilerKey &rhs) const
            {
                return loc_hash.value == rhs.loc_hash.value &&
                       context.data() == rhs.context.data() &&
                       loc.line() == rhs.loc.line() &&
                       loc.column() == rhs.loc.column() &&
                       loc.file_name() == rhs.loc.file_name() &&
                       loc.function_name() == rhs.loc.function_name();
            }

            struct Hasher
            {
                size_t operator()(const ProfilerKey &key) const { return key.loc_hash.value ^ size_t(key.context.data()); }
            };
        };

        struct ThreadData
        {
            std::unordered_map<ProfilerKey, Measurements, ProfilerKey::Hasher> profilers_accum;
            ProfilingScope *current_scope = nullptr;
        };

        inline static thread_local ThreadData thread_data;

        ProfilingScope *parent;
        Measurements sub_measurements;
        std::string_view context;
        float report_threshold;
        ProfilerKey key;
        ActiveMeasurement active_meas;

    public:
        ProfilingScope(
            std::string_view context = {},
            float report_threshold = 0.1f,
            SrcLocHash loc_hash = SrcLocHash{std::source_location::current()},
            const std::source_location &loc = std::source_location::current()
        )
            : parent(thread_data.current_scope),
              sub_measurements(),
              context(context),
              report_threshold(report_threshold),
              key(loc, loc_hash, context),
              active_meas()
        {
            thread_data.current_scope = this;
        }

        Measurement CalcSelfMeasurement(const Measurement &tot_meas)
        {
            auto self_meas = tot_meas;
            self_meas.seconds -= sub_measurements.seconds.total;
            self_meas.cycles -= sub_measurements.cycles.total;
            return self_meas;
        }

        ~ProfilingScope()
        {
            thread_data.current_scope = parent;

            Measurement tot_meas{this->active_meas};
            Measurement self_meas = CalcSelfMeasurement(tot_meas);

            if (parent)
            {
                parent->sub_measurements.Assimilate(tot_meas);
                thread_data.profilers_accum[key].Assimilate(self_meas);
            }
            else
            {
                FixedVector<char, 1024> buffer;
                buffer.AppendFormat(
                    "{}:{} took {}s, {}cyc",
                    key.FindFuncName(),
                    context,
                    Utils::ToHumanReadable(tot_meas.seconds),
                    Utils::ToHumanReadable(tot_meas.cycles)
                );

                float self_usage = self_meas.cycles / tot_meas.cycles;
                if (self_usage >= report_threshold)
                {
                    buffer.AppendFormat(
                        "\n{:.0f}% - SELF",
                        self_usage * 100.f
                    );
                }

                for (auto &[key, value] : thread_data.profilers_accum)
                {
                    float usage = value.cycles.total / tot_meas.cycles;
                    if (usage < report_threshold)
                        continue;

                    buffer.AppendFormat(
                        "\n{:.0f}% - {}:{}",
                        usage * 100.f,
                        key.FindFuncName(),
                        key.context
                    );
                }
                thread_data.profilers_accum.clear();
                buffer.push_back('\0');
                Utils::WriteToChat(buffer.data());
            }
        }
    };

#else
    struct ProfilingScope
    {
        FORCE_INLINE ProfilingScope(std::string_view, float = 0.f) noexcept {}
    };
#endif
}