#pragma once

#include <assert.h>
#include <iostream>
#include <unordered_map>

#include "parsers/parser.hpp"

struct Block
{
  uint64_t _lba;
  int64_t _size;
  static inline int64_t _capacity = 4096;
  mutable int hit_count;
  int64_t oracle_count;
  bool is_dirty;

  static Block make(const parser::Request &req)
  { // 没有用到请求的num
    return Block{._lba = req.id, ._size = req.req_size, .hit_count = 0, .oracle_count = req.oracle_count};
  }

  inline bool operator==(const Block &that) const
  {
    return (_lba == that._lba);
  }

  inline bool operator!=(const Block &that) const
  {
    return !(operator==(that));
  }

  inline bool operator<(const Block &that) const
  {
    return this->_lba < that._lba;
  }
};

template <typename T>
class BlockMap : public std::unordered_map<Block, T>
{
public:
  typedef std::unordered_map<Block, T> Base;
  const T DEFAULT;

  BlockMap(const T &_DEFAULT)
      : Base(), DEFAULT(_DEFAULT) {}

  using typename Base::const_reference;
  using typename Base::reference;

  T &operator[](Block c)
  {
    auto itr = Base::find(c);
    if (itr == Base::end())
    {
      auto ret = Base::insert({c, DEFAULT});
      assert(ret.second);
      return ret.first->second;
    }
    else
    {
      return itr->second;
    }
  }

  const T &operator[](Block c) const
  {
    auto itr = Base::find(c);
    if (itr == Base::end())
    {
      return DEFAULT;
    }
    else
    {
      return itr->second;
    }
  }
};

// Block specializations
namespace std
{

  template <>
  struct hash<Block>
  {
    size_t operator()(const Block &x) const
    {
      return x._lba;
    }
  };

}

namespace
{

  inline std::ostream &operator<<(std::ostream &os, const Block &x)
  {
    return os << "(" << x._lba << ")";
  }

}
