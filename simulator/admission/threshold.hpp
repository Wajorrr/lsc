#pragma once
#include <limits>

#include "common/rand.hpp"
#include "config_reader.hpp"
#include "stats/stats.hpp"

namespace admission
{

    class Threshold : public virtual Policy
    {
    public:
        Threshold(const libconfig::Setting &settings, flashCache::SetsAbstract *sets,
                  flashCache::LogAbstract *log, stats::LocalStatsCollector &admission_stats) : Policy(admission_stats, sets, log)
        {
            misc::ConfigReader cfg(settings);
            threshold = cfg.read<int>("threshold"); // 准入阈值
            assert(threshold > 1);
            _admission_stats["thresholdValue"] = threshold;
        }

        std::unordered_map<uint64_t, std::vector<candidate_t>> admit(std::vector<candidate_t> items)
        {
            std::unordered_map<uint64_t, std::vector<candidate_t>> ret = groupBasic(items);
            std::vector<candidate_t> evicted;
            // 遍历当前这些对象映射到的各个set
            for (auto it = ret.begin(); it != ret.end();)
            {
                trackPossibleAdmits(it->second);
                // 包含对象数量小于阈值的set不被准入
                if (it->second.size() < (uint)threshold)
                {
                    evicted.insert(evicted.end(), it->second.begin(), it->second.end());
                    it = ret.erase(it);
                }
                else
                {
                    trackAdmitted(it->second);
                    ++it;
                }
            }
            performReadmission(evicted);
            return ret;
        }

        std::vector<candidate_t> admit_simple(std::vector<candidate_t> items)
        {
            std::vector<candidate_t> admitted;
            std::cout << "Threshold admission filter needs sets" << std::endl;
            assert(false);
            return admitted;
        }

    private:
        int threshold;
    };

} // admission namespace
