#include <cmath>

#include "caches/mem_log_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "kangaroo/lru.hpp"
#include "stats/stats.hpp"

namespace cache
{

    MemLogCache::MemLogCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : Cache(sc, gs, settings)
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

        // 下刷块大小的大小，默认为256KB，即擦除块的大小
        uint64_t block_size = 1024 * (uint64_t)cfg.read<int>("log.flushBlockSizeKB", 256);
        _log = new flashCache::LogOnly(log_capacity, block_size, log_stats, readmit);

        // 内存缓存大小
        uint64_t memory_size = (uint64_t)cfg.read<int>("cache.memorySizeMB") * 1024 * 1024;
        auto &memory_cache_stats = statsCollector->createLocalCollector("memCache");

        // flash日志存储使用的内存索引空间比例
        double perc_mem_log_overhead = cfg.read<float>("cache.memOverheadRatio", INDEX_LOG_RATIO);
        std::cout << "Log Capacity: " << log_capacity << " mem capacity " << memory_size
                  << " percent " << perc_mem_log_overhead
                  << std::endl;
        assert(log_capacity * perc_mem_log_overhead <= memory_size);

        // 除去索引后，剩下的内存缓存空间容量
        uint64_t mem_cache_capacity = memory_size - (log_capacity * perc_mem_log_overhead);
        std::cout << "Actual Memory Cache Size after indexing costs: "
                  << mem_cache_capacity << std::endl;
        _memCache = new memcache::LRU(mem_cache_capacity, memory_cache_stats);

        /* Initialize prelog admission policy */
        if (cfg.exists("preLogAdmission")) // 是否使用准入策略
        {
            std::string policyType = cfg.read<const char *>("preLogAdmission.policy");
            std::cout << "Creating admission policy of type " << policyType << std::endl;
            policyType.append(".preLogAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preLogAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            _prelog_admission = admission::Policy::create(admission_settings, nullptr, _log, admission_stats);
        }

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        {
            warmed_up = true;
        }
        assert(warmed_up);
    }

    MemLogCache::~MemLogCache()
    {
        delete _log;
        delete _memCache;
        delete _prelog_admission;
    }

    void MemLogCache::insert(candidate_t id)
    {
        std::vector<candidate_t> ret = _memCache->insert(id); // 将对象插入到内存缓存，返回驱逐的对象列表
        if (warmed_up && _prelog_admission)
        {
            ret = _prelog_admission->admit_simple(ret); // 对内存缓存中驱逐的对象应用准入策略后准入的对象列表
        }
        ret = _log->insert(ret); // 插入flash日志记录缓存

        /* check warmed up condition every so often */
        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    bool MemLogCache::find(candidate_t id)
    {
        // 分别查找内存缓存和flash日志缓存
        if (_memCache->find(id) || _log->find(id))
        {
            return true;
        }
        return false;
    }

    double MemLogCache::calcFlashWriteAmp()
    { // 计算写放大
        double flash_write_amp = _log->calcWriteAmp();
        if (warmed_up && _prelog_admission)
        {
            /* include all bytes not written to structs */
            return flash_write_amp * _prelog_admission->byteRatioAdmitted();
        }
        else
        {
            return flash_write_amp;
        }
    }

    void MemLogCache::checkWarmup()
    {
        // 当flash日志缓存的累计驱逐大小超过容量时，预热结束
        if (_log->ratioEvictedToCapacity() < 1)
        {
            return;
        }
        flushStats(); // 总体的统计信息
        std::cout << "Reached end of warmup, resetting stats\n\n";
        _log->flushStats();      // flash日志缓存统计信息
        _memCache->flushStats(); // 内存缓存统计信息
        dumpStats();             // 将统计信息写入输出文件
        warmed_up = true;
    }

} // namespace cache
