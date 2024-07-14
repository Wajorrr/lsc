#include <assert.h>
#include <libconfig.h++>

#include "admission/admission.hpp"
#include "config_reader.hpp"
#include "admission/random_admission.hpp"
#include "admission/threshold.hpp"

namespace admission
{

    Policy::Policy(stats::LocalStatsCollector &admission_stats,
                   flashCache::SetsAbstract *sets, flashCache::LogAbstract *log) : _admission_stats(admission_stats), _sets(sets), _log(log)
    {
    }

    Policy *Policy::create(const libconfig::Setting &settings, flashCache::SetsAbstract *sets,
                           flashCache::LogAbstract *log, stats::LocalStatsCollector &admission_stats)
    {
        misc::ConfigReader cfg(settings);

        std::string policyType = cfg.read<const char *>("policy");

        std::cout << "Admission policy: " << policyType << std::endl;

        if (policyType == "Random") // 概率准入
        {
            return new RandomAdmission(settings, sets, log, admission_stats);
        }
        else if (policyType == "Threshold") // set大小阈值准入，包含对象数量少于阈值的set不被准入
        {
            return new Threshold(settings, sets, log, admission_stats);
        }
        else
        {
            std::cerr << "Unknown admission policy: " << policyType << std::endl;
            assert(false);
        }

        return NULL;
    }

    // 给定一批对象，将各个对象按其映射到的set分组，返回分组情况：<组号，组内对象列表>
    std::unordered_map<uint64_t, std::vector<candidate_t>> Policy::groupBasic(std::vector<candidate_t> items)
    {
        assert(_sets); // give more helpful error than segfault
        // 各个对象对应的不同set
        std::unordered_map<uint64_t, std::vector<candidate_t>> grouped;
        for (auto item : items) // 遍历对象
        {
            // 返回对象可以映射到的一组setNum，取第一个
            uint64_t setNum = *(_sets->findSetNums(item).begin());
            grouped[setNum].push_back(item);
        }
        return grouped;
    }

    void Policy::trackPossibleAdmits(std::vector<candidate_t> items)
    { // 记录有多少对象执行了准入判断，用于计算准入比例
        _admission_stats["trackPossibleAdmitsCalls"]++;
        for (auto item : items)
        {
            _admission_stats["numPossibleAdmits"]++;
            _admission_stats["sizePossibleAdmits"] += item.obj_size;
        }
    }

    void Policy::trackAdmitted(std::vector<candidate_t> items)
    { // 记录实际准入了多少对象
        _admission_stats["trackAdmittedCalls"]++;
        for (auto item : items)
        {
            _admission_stats["numAdmits"]++;
            _admission_stats["sizeAdmits"] += item.obj_size;
        }
    }

    double Policy::byteRatioAdmitted()
    { // 准入的字节比例
        return _admission_stats["sizeAdmits"] / (double)_admission_stats["sizePossibleAdmits"];
    }

    void Policy::performReadmission(std::vector<candidate_t> evicted)
    {
        if (!_log)
        {
            /* no readmission possible */
            return;
        }
        else
        {
            _log->readmit(evicted);
        }
    }

}
