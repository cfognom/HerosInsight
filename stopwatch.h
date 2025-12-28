#pragma once

#include <GWCA/Managers/MemoryMgr.h>
#include <utils.h>
#include <vector>

namespace HerosInsight
{
    // TODO: Use barriers to prevent instruction reordering?
    struct Stopwatch
    {
        Stopwatch(std::string_view name, float report_threshold = 0.1f)
            : thread(GetCurrentThread()),
              name(name),
              report_threshold(report_threshold)
        {
            checkpoints.emplace_back(thread, "begin");
        }

        ~Stopwatch()
        {
            checkpoints.emplace_back(thread, "end");

            FixedVector<char, 1024> buffer;
            Measurement main_report(checkpoints.front(), checkpoints.back());
            buffer.AppendFormat(
                "{} took {}s, {}cyc, thrd_util: {:.0f}",
                name,
                Utils::ToHumanReadable(main_report.seconds),
                Utils::ToHumanReadable(main_report.cycles),
                main_report.thread_utilization
            );

            for (size_t i = 1; i < checkpoints.size(); ++i)
            {
                Measurement report(checkpoints[i - 1], checkpoints[i]);
                float usage = report.cycles / main_report.cycles;
                if (usage < report_threshold)
                    continue;

                buffer.AppendFormat(
                    "\n{} - {}: {:.0f}% of cycles, thrd_util: {:.0f}",
                    report.name_from,
                    report.name_to,
                    usage * 100.f,
                    report.thread_utilization
                );
            }
            buffer.push_back('\0');
            Utils::WriteToChat(buffer.data());
        }

        void Checkpoint(std::string &&name)
        {
            checkpoints.emplace_back(thread, std::forward<std::string>(name));
        }

    private:
        struct CheckpointData
        {
            std::string name;
            uint64_t cycles;
            uint64_t active_cycles;
            DWORD ms;

            CheckpointData(HANDLE thread, std::string &&name)
                : name(std::forward<std::string>(name)),
                  cycles(__rdtsc()),
                  ms(GW::MemoryMgr::GetSkillTimer())
            {
                QueryThreadCycleTime(thread, &active_cycles);
            }
        };

        struct Measurement
        {
            std::string_view name_from;
            std::string_view name_to;
            float seconds;
            float cycles;
            float active_cycles;
            float thread_utilization;

            Measurement(CheckpointData &begin, CheckpointData &end)
                : name_from(begin.name),
                  name_to(end.name),
                  seconds(float(end.ms - begin.ms) / 1000.f),
                  cycles(float(end.cycles - begin.cycles)),
                  active_cycles(float(end.active_cycles - begin.active_cycles)),
                  thread_utilization(active_cycles / cycles)
            {
            }
        };

        HANDLE thread;
        std::string_view name;
        float report_threshold;
        std::vector<CheckpointData> checkpoints;
    };
}