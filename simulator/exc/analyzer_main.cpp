#include "analyze/analyzer.h"
#include "caches/block_cache.hpp"
#include "caches/cache.hpp"
#include "caches/write_cache.hpp"
#include "common/logging.h"
#include "config_reader.hpp"
#include "parsers/parser.hpp"

traceAnalyzer::TraceAnalyzer* analyzer;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./cache <config-file>\n");
        exit(-1);
    }

    // libconfig::Config 是 libconfig 库中的一个类，用于处理配置文件
    libconfig::Config cfgFile;

    // Read the file. If there is an error, report it and exit.
    try {
        // 读取命令行参数指定的配置文件
        cfgFile.readFile(argv[1]);
    } catch (const libconfig::FileIOException& fioex) {
        ERROR("I/O error while reading config file.\n");
        // std::cerr << "I/O error while reading config file." << std::endl;
        return (EXIT_FAILURE);
    } catch (const libconfig::ParseException& pex) {
        ERROR("Parse error at %s:%d - %s\n", pex.getFile(), pex.getLine(),
              pex.getError());
        // std::cerr << "Parse error at " << pex.getFile() << ":" <<
        // pex.getLine()
        //           << " - " << pex.getError() << std::endl;
        return (EXIT_FAILURE);
    }

    // 根设置是所有其他设置的容器。可以把它想象成一个树的根，所有其他的设置（如字符串、数字、数组、对象等）都是它的子节点。
    // 从配置文件中获取根设置，通过获取配置文件的根设置，我们可以访问和操作配置文件中的所有其他设置
    const libconfig::Setting& root = cfgFile.getRoot();
    misc::ConfigReader cfg(root);

    // 根据配置文件中的设置，创建一个用于解析对应请求格式的字符串的 Parser 实例
    parser::Parser* parserInstance = parser::Parser::create(root);

    traceAnalyzer::analysis_option_t option = traceAnalyzer::default_option();
    traceAnalyzer::analysis_param_t analyzer_params =
        traceAnalyzer::default_param();
    // option.req_rate = true;
    // option.access_pattern = true;
    // option.size = true;
    // option.reuse = true;
    // option.popularity = true;
    // option.ttl = true;
    // option.popularity_decay = true;
    // option.lifetime = true;
    // // option.create_future_reuse_ccdf = true;
    // option.prob_at_age = true;
    // option.size_change = true;

    option.lifespan = true;

    // traceAnalyzer::TraceAnalyzer analyzer(parserInstance,
    // "/ssd1/output/twitter/cluster53/cluster53", option, analyzer_params);
    // traceAnalyzer::TraceAnalyzer analyzer(parserInstance,
    // "/ssd1/output/meta/202206_1/202206_1", option, analyzer_params);
    // traceAnalyzer::TraceAnalyzer analyzer(parserInstance,
    // "/ssd1/output/ibm/",
    //                                       option, analyzer_params);
    traceAnalyzer::TraceAnalyzer analyzer(
        parserInstance, "/data/ali_block/output/", option, analyzer_params);
    analyzer.initialize();
    analyzer.run();
}