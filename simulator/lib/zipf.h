#include <math.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <random>
#include "common/logging.h"

/*
 sampling object id and object size from a Zipf-like distribution
(aka the independent reference model (IRM))

 Multiple objects with similar rates are grouped together for more efficient sampling.
 Two level sampling process: first skewed sample to select the rate, then uniform sample within rate to select object.

 从类似Zipf的分布中采样对象ID和对象大小，这是一种被称为独立引用模型（IRM）的方法。

 Zipf分布是一种离散概率分布，它的特点是少数几个事件的发生频率非常高，而大多数事件的发生频率非常低。
 在这种分布中，第n个最常见的事件的发生频率与n的倒数成正比。这种分布在许多自然和社会现象中都有出现，例如词频、城市人口等。

 在这个模型中，首先对频率进行倾斜采样以选择频率，然后在频率内进行均匀采样以选择对象。
 这样做的原因是，对于Zipf分布，大多数对象的采样频率非常低，而少数几个对象的采样频率非常高。
 如果直接对所有对象进行采样，那么大多数时间都会采样到频率低的对象，而频率高的对象则很少被采样到。
 通过这种两级采样过程，可以更有效地模拟Zipf分布，即在保证采样效率的同时，也能保证采样结果的准确性。

 此外，这个模型还将具有相似频率的多个对象组合在一起，以提高采样效率。
 这是因为，对于频率相似的对象，它们被采样的概率也相似，因此可以将它们视为一个整体进行采样，这样可以减少采样次数，提高采样效率。

*/

class ZipfRequests
{

private:
    // 对象ID和大小的对
    typedef std::pair<int64_t, int64_t> IdSizePair;

    std::mt19937_64 rd_gen;
    // 流行度分布桶popDist，将对象按频率总和分组，举例：若最后共分组出了3个桶
    // 将对象按频率排序后，顺序遍历对象，频率累加，频率每加到总和的1/3，就分出一个组
    std::vector<std::vector<IdSizePair>> popDist;
    // 均匀分布，用于在不同桶之间均匀采样
    std::uniform_int_distribution<size_t> popOuterRng;
    // 均匀分布，在同一个桶内均匀采样
    std::vector<std::uniform_int_distribution<size_t>> popInnerRngs;

    // std::unordered_map<int64_t, IdSizePair> -> <对象ID,<对象大小,对象被采样的次数>>
    std::unordered_map<int64_t, IdSizePair> stats;

public:
    // sizeFile: 对象大小文件，存储可能的对象大小
    // objCount: 对象数量，zipfAlpha: zipf分布的参数，min_size: 流行度桶的最小大小
    ZipfRequests(std::string sizeFile, uint64_t objCount, double zipfAlpha, uint64_t min_size)
    {
        objCount *= 1000;
        std::cout << "zipf objCount " << objCount << std::endl;

        // init
        rd_gen.seed(1);

        // try to scan object sizes from file
        std::vector<int64_t> base_sizes;
        // 指定了对象大小文件，从文件中读取对象大小
        if (sizeFile.size() > 0)
        {
            std::ifstream infile;
            infile.open(sizeFile);
            int64_t tmp;
            while (infile >> tmp)
            {
                if (tmp > 0 && tmp < 4096)
                {
                    base_sizes.push_back(tmp);
                }
            }
            infile.close();
        }
        // 文件为空/不存在，使用默认的对象大小64
        if (base_sizes.size() == 0)
        {
            // base_sizes = std::vector<int64_t>({64});
            base_sizes = std::vector<int64_t>({4096});
        }
        // 均匀分布，将用于随机选择对象的大小
        std::uniform_int_distribution<uint64_t> size_dist(0, base_sizes.size() - 1);

        // initialize popularity dist
        // group buckets of "similarly popular" objects (adding up to zipf-rate 1)
        popDist.push_back(std::vector<IdSizePair>());
        double ratesum = 0;    // 累计概率
        int64_t popBucket = 0; // 当前的流行度桶序号
        uint64_t objectId = 1; // 生成的对象id
        uint64_t vec_size = min_size;
        // 创建一个空的流行度桶，流行度桶是一个向量，用于存储具有相似流行度的对象，每个对象都由一个ID和大小的对组成。
        popDist[popBucket].reserve(vec_size);
        // 根据对象数量生成对象ID和大小，并将其添加到流行度桶中
        for (; objectId < objCount; objectId++)
        {
            // 流行度排名为n的元素的频率与1/n^alpha成正比
            ratesum += 1.0 / pow(double(objectId), zipfAlpha);

            // 注意这里大小与对象频率没有关系，是随机选取的一个大小
            // 为每个对象生成一个ID和大小，并将其添加到当前的流行度桶中
            popDist[popBucket].push_back(
                IdSizePair(objectId,
                           base_sizes[size_dist(rd_gen)]));
            if (objectId % 100000000 == 0)
            {
                std::cout << "Generated object " << objectId << std::endl;
            }
            if (objectId == objCount - 1)
            {
                std::cout << "Generated object " << objectId << std::endl;
                break;
            }
            // 当累计的频率超过1时，将创建一个新的流行度桶，并将累计频率重置为0
            // 相当于做分割，比如若有3个桶，首先按1/3概率选取到其中一个桶，即选取到了所有样本的前1/3、中1/3、后1/3
            // 然后再在选取到的桶中按照均匀分布选取一个样本，这样实际上和从所有样本中按频率选取一个样本的概率是一样的
            if (ratesum > 1 && (popDist[popBucket].size() >= min_size))
            {
                // std::cerr << "init " << objectId << " " << ratesum << "\n";
                // move to next popularity bucket
                popDist.push_back(std::vector<IdSizePair>());
                // 下一个桶容量翻倍，因为越往后每个对象的频率越低，所以每个桶的对象数量也越多
                vec_size = 2 * popDist[popBucket].size();
                popBucket++;
                popDist[popBucket].reserve(vec_size);
                ratesum = 0;
            }
        }
        // std::cerr << "init " << objectId << " " << ratesum << "\n";
        // corresponding PNRGs

        popOuterRng = std::uniform_int_distribution<size_t>(0, popDist.size() - 1);
        for (size_t i = 0; i < popDist.size(); i++)
        { // 为每个流行度桶创建了一个均匀分布，用于在桶内部进行随机采样
            popInnerRngs.push_back(std::uniform_int_distribution<size_t>(0, popDist[i].size() - 1));
        }
        std::cerr << "zipf debug objectCount " << popDist.size() << " smallest bucket size " << popDist[0].size() << " largest bucket size " << popDist[popDist.size() - 1].size() << " effective probability " << 1.0 / popDist.size() * 1.0 / popDist[popDist.size() - 1].size() << " theoretical probability " << 1 / pow(double(objCount), zipfAlpha) << "\n";

        printBucketSize();
    }

    // 从Zipf-like分布中采样一个对象
    void Sample(uint64_t &id, uint64_t &size)
    {
        // two step sample
        // 首先从流行度分布中随机选择一个桶
        const size_t outerIdx = popOuterRng(rd_gen);
        // 在该桶内部随机选择一个对象
        const size_t innerIdx = popInnerRngs[outerIdx](rd_gen);
        // std::cerr << outerIdx << " " << innerIdx << "\n";
        const IdSizePair sample = (popDist[outerIdx])[innerIdx];
        id = sample.first;    // return values
        size = sample.second; // return values
        // stats => std::unordered_map<int64_t, IdSizePair> -> <对象ID,<对象大小,对象被采样的次数>>
        if (stats.count(id) > 0)
        {
            stats[id].second++; // 对象被采样的次数
        }
        else
        { // 对象第一次被采样
            stats[id].first = size;
            stats[id].second = 1;
        }
    }

    void printBucketSize()
    {
        for (size_t i = 0; i < popDist.size(); i++)
        {
            DEBUG("bucket %lu size %lu\n", i, popDist[i].size());
        }
    }

    // 输出统计信息，包括每个对象的ID、大小和被采样的次数
    void Summarize()
    {
        std::map<int64_t, IdSizePair> summary;
        for (auto it : stats)
        {
            summary[it.second.second] = IdSizePair(it.first, it.second.first);
        }
        // int64_t top=10;
        int64_t summed_bytes = 0;
        int64_t idx = 0;
        // 创建了一个反向迭代器, 从summary的末尾开始遍历
        std::map<int64_t, IdSizePair>::reverse_iterator rit = summary.rbegin();
        for (; rit != summary.rend(); ++rit)
        {
            // 到目前为止的总大小
            summed_bytes += rit->second.second;
            // 当前的索引、对象被采样的次数、对象ID、对象大小和summed_bytes输出到标准错误流
            std::cerr << idx++ << " " << rit->first << " " << rit->second.first << " " << rit->second.second << " " << summed_bytes << "\n";
            // if(--top <1 ) {
            //     break;
            // }
        }
    }
};
