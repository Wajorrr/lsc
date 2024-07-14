#include <cmath>

#include "block_log_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace cache
{

    BlockLogCache::BlockLogCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : BlockCache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        /* Initialize flash cache, split size among log and
         * sets so that sets are always a multiple of setCapacity
         * and close to provided percentLog, leans toward more log */
        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存容量
        uint64_t log_capacity = flash_size_mb * 1024 * 1024;

        /* figure out what log version to use and initialize it */
        uint64_t readmit = cfg.read<int>("log.readmit", 0);
        auto &log_stats = statsCollector->createLocalCollector("log");

        uint64_t segment_size = 1024 * 1024 * (uint64_t)cfg.read<int>("log.segmentSizeMB", 2);

        _log = new flashCache::BlockLog(log_capacity, segment_size, log_stats);

        /* Initialize prelog admission policy */
        if (cfg.exists("preLogAdmission")) // 是否使用准入策略
        {
            std::string policyType = cfg.read<const char *>("preLogAdmission.policy");
            std::cout << "Creating admission policy of type " << policyType << std::endl;
            policyType.append(".preLogAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preLogAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            // _prelog_admission = admission::Policy::create(admission_settings, nullptr, _log, admission_stats);
        }

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        {
            warmed_up = true;
        }
        assert(warmed_up);
    }

    BlockLogCache::~BlockLogCache()
    {
        delete _log;
        // delete _prelog_admission;
    }

    void BlockLogCache::insert(Block id)
    {
        _log->insert({id}); // 插入flash日志记录缓存

        /* check warmed up condition every so often */
        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    bool BlockLogCache::find(Block id)
    {
        // 分别查找内存缓存和flash日志缓存
        if (_log->find(id))
        {
            return true;
        }
        return false;
    }

    void BlockLogCache::update(Block id)
    {
        _log->update({id});
    }

    double BlockLogCache::calcFlashWriteAmp()
    { // 计算写放大
        double flash_write_amp = _log->calcWriteAmp();
        return flash_write_amp;
    }

    void BlockLogCache::checkWarmup()
    {
        // 当flash日志缓存的累计驱逐大小超过容量时，预热结束
        if (_log->ratioEvictedToCapacity() < 1)
        {
            return;
        }
        flushStats(); // 总体的统计信息
        std::cout << "Reached end of warmup, resetting stats\n\n";
        _log->flushStats(); // flash日志缓存统计信息
        dumpStats();        // 将统计信息写入输出文件
        warmed_up = true;
    }

} // namespace cache
