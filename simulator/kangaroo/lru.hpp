#pragma once

#include <unordered_map>
#include "stats/stats.hpp"
#include "mem_cache.hpp"

namespace memcache
{

  template <typename DataT>
  struct List // 双向链表
  {
    typedef DataT Data;

    struct Entry // 链表节点
    {
      Data data;
      Entry *prev;
      Entry *next;

      void remove()
      {
        assert(prev != nullptr);
        prev->next = next;
        assert(next != nullptr);
        next->prev = prev;
      }
    };

    List()
        : _head(new Entry{Data(), nullptr, nullptr}), _tail(new Entry{Data(), nullptr, nullptr})
    { // 单独的头指针和尾指针，不存储数据
      _head->next = _tail;
      _tail->prev = _head;
    }

    ~List()
    {
      Entry *entry = _head;

      while (entry)
      {
        auto *next = entry->next;
        delete entry;
        entry = next;
      }
    }

    // 插入首部
    void insert_front(Entry *entry)
    {
      entry->prev = _head;
      entry->next = _head->next;
      _head->next->prev = entry;
      _head->next = entry;
    }

    // 插入尾部
    void insert_back(Entry *entry)
    {
      entry->prev = _tail->prev;
      entry->next = _tail;
      _tail->prev->next = entry;
      _tail->prev = entry;
    }

    Data &front()
    {
      return _head->next->data;
    }

    Data &back()
    {
      return _tail->prev->data;
    }

    bool empty() const
    {
      return _head->next == _tail;
    }

    Entry *begin() const
    {
      return _head->next;
    }

    Entry *end() const
    {
      return _tail;
    }

    Entry *_head, *_tail;
  };

  template <typename Data>
  struct Tags
      : public std::unordered_map<candidate_t, typename List<Data>::Entry *> // 对象到链表节点的映射
  {
    typedef typename List<Data>::Entry Entry;

    Entry *lookup(candidate_t id) const
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

    Entry *allocate(candidate_t id) // 为对象创建一个新链表节点
    {
      auto *entry = new Entry{id, nullptr, nullptr};
      (*this)[id] = entry;
      return entry;
    }

    Entry *evict(candidate_t id) // 驱逐对象
    {
      auto itr = this->find(id);
      assert(itr != this->end());

      auto *entry = itr->second;
      this->erase(itr); // 删除对象索引
      return entry;     // 返回链表节点
    }
  };

  class LRU : public virtual MemCache
  {
  public:
    LRU(uint64_t _max_size, stats::LocalStatsCollector &mem_stats)
        : max_size(_max_size), current_size(0), _mem_stats(mem_stats)
    {
      _mem_stats["lruCacheCapacity"] = _max_size; // 缓存容量，单位为字节
    }

    void update(candidate_t id)
    {
      auto *entry = tags.lookup(id);
      if (entry) // 从原位置删除
      {
        entry->remove();
      }
      else
      {
        entry = tags.allocate(id); // 新分配一个链表节点
      }

      list.insert_front(entry); // 插入头部
    }

    void replaced(candidate_t id) // 驱逐给定的对象
    {
      auto *entry = tags.evict(id); // 先删除索引
      current_size -= id.obj_size;  // 当前缓存大小
      entry->remove();              // 再删除链表节点
      delete entry;
    }

    candidate_t rank()
    {
      return list.back(); // 返回链表尾部的对象
    }

    std::vector<candidate_t> insert(candidate_t id)
    {
      std::vector<candidate_t> evicted;
      if ((uint64_t)id.obj_size > max_size) // 对象大小大于内存缓存大小，直接下刷到flash
      {
        _mem_stats["numEvictions"]++;
        _mem_stats["sizeEvictions"] += id.obj_size;
        evicted.push_back(id);
        return evicted;
      }

      while (current_size + id.obj_size > max_size) // 驱逐直到空间足够容纳当前对象
      {
        candidate_t evict_id = rank(); // 驱逐LRU尾部对象
        _mem_stats["numEvictions"]++;
        _mem_stats["sizeEvictions"] += evict_id.obj_size;
        evicted.push_back(evict_id);
        replaced(evict_id);
      }
      update(id); // 将对象插入到链表头部
      current_size += id.obj_size;
      _mem_stats["current_size"] = current_size;
      assert(current_size <= max_size);
      return evicted;
    }

    bool find(candidate_t id) // 请求一个对象
    {
      auto *entry = tags.lookup(id);
      if ((bool)entry)
      {
        update(id);
        _mem_stats["hits"]++;
      }
      else
      {
        _mem_stats["misses"]++;
      }
      return (bool)entry;
    }

    void flushStats() // 刷新统计信息
    {
      _mem_stats["hits"] = 0;
      _mem_stats["misses"] = 0;
      _mem_stats["numEvictions"] = 0;
      _mem_stats["sizeEvictions"] = 0;
    }

  private:
    List<candidate_t> list; // 一个双向链表
    Tags<candidate_t> tags; // 一个hash表
    typedef typename List<candidate_t>::Entry Entry;
    uint64_t max_size;
    uint64_t current_size;
    stats::LocalStatsCollector &_mem_stats;
  };

} // namespace memcache
