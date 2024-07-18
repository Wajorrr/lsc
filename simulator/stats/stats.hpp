#pragma once

#include <fstream>
#include <memory>
#include <unordered_map>

#include "lib/json.hpp"

#define PRETTY_JSON_SPACES 4

namespace stats
{

    // nlohmann::json 是一个非常流行的 C++ JSON 库，它提供了一种简单和直观的方式来处理 JSON 数据
    using json = nlohmann::json;

    class StatsCollector;

    class LocalStatsCollector
    {
    public:
        int64_t &operator[](std::string name);
        void print();
        friend class StatsCollector;
        ~LocalStatsCollector() {}

        void addChild(std::string name, LocalStatsCollector *child); // 添加子LocalStatsCollector
        LocalStatsCollector &createChild(std::string name);          // 创建子LocalStatsCollector

        json toJson();

    private:
        std::unordered_map<std::string, int64_t> counters;     // 计数
        std::map<std::string, LocalStatsCollector *> children; // 存储子LocalStatsCollector的容器
        LocalStatsCollector(StatsCollector &parent);
        StatsCollector &_parent;
    };

    class StatsCollector
    {
    public:
        StatsCollector(std::string output_filename);
        ~StatsCollector();
        LocalStatsCollector &createLocalCollector(std::string name);
        void print();

    private:
        std::ofstream _outputFile;
        std::unordered_map<std::string, LocalStatsCollector *> locals;
    };

} // namespace stats
