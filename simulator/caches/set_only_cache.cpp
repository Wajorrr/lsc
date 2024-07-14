#include "caches/set_only_cache.hpp"
#include "config_reader.hpp"
#include "constants.hpp"
#include "kangaroo/lru.hpp"
#include "kangaroo/rrip_sets.hpp"
#include "kangaroo/sets.hpp"
#include "stats/stats.hpp"

namespace cache
{

    SetOnlyCache::SetOnlyCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : Cache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        /* Initialize flash cache */
        uint64_t set_capacity = (uint64_t)cfg.read<int>("sets.setCapacity");   // 集合大小
        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 缓存容量
        uint64_t num_sets = (flash_size_mb * 1024 * 1024) / set_capacity;      // 集合数量
        int num_hash_functions = cfg.read<int>("sets.numHashFunctions", 1);    // 哈希函数数量
        auto &set_stats = statsCollector->createLocalCollector("sets");
        if (cfg.exists("sets.rripBits"))
        {
            int rrip_bits = cfg.read<int>("sets.rripBits");                // rrip中每个缓存项使用的rrpv位数
            bool promotion = (bool)cfg.read<int>("sets.promotionOnly", 0); // 若为true，则只要对象被访问过，在插入时，或被再次访问时，其rrpv都重置为0
            bool mixed = (bool)cfg.read<int>("sets.mixedRRIP", 0);         // 若为true，则对象被再次访问时，rrpv重置为0，在插入时，按其访问次数决定rrpv
            _sets = new flashCache::RripSets(num_sets, set_capacity, set_stats, nullptr,
                                             num_hash_functions, rrip_bits, promotion, mixed);
        }
        else
        {
            bool sets_track_hits = cfg.exists("sets.trackHitsPerItem"); // 是否启用NRU策略，若为false，则默认为fifo
            _sets = new flashCache::Sets(num_sets, set_capacity, set_stats, nullptr, num_hash_functions, sets_track_hits);
        }

        /* Initialize memory cache */
        // 计算rrip集合为每个对象维护元数据的内存消耗
        uint64_t sets_memory_consumption = _sets->calcMemoryConsumption();
        uint64_t memory_size = (uint64_t)cfg.read<int>("cache.memorySizeMB") * 1024 * 1024;
        assert(sets_memory_consumption <= memory_size);
        memory_size -= sets_memory_consumption; // 减去元数据之后剩余内存容量
        auto &memory_cache_stats = statsCollector->createLocalCollector("memCache");
        assert(!strcmp(cfg.read<const char *>("memoryCache.policy"), "LRU"));
        _memCache = new memcache::LRU(memory_size, memory_cache_stats); // 内存缓存，LRU策略

        /* Initialize preset admission policy */
        if (cfg.exists("preSetAdmission"))
        {
            // 准入策略
            std::string policyType = cfg.read<const char *>("preSetAdmission.policy");
            policyType.append(".preSetAdmission");
            const libconfig::Setting &admission_settings = cfg.read<libconfig::Setting &>("preSetAdmission");
            auto &admission_stats = statsCollector->createLocalCollector(policyType);
            _preset_admission = admission::Policy::create(admission_settings, _sets, nullptr, admission_stats);
        }

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        {
            warmed_up = true;
        }
    }

    SetOnlyCache::~SetOnlyCache()
    {
        delete _sets;
        delete _memCache;
        delete _preset_admission;
    }

    // 将对象插入内存缓存，驱逐的对象插入闪存缓存
    void SetOnlyCache::insert(candidate_t id)
    {
        // 先将对象插入到内存缓存，获取从内存缓存中驱逐的一批对象
        std::vector<candidate_t> ret = _memCache->insert(id);
        if (warmed_up && _preset_admission) // 缓存预热完毕并且启用了准入策略
        {
            // 进行准入判断，返回一组set，表示各个set中被准入的对象
            auto admitted = _preset_admission->admit(ret);
            // 遍历set，将准入的对象插入到flash cache中
            for (auto set : admitted)
            {
                // 向指定set插入一批对象
                _sets->insert(set.first, set.second);
            }
        }
        else if (ret.size() != 0) // 否则直接将对象插入到flash cache中
        {
            _sets->insert(ret);
        }

        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    // 查找对象是否在内存缓存/闪存缓存中
    bool SetOnlyCache::find(candidate_t id)
    {
        return _memCache->find(id) || _sets->find(id);
    }

    double SetOnlyCache::calcFlashWriteAmp()
    {
        double set_write_amp = _sets->calcWriteAmp();
        if (warmed_up && _preset_admission)
        {
            // 准入的写入字节数/请求的写入字节数=准入比例
            // 实际写入字节数/准入的写入字节数=写放大比例
            // 准入比例*写放大比例=实际写入字节数/请求的写入字节数
            // 将写放大比例乘以准入比例，得到实际写放大比例
            set_write_amp *= _preset_admission->byteRatioAdmitted();
        }
        return set_write_amp;
    }

    void SetOnlyCache::checkWarmup()
    {
        // 若驱逐对象的总大小达到闪存容量大小，预热结束
        if (_sets->ratioEvictedToCapacity() < 1)
        {
            return;
        }
        flushStats();
        std::cout << "Reached end of warmup, resetting stats\n\n";
        _sets->flushStats();
        _memCache->flushStats();
        dumpStats();
        warmed_up = true;
    }

} // namespace cache
