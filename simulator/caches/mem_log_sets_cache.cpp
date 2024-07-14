#include <cmath>

#include "caches/mem_log_sets_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "kangaroo/log.hpp"
#include "kangaroo/lru.hpp"
#include "kangaroo/rotating_log.hpp"
#include "kangaroo/rrip_sets.hpp"
#include "kangaroo/sets.hpp"
#include "stats/stats.hpp"

namespace cache
{

    MemLogSetsCache::MemLogSetsCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : Cache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        /* Initialize flash cache, split size among log and
         * sets so that sets are always a multiple of setCapacity
         * and close to provided percentLog, leans toward more log */
        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存缓存大小
        double log_percent = (double)cfg.read<float>("log.percentLog") / 100.; // 日志缓存比例
        uint64_t set_capacity = (uint64_t)cfg.read<int>("sets.setCapacity");   // 集合缓存比例
        uint64_t flash_size = flash_size_mb * 1024 * 1024;
        double exact_set_capacity = flash_size * (1 - log_percent); // 集合缓存容量
        // 将集合缓存容量减去其除以集合大小之后得到的余数，使得集合缓存容量为集合大小的整数倍
        uint64_t actual_set_capacity = exact_set_capacity - std::fmod(exact_set_capacity, set_capacity);
        uint64_t log_capacity = flash_size - actual_set_capacity; // 日志缓存容量
        if (cfg.exists("log.adjustFlashSizeUp"))                  // ？
        {
            actual_set_capacity += (log_capacity / 2);
        }
        std::cout << "Desired Percent Log: " << log_percent
                  << " Actual Percent Log: " << log_capacity / (double)flash_size
                  << std::endl;

        auto &set_stats = statsCollector->createLocalCollector("sets");
        uint64_t num_sets = actual_set_capacity / set_capacity;             // 集合数量
        int num_hash_functions = cfg.read<int>("sets.numHashFunctions", 1); // 哈希函数数量
        if (cfg.exists("sets.rripBits"))
        {
            int rrip_bits = cfg.read<int>("sets.rripBits");                // rrip中为每个对象维护的位数
            bool promotion = (bool)cfg.read<int>("sets.promotionOnly", 0); // promotion模式
            bool mixed = (bool)cfg.read<int>("sets.mixedRRIP", 0);         // mixed模式
            _sets = new flashCache::RripSets(num_sets, set_capacity, set_stats, this,
                                             num_hash_functions, rrip_bits, promotion, mixed);
        }
        else
        {
            bool sets_track_hits = cfg.exists("sets.trackHitsPerItem"); // 是否启用NRU策略，若不启用，则默认为FIFO
            _sets = new flashCache::Sets(num_sets, set_capacity, set_stats, this, num_hash_functions, sets_track_hits);
        }
        if (cfg.exists("sets.hitDistribution"))
        {
            std::cout << "CALLED enable set hits distribution" << std::endl;
            _sets->enableHitDistributionOverSets();
        }

        /* figure out what log version to use and initialize it */
        uint64_t readmit = cfg.read<int>("log.readmit", 0);
        if (cfg.exists("log.flushBlockSizeKB")) // 若设置了flushBlockSizeKB，则为RotatingLog，下刷时将同一集合的块一起下刷
        {
            auto &log_stats = statsCollector->createLocalCollector("log");
            uint64_t block_size = 1024 * (uint64_t)cfg.read<int>("log.flushBlockSizeKB"); // 日志块大小，乘以1024，单位为字节
            _log = new flashCache::RotatingLog(log_capacity, block_size, _sets, log_stats, readmit);
        }
        else // 否则为Log，下刷时将所有对象一起下刷
        {
            auto &log_stats = statsCollector->createLocalCollector("log");
            _log = new flashCache::Log(log_capacity, log_stats, readmit);
        }

        /* Initialize memory cache, size takes into account indexing overhead
         * of index to log, and set nru tracking */
        // 计算集合缓存rrpv的内存消耗
        uint64_t sets_memory_consumption = _sets->calcMemoryConsumption();
        // 总内存大小
        uint64_t memory_size = (uint64_t)cfg.read<int>("cache.memorySizeMB") * 1024 * 1024;
        // 日志缓存索引的内存消耗比例
        double perc_mem_log_overhead = cfg.read<float>("cache.memOverheadRatio", INDEX_LOG_RATIO);
        assert(log_capacity * perc_mem_log_overhead + sets_memory_consumption <= memory_size);
        // 计算剩余内存缓存容量
        uint64_t mem_cache_capacity = memory_size - (log_capacity * perc_mem_log_overhead);
        mem_cache_capacity -= sets_memory_consumption;
        std::cout << "Actual Memory Cache Size after indexing costs: "
                  << mem_cache_capacity << std::endl;
        auto &memory_cache_stats = statsCollector->createLocalCollector("memCache");
        // 内存缓存，LRU
        _memCache = new memcache::LRU(mem_cache_capacity, memory_cache_stats);

        /* Initialize prelog admission policy */
        if (cfg.exists("preLogAdmission"))
        { // 用于日志缓存的准入策略，文中提到默认为概率准入
            std::string policyType = cfg.read<const char *>("preLogAdmission.policy");
            policyType.append(".preLogAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preLogAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            _prelog_admission = admission::Policy::create(admission_settings, _sets, _log, admission_stats);
        }

        /* Initialize preset admission policy */
        if (cfg.exists("preSetAdmission"))
        { // 用于集合缓存的准入策略，文中提到默认为阈值准入，同一集合的下刷对象超过阈值时下刷
            std::string policyType = cfg.read<const char *>("preSetAdmission.policy");
            policyType.append(".preSetAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preSetAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            _preset_admission = admission::Policy::create(admission_settings, _sets, _log, admission_stats);
        }

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        { // 缓存预热
            warmed_up = true;
        }

        /* distribution num objects per set from log */
        if (cfg.exists("cache.recordSetDistribution"))
        {
            _record_dist = true;
            if (warmed_up)
            {
                _sets->enableDistTracking();
            }
        }
    }

    MemLogSetsCache::~MemLogSetsCache()
    {
        delete _sets;
        delete _log;
        delete _memCache;
        delete _prelog_admission;
        delete _preset_admission;
    }

    void MemLogSetsCache::insert(candidate_t id)
    {
        // 首先将对象插入到内存缓存中，返回驱逐的对象集合，插入到flashcache中
        std::vector<candidate_t> ret = _memCache->insert(id);
        // 若预热完毕且设置了日志缓存准入策略，则根据准入策略对对象进行处理
        if (warmed_up && _prelog_admission)
        {
            // 按所属集合返回被准入的对象
            auto admitted = _prelog_admission->admit(ret);
            ret.clear();
            for (auto set : admitted)
            {
                ret.insert(ret.end(), set.second.begin(), set.second.end());
            }
        }
        ret = _log->insert(ret); // 将驱逐的对象插入到日志闪存缓存中，返回从日志缓存中驱逐的对象
        if (ret.size())          // 若有对象被驱逐，则插入到集合缓存中
        {
            // 若预热完毕且设置了集合缓存准入策略，则根据准入策略对对象进行处理
            if (warmed_up && _preset_admission)
            {
                auto admitted = _preset_admission->admit(ret);
                for (auto set : admitted) // 按集合进行准入
                {
                    if (_record_dist)
                    {
                        // 记录当前准入集合中对象的数量和大小
                        std::string num_name = "numItemsMoved" + std::to_string(set.second.size());
                        uint64_t size = std::accumulate(set.second.begin(),
                                                        set.second.end(), 0,
                                                        [](uint64_t sum, candidate_t next)
                                                        { return (sum + next.obj_size); });
                        // 将对象总大小按SIZE_BUCKETING取整
                        size = (size / SIZE_BUCKETING) * SIZE_BUCKETING;
                        std::string size_name = "sizeItemsMoved" + std::to_string(size);
                        globalStats[num_name]++;
                        globalStats[size_name]++;
                    }
                    // 将准入的对象集合插入到集合缓存中
                    ret = _sets->insert(set.first, set.second);
                }
            }
            else
            { // 预热没结束，或者没有设置集合缓存准入策略，直接插入
                ret = _sets->insert(ret);
            }
        }

        /* check warmed up condition every so often */
        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    bool MemLogSetsCache::find(candidate_t id)
    {
        // 依次在内存缓存，闪存日志缓存，闪存集合缓存中查找
        if (_memCache->find(id) || _log->find(id) || _sets->find(id))
        {
            return true;
        }
        return false;
    }

    double MemLogSetsCache::calcFlashWriteAmp()
    {
        // 首先计算集合缓存的写放大
        double set_write_amp = _sets->calcWriteAmp();
        if (warmed_up && _preset_admission)
        {
            // 集合缓存的写放大乘以集合缓存的准入率
            set_write_amp *= _preset_admission->byteRatioAdmitted();
        }
        // 再加上日志缓存的写放大
        double flash_write_amp = set_write_amp + _log->calcWriteAmp();
        if (warmed_up && _prelog_admission)
        {
            // 最后乘以日志缓存的准入率
            /* include all bytes not written to structs */
            return flash_write_amp * _prelog_admission->byteRatioAdmitted();
        }
        else
        {
            return flash_write_amp;
        }
    }

    void MemLogSetsCache::checkWarmup()
    {
        // 当集合缓存的驱逐对象总大小达到集合缓存的容量时，预热完毕
        if (_sets->ratioEvictedToCapacity() < 1)
        {
            return;
        }
        flushStats();
        std::cout << "Reached end of warmup, resetting stats\n\n";
        _sets->flushStats();
        _log->flushStats();
        _memCache->flushStats();
        dumpStats();
        warmed_up = true;
        _sets->enableDistTracking();
    }

} // namespace cache
