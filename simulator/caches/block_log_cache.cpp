#include "block_log_cache.hpp"

#include <cmath>

#include "common/logging.h"
#include "config_reader.hpp"
#include "constants.hpp"
#include "stats/stats.hpp"

namespace cache {

BlockLogCache::BlockLogCache(stats::StatsCollector *sc, stats::LocalStatsCollector &gs, const libconfig::Setting &settings) : BlockCache(sc, gs, settings) {
    misc::ConfigReader cfg(settings);

    uint64_t flash_size_mb = (uint64_t)cfg.read<int>("cache.flashSizeMB");  // 闪存容量
    uint64_t log_capacity = flash_size_mb * 1024 * 1024;

    std::string log_type = cfg.read<const char *>("log.logType");
    auto &log_stats = statsCollector->createLocalCollector("log");

    if (log_type == "FIFOLog") {
        DEBUG("Creating FIFOLog cache\n");
        _log = new flashCache::BlockLog(log_capacity, log_stats);
    } else if (log_type == "mFIFOLog") {
        DEBUG("Creating mFIFOLog cache\n");
        _log = new flashCache::mBlockLog(log_capacity, log_stats);
    } else if (log_type == "RIPQ") {
        DEBUG("Creating RIPQ cache\n");
        _log = new flashCache::BlockRIPQ(log_capacity, log_stats);
    } else {
        ERROR("Unknown log type %s\n", log_type.c_str());
        abort();
    }

    /* slow warmup */
    if (cfg.exists("cache.slowWarmup")) {
        warmed_up = true;
    }
    assert(warmed_up);
}

BlockLogCache::~BlockLogCache() {
    delete _log;
    // delete _prelog_admission;
}

void BlockLogCache::insert(const parser::Request *req) {
    Block id = Block::make(*req);
    if (req->type == parser::OP_SET)
        id.is_dirty = true;
    _log->insert({id});  // 插入flash日志记录缓存
    /* check warmed up condition every so often */
    if (!warmed_up && getAccessesAfterFlush() % CHECK_WARMUP_INTERVAL == 0) {
        checkWarmup();
    }
}

bool BlockLogCache::find(const parser::Request *req) {
    // 分别查找内存缓存和flash日志缓存
    bool updateStats = false;
    if (req->type == parser::OP_GET)
        updateStats = true;
    if (_log->find(req->id, updateStats)) {
        return true;
    }
    return false;
}

void BlockLogCache::update(const parser::Request *req) {
    DEBUG("update %lu\n", req->id);
    Block id = Block::make(*req);
    id.is_dirty = true;
    _log->update({id});
}

}  // namespace cache
