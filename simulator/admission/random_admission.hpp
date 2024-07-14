#pragma once
#include <limits>

#include "admission/admission.hpp"
#include "common/rand.hpp"
#include "candidate.hpp"
#include "config_reader.hpp"
#include "stats/stats.hpp"

namespace admission
{

    class RandomAdmission : public virtual Policy
    {

    public:
        RandomAdmission(const libconfig::Setting &settings, flashCache::SetsAbstract *sets,
                        flashCache::LogAbstract *log, stats::LocalStatsCollector &admission_stats) : Policy(admission_stats, sets, log)
        {
            misc::ConfigReader cfg(settings);
            _admit_ratio = cfg.read<float>("admitRatio");                           // 准入比例
            _admit_threshold = _admit_ratio * std::numeric_limits<uint64_t>::max(); // 准入阈值
        }

        // 先把对象按照映射到的set分组，然后执行准入，未准入的对象将被刷新到set映射flash缓存？，或者直接丢弃
        std::unordered_map<uint64_t, std::vector<candidate_t>> admit(std::vector<candidate_t> items)
        {
            // 获取分组情况
            std::unordered_map<uint64_t, std::vector<candidate_t>> ret = groupBasic(items);
            // 按比例分配驱逐对象数组
            std::vector<candidate_t> evicted(int(_admit_ratio * items.size()) + 1);
            // 遍历当前这些对象映射到的各个set
            for (auto &bin : ret)
            {
                trackPossibleAdmits(bin.second);
                // 每个set中分别包括哪些对象
                for (auto it = bin.second.begin(); it != bin.second.end();)
                {
                    if (_rand_gen.next() > _admit_threshold) // 若生成的随机数超过了准入阈值
                    {
                        evicted.push_back(*it);
                        it = bin.second.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                trackAdmitted(bin.second);
            }
            performReadmission(evicted);
            return ret; // 过滤掉未准入对象后剩下的对象
        }

        // 简单准入，只准入对象，不记录未被准入的对象
        std::vector<candidate_t> admit_simple(std::vector<candidate_t> items)
        {
            std::vector<candidate_t> admitted;
            admitted.reserve(int(_admit_ratio * items.size()) + 1);
            trackPossibleAdmits(items);
            for (auto it = items.begin(); it != items.end();)
            {
                if (_rand_gen.next() <= _admit_threshold)
                {
                    admitted.push_back(*it);
                }
                ++it;
            }
            trackAdmitted(admitted);
            return admitted;
        }

    private:
        float _admit_ratio;
        uint64_t _admit_threshold;
        misc::Rand _rand_gen;
    };

} // admission namespace
