#include "block_rw_partition_cache.hpp"
#include "block_cache.hpp"

#include <cmath>

#include "common/logging.h"
#include "config_reader.hpp"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace cache
{

    BlockRWPartitionCache::BlockRWPartitionCache(stats::StatsCollector *sc,
                                                 stats::LocalStatsCollector &gs,
                                                 const libconfig::Setting &settings)
        : BlockCache(sc, gs, settings) // 调用基类构造函数
    {
        misc::ConfigReader cfg(settings);

        //
        uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB"); // 闪存容量
        uint64_t log_capacity = flash_size_mb * 1024 * 1024;

        auto &log_stats = statsCollector->createLocalCollector("log");

        read_percent = cfg.read<double>("cache.readPercent", 90.0);

        read_cache = new BlockGCCache(sc, gs, settings, true);
        write_cache = new BlockGCCache(sc, gs, settings, false);
        calcCapacityUtilization();
        DEBUG("read cache config:\n");
        read_cache->print_config();
        DEBUG("write cache config:\n");
        write_cache->print_config();
        calcCapacityUtilization();

        /* slow warmup */
        if (cfg.exists("cache.slowWarmup"))
        {
            warmed_up = true;
        }
        assert(warmed_up);
    }

    BlockRWPartitionCache::~BlockRWPartitionCache()
    {
        delete read_cache;
        delete write_cache;
        // delete _log;
        // delete _prelog_admission;
    }

    void BlockRWPartitionCache::insert(const parser::Request *req)
    {
        if (req->type == parser::OP_SET)
        {
            // DEBUG("write insert\n");
            write_cache->insert(req);
        }
        else
        {
            // DEBUG("read insert\n");
            read_cache->insert(req);
        }
        // _log->insert({id}); // 插入flash日志记录缓存
        /* check warmed up condition every so often */
        if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0)
        {
            checkWarmup();
        }
    }

    bool BlockRWPartitionCache::find(const parser::Request *req)
    {
        // 分别查找读取缓存和写入缓存
        if (read_cache->find(req) || write_cache->find(req))
        {
            return true;
        }
        return false;
    }

    void BlockRWPartitionCache::update(const parser::Request *req)
    {
        int flag = 1;
        if (write_cache->find(req))
        {
            flag = 2;
        }
        if (flag == 1)
        {
            // DEBUG("read update %lu\n", req->id);
            read_cache->update(req);
        }
        else
        {
            // DEBUG("write update %lu\n", req->id);
            write_cache->update(req);
            // write_cache->calcCapacityUtilization();
        }
    }

    double BlockRWPartitionCache::calcFlashWriteAmp()
    { // 计算写放大

        double flash_write_amp = read_percent / 100 * read_cache->calcFlashWriteAmp() +
                                 (1 - read_percent / 100) * write_cache->calcFlashWriteAmp();
        return flash_write_amp;
    }

    double BlockRWPartitionCache::calcCapacityUtilization()
    {
        // DEBUG("read\n");
        double read_util = read_cache->calcCapacityUtilization();
        // DEBUG("write\n");
        double write_util = write_cache->calcCapacityUtilization();
        DEBUG("read_percent:%lf, read_util:%lf,write_util:%lf\n", read_percent, read_util, write_util);
        double utilization = read_percent / 100 * read_cache->calcCapacityUtilization() +
                             (1 - read_percent / 100) * write_cache->calcCapacityUtilization();
        return utilization;
    }

} // namespace cache
