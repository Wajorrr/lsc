#pragma once

#include <vector>
#include "cacheAlgo/cache_obj.h"

namespace CacheAlgo
{
    class CacheAlgoAbstract
    {
    public:
        virtual ~CacheAlgoAbstract() = default;

        virtual bool get(obj_id_t id) = 0;

        virtual std::vector<uint64_t> set(obj_id_t id, int64_t size) = 0;

        // virtual cache_obj_t evict(cache_obj_t item) = 0;
    };
}