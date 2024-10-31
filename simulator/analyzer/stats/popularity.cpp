#include "popularity.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <numeric>
#include <unordered_map>
#include <vector>

namespace traceAnalyzer
{
    using namespace std;

    void Popularity::dump(string &path_base)
    {
        if (freq_vec_.empty())
        {
            assert(!has_run);
            ERROR("popularity has not been computed\n");
            return;
        }

        string ofile_path = path_base + ".popularity";
        ofstream ofs(ofile_path, ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        ofs << "# freq (sorted):cnt - for Zipf plot\n";

        /* convert sorted freq list to freq:cnt list sorted by freq (to save some
         * space) */
        uint32_t last_freq = freq_vec_[0];
        uint32_t freq_cnt = 0;
        // 将排序后的对象访问频率列表转换为访问频率:数量列表
        for (auto &cnt : freq_vec_)
        {
            if (cnt == last_freq)
            {
                freq_cnt += 1;
            }
            else
            {
                ofs << last_freq << ":" << freq_cnt << "\n";
                freq_cnt = 1;
                last_freq = cnt;
            }
        }
        // 依次输出频率和出现次数
        ofs << last_freq << ":" << freq_cnt << "\n";
        ofs.close();
    }

    void Popularity::run(obj_info_map_type &obj_map)
    {
        /* freq_vec_ is a sorted vec of obj frequency */
        freq_vec_.reserve(obj_map.size());

        // 将所有对象的访问频率放入freq_vec_中并按降序排序
        for (const auto &p : obj_map)
        {
            freq_vec_.push_back(p.second.freq);
        }
        sort(freq_vec_.begin(), freq_vec_.end(), greater<>());

        // 对象数量太少，返回
        if (obj_map.size() < 200)
        {
            fit_fail_reason_ = "popularity: too few objects (" +
                               to_string(obj_map.size()) +
                               "), skip the popularity computation";
            WARN("%s\n", fit_fail_reason_.c_str());
            return;
        }

        // 最受欢迎的对象的访问频率太低
        if (freq_vec_[0] < 200)
        {
            fit_fail_reason_ = "popularity: the most popular object has " +
                               to_string(freq_vec_[0]) + " requests ";
            WARN("%s\n", fit_fail_reason_.c_str());
        }

        /* calculate Zipf alpha using linear regression */
        // Zipf 分布是一种离散概率分布，它的概率质量函数满足幂律，即排名（rank）和频率（frequency）之间的关系可以用一个负的幂次表示

        vector<double> log_freq(obj_map.size());
        vector<double> log_rank(obj_map.size());

        int i = 0;
        // 将所有对象的访问频率的对数放入log_freq中
        for_each(log_freq.begin(), log_freq.end(),
                 [&](double &item)
                 { item = log(freq_vec_[i++]); });
        i = 0;
        // 将所有对象的访问频率排名的对数放入log_rank中
        for_each(log_rank.begin(), log_rank.end(), [&](double &item)
                 { ++i;
               item = log(i); });

        /* TODO: a better linear regression with intercept and R2 */
        // 线性回归分析，使用最小二乘法计算斜率
        // 这个线性关系的斜率就是我们要找的 Zipf 分布的参数
        slope_ = -PopularityUtils::slope(log_rank, log_freq);

        has_run = true;
    }

    vector<uint32_t> freq_vec_{};
    double slope_, intercept_, r2_;
    bool has_run = false;
}; // namespace traceAnalyzer
