#include "caches/mem_only_cache.hpp"
#include "config_reader.hpp"
#include "kangaroo/lru.hpp"
#include "stats/stats.hpp"

namespace cache
{

    MemOnlyCache::MemOnlyCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : Cache(sc, gs, settings)
    {
        misc::ConfigReader cfg(settings);

        /* Initialize memory cache */
        uint64_t memory_size_mb = (uint64_t)cfg.read<int>("cache.memorySizeMB");     // 内存缓存大小
        auto &memory_cache_stats = statsCollector->createLocalCollector("memCache"); // 本地统计信息收集器，命名为memCache
        assert(!strcmp(cfg.read<const char *>("memoryCache.policy"), "LRU"));
        _memCache = new memcache::LRU(memory_size_mb * 1024 * 1024, memory_cache_stats); // 缓存大小单位为字节
    }

    MemOnlyCache::~MemOnlyCache()
    {
        delete _memCache;
    }

    void MemOnlyCache::insert(candidate_t id)
    {
        std::vector<candidate_t> ret = _memCache->insert(id);
    }

    bool MemOnlyCache::find(candidate_t id)
    {
        return _memCache->find(id);
    }

    double MemOnlyCache::calcFlashWriteAmp()
    {
        return 0;
    }

} // namespace cache
