#include "caches/cache.hpp"
#include "caches/write_cache.hpp"
#include "caches/block_cache.hpp"
#include "parsers/parser.hpp"
#include "config_reader.hpp"
#include "common/logging.h"

using namespace std;

// cache::Cache *_cache;
// cache::WriteCache *_cache;
cache::BlockCache *_cache;

void simulateCache(const parser::Request &req)
{
  _cache->access(req);
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: ./cache <config-file>\n");
    exit(-1);
  }

  // libconfig::Config 是 libconfig 库中的一个类，用于处理配置文件
  libconfig::Config cfgFile;

  // Read the file. If there is an error, report it and exit.
  try
  {
    // 读取命令行参数指定的配置文件
    cfgFile.readFile(argv[1]);
  }
  catch (const libconfig::FileIOException &fioex)
  {
    std::cerr << "I/O error while reading config file." << std::endl;
    return (EXIT_FAILURE);
  }
  catch (const libconfig::ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    return (EXIT_FAILURE);
  }

  // 根设置是所有其他设置的容器。可以把它想象成一个树的根，所有其他的设置（如字符串、数字、数组、对象等）都是它的子节点。
  // 从配置文件中获取根设置，通过获取配置文件的根设置，我们可以访问和操作配置文件中的所有其他设置
  const libconfig::Setting &root = cfgFile.getRoot();
  misc::ConfigReader cfg(root);

  // 根据配置文件中的设置，创建一个用于解析对应请求格式的字符串的 Parser 实例
  parser::Parser *parserInstance = parser::Parser::create(root);

  // 根据配置文件中的设置，创建一个用于缓存请求的 Cache 实例
  // _cache = cache::Cache::create(root);
  // _cache = cache::WriteCache::create(root);
  _cache = cache::BlockCache::create(root);

  // 打印信息
  _cache->dumpStats();

  // 开始读取请求
  time_t start = time(NULL);

  INFO("Start reading requests\n");

  parserInstance->go(simulateCache);

  // 结束读取请求
  time_t end = time(NULL);

  std::cout << std::endl;
  // 打印统计信息
  _cache->dumpStats();

  INFO("Processed %lu in %ld seconds, rate of %lf accs/sec\n",
       _cache->getTotalAccesses(), end - start,
       (1. * _cache->getTotalAccesses() / (end - start)));
  // std::cout << "Processed " << _cache->getTotalAccesses() << " in "
  //           << (end - start) << " seconds, rate of "
  //           << (1. * _cache->getTotalAccesses() / (end - start)) << " accs/sec"
  //           << std::endl;

  delete parserInstance;
  delete _cache;
  return 0;
}
