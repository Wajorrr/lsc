#include <fstream>
#include <iostream>

#include "lib/json.hpp"
#include "stats.hpp"
#include "common/logging.h"

namespace stats
{

    LocalStatsCollector::LocalStatsCollector(StatsCollector &parent) : _parent(parent) {}

    void LocalStatsCollector::addChild(std::string name, LocalStatsCollector *child)
    {
        children[name] = child;
    }

    LocalStatsCollector &LocalStatsCollector::createChild(std::string name)
    {
        LocalStatsCollector *child = new LocalStatsCollector(_parent);
        children[name] = child;
        return *child;
    }

    json LocalStatsCollector::toJson()
    {
        json j;
        for (const auto &counter : counters)
        {
            // j["counters"][counter.first] = counter.second;
            j[counter.first] = counter.second;
        }
        for (const auto &child : children)
        {
            j["children"][child.first] = child.second->toJson();
        }
        return j;
    }

    void LocalStatsCollector::print()
    {
        _parent.print();
    }

    int64_t &LocalStatsCollector::operator[](std::string name)
    {
        return counters[name];
    }

    StatsCollector::StatsCollector(std::string output_filename)
    {
        DEBUG("Stats file at %s\n", output_filename.c_str());
        // std::cout << "Stats file at " << output_filename << std::endl;
        _outputFile.open(output_filename);
    }

    StatsCollector::~StatsCollector()
    {
        auto itr = locals.begin();
        while (itr != locals.end())
        {
            LocalStatsCollector *local_stats = itr->second;
            itr = locals.erase(itr);
            delete local_stats;
        }
        _outputFile.close();
    }

    void StatsCollector::print()
    {
        // INFO("Printing stats\n");
        json blob;
        for (const auto &local : locals)
        {
            // blob[local.first] = local.second->counters;
            blob[local.first] = local.second->toJson();
        }
        // 使用 json 类的 dump 函数将 blob 转换为字符串，并将其写入 _outputFile。
        // dump 函数的参数 PRETTY_JSON_SPACES 指定了在格式化 JSON 时每个级别应该缩进的空格数。这样可以使输出的 JSON 更易于阅读
        _outputFile << blob.dump(PRETTY_JSON_SPACES) << std::endl;
    }

    LocalStatsCollector &StatsCollector::createLocalCollector(std::string name)
    {
        locals[name] = new LocalStatsCollector(*this);
        return *locals[name];
    }

} // namespace stats
