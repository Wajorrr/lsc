#pragma once

#include <string>
#include <sstream>

namespace misc
{

  inline std::string bytes(uint64_t size)
  { // 将字节大小转换为更易读的格式，例如将1024转换为"1KB"
    std::stringstream ss;
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};

    uint32_t i;
    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
    {
      if (size < 1024 * 1024)
      {
        break;
      }
      else
      {
        size /= 1024;
      }
    }

    if (i == sizeof(suffixes) / sizeof(suffixes[0]))
    { // 如果size的值超过了"TB"，我们仍然使用"TB"作为单位
      --i;
    }

    printf("%ld%s\n", size, suffixes[i]);
    ss << size << suffixes[i];
    return ss.str();
  }

}
