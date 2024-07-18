#pragma once

#include <unordered_map>
#include "cache_obj.h"
#include "parsers/parser.hpp"

struct Tags
    : public std::unordered_map<obj_id_t, cache_obj_t *> // 对象到链表节点的映射
{
    cache_obj_t *hashtable_find(const parser::Request *req)
    {
        return lookup(req->id);
    }

    cache_obj_t *hashtable_find_obj_id(const obj_id_t id)
    {
        return lookup(id);
    }

    cache_obj_t *hashtable_insert(const parser::Request *req)
    {
        return allocate(req->id, req->req_size);
    }

    void hashtable_delete(cache_obj_t *obj)
    {
        evict(obj->obj_id);
        free_cache_obj(obj);
    }

    void free_hashtable()
    {
        for (auto &kv : *this)
        {
            free_cache_obj(kv.second);
        }
        this->clear();
    }

    cache_obj_t *lookup(obj_id_t id) const
    {
        auto itr = this->find(id); // 在hash表中查找对象
        if (itr != this->end())
        {
            return itr->second;
        }
        else
        {
            return nullptr;
        }
    }

    cache_obj_t *allocate(obj_id_t id, int64_t size) // 为对象创建一个新链表节点
    {
        auto *entry = new cache_obj_t(id, size);
        (*this)[id] = entry;
        return entry;
    }

    cache_obj_t *evict(obj_id_t id) // 驱逐对象
    {
        auto itr = this->find(id);
        assert(itr != this->end());

        auto *entry = itr->second;
        this->erase(itr); // 删除对象索引
        return entry;     // 返回链表节点
    }
};