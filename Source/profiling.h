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
            struct MedianBins // Used to calculate approximate median
            {
                uint32_t counts[256]; // indexed by float exponent (8 bits)
            };

            float total = 0;
            float max = std::numeric_limits<float>::lowest();
            float min = std::numeric_limits<float>::max();
            uint32_t count = 0;
            MedianBins median_bins;

            float Avg() const
            {
                GWCA_ASSERT(count > 0);
                return total / count;
            }
            float CalcApproxMedian() const
            {
                GWCA_ASSERT(count > 0);
                uint32_t target_index = count / 2;
                for (uint32_t exp_index = 0; exp_index < std::size(median_bins.counts); ++exp_index)
                {
                    auto bin_count = median_bins.counts[exp_index];
                    if (target_index < bin_count)
                    {
                        constexpr uint64_t max_mantissa = 0x007FFFFF;
                        uint32_t mantissa = ((uint64_t)target_index * max_mantissa) / bin_count; // We assume uniform distribution within a bin, and do a linear interpolation.
                        GWCA_ASSERT(mantissa <= max_mantissa);
                        uint32_t float_bits = (exp_index << 23) | mantissa;
                        return std::bit_cast<float>(float_bits);
                    }
                    target_index -= bin_count;
                }
                return 0; // Should never be hit
            }

            void Assimilate(float value)
            {
                GWCA_ASSERT(value >= 0);
                total += value;
                max = std::max(max, value);
                min = std::min(min, value);
                ++count;
                auto exp_index = (std::bit_cast<uint32_t>(value) >> 23) & 0xFF;
                ++median_bins.counts[exp_index];
            }
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
            std::unordered_map<ProfilerKey, Stat, ProfilerKey::Hasher> profiler_cycles_accum;
            ProfilingScope *current_scope = nullptr;
        };

        inline static thread_local ThreadData thread_data;

        ProfilingScope *parent;
        float child_cycles = 0; // How many cycles were spent in child scopes. Used to calculate "self" cycles.
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
              context(context),
              report_threshold(report_threshold),
              key(loc, loc_hash, context),
              active_meas()
        {
            thread_data.current_scope = this;
        }

        ~ProfilingScope()
        {
            thread_data.current_scope = parent;

            Measurement tot_meas{this->active_meas};
            float self_cycles = tot_meas.cycles - child_cycles;

            if (parent)
            {
                parent->child_cycles += tot_meas.cycles;
                thread_data.profiler_cycles_accum[key].Assimilate(self_cycles);
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

                float self_usage = self_cycles / tot_meas.cycles;
                if (self_usage >= report_threshold)
                {
                    buffer.AppendFormat(
                        "\n{:.0f}% - SELF",
                        self_usage * 100.f
                    );
                }

                for (auto &[key, cycles] : thread_data.profiler_cycles_accum)
                {
                    GWCA_ASSERT(cycles.count > 0);

                    float usage = cycles.total / tot_meas.cycles;
                    if (usage < report_threshold)
                        continue;

                    buffer.AppendFormat(
                        "\n{:.0f}%, n={}",
                        usage * 100.f,
                        cycles.count
                    );

                    if (cycles.count > 1)
                    {
                        float med_usage = cycles.CalcApproxMedian() / tot_meas.cycles;
                        if (med_usage >= report_threshold)
                        {
                            buffer.AppendFormat(
                                ", med={:.0f}%",
                                med_usage * 100.f
                            );
                        }
                        float avg_usage = cycles.Avg() / tot_meas.cycles;
                        if (avg_usage >= report_threshold)
                        {
                            buffer.AppendFormat(
                                ", avg={:.0f}%",
                                avg_usage * 100.f
                            );
                        }
                        float min_usage = cycles.min / tot_meas.cycles;
                        if (min_usage >= report_threshold)
                        {
                            buffer.AppendFormat(
                                ", min={:.0f}%",
                                min_usage * 100.f
                            );
                        }
                        float max_usage = cycles.max / tot_meas.cycles;
                        if (max_usage >= report_threshold)
                        {
                            buffer.AppendFormat(
                                ", max={:.0f}%",
                                max_usage * 100.f
                            );
                        }
                    }

                    buffer.AppendFormat(
                        " - {}:{}",
                        key.FindFuncName(),
                        key.context
                    );
                }
                thread_data.profiler_cycles_accum.clear();
                buffer.push_back('\0');
                Utils::WriteToChat(buffer.data());
            }
        }
    };

#else
    struct ProfilingScope
    {
        FORCE_INLINE ProfilingScope(std::string_view = {}, float = 0.f) noexcept {}
    };
#endif
}