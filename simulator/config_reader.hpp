#pragma once

#include <iostream>
#include "common/logging.h"

namespace misc
{

    struct ConfigReader
    {
        ConfigReader(const libconfig::Setting &_setting) : setting(_setting) {}

        const libconfig::Setting &setting;

        template <typename T>
        T search(const libconfig::Setting &root, std::string key) const
        {
            // 在 libconfig 中，可以使用点分隔符（.）来表示配置设置的层次结构
            // 例如，"database.port" 表示 database 设置中的 port 设置
            auto split = key.find('.');
            // 直到剩下的字符串中不存在'.'，递归回溯
            if (split == std::string::npos)
            {
                return root[key.c_str()];
            }
            else // 继续递归，当前'.'之后的字符串
            {
                return search<T>(root[key.substr(0, split).c_str()], key.substr(split + 1, std::string::npos));
            }
        }

        bool exists(const libconfig::Setting &root, std::string key) const
        {
            auto split = key.find('.');
            // 递归到最后一个时，判断配置属性是否存在
            if (split == std::string::npos)
            {
                return root.exists(key);
            }
            else
            {
                return exists(root[key.substr(0, split).c_str()], key.substr(split + 1, std::string::npos));
            }
        }

        bool exists(std::string key) const
        {
            return exists(setting, key);
        }

        template <typename T>
        T read(std::string key) const // 读取给定的配置
        {
            try
            {
                return search<T>(setting, key);
            }
            catch (const libconfig::SettingTypeException &tex)
            {
                ERROR("Type mismatched: %s\n", key.c_str());
                // std::cerr << "Type mismatched: " << key << std::endl;
                exit(-1);
            }
            catch (const libconfig::SettingNotFoundException &nfex)
            {
                ERROR("Setting not found: %s\n", key.c_str());
                // std::cerr << "Setting not found: " << key << std::endl;
                exit(-1);
            }
        }

        template <typename T>
        T read(std::string key, T _default) const // 读取给定的配置，若不存在则返回_default值
        {
            try
            {
                return search<T>(setting, key);
            }
            catch (const libconfig::SettingTypeException &tex)
            {
                ERROR("Type mismatched: %s\n", key.c_str());
                // std::cerr << "Type mismatched: " << key << std::endl;
                exit(-1);
            }
            catch (const libconfig::SettingNotFoundException &nfex)
            {
                WARN("Setting not found: %s, using default value: %d\n", key.c_str(), _default);
                // std::cerr << "Setting not found: " << key << ", using default value: " << _default << std::endl;
                return _default;
            }
        }
    };

}
