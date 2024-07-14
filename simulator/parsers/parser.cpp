#include <libconfig.h++>

#include "parser.hpp"
#include "../config_reader.hpp"
#include "facebook_tao_parser_simple.hpp"
#include "meta_kv_parser.hpp"
#include "zipf_parser.hpp"
#include "binary_parser.hpp"
#include "block_binary_parser.hpp"

parser::Parser *parser::Parser::create(const libconfig::Setting &settings)
{
    misc::ConfigReader cfg(settings);

    std::string parserType = cfg.read<const char *>("trace.format");

    std::cout << "Parser type: " << parserType << std::endl;

    // 负载中的请求总数/K个
    auto numKRequests = cfg.read<int>("trace.totalKAccesses", -1);
    // 负载中的请求总数/个
    int64_t numRequests = 1024 * numKRequests;
    std::cout << "num requests: " << numRequests << std::endl;
    // 根据负载格式，返回对应的 Parser 实例
    if (parserType == "Zipf")
    {
        assert(numRequests > 0);
        auto alpha = cfg.read<float>("trace.alpha");
        auto numObjects = cfg.read<int>("trace.numObjects");
        return new ZipfParser(alpha, numObjects, numRequests);
    }
    else if (parserType == "FacebookTaoSimple")
    {
        std::string filename1 = cfg.read<const char *>("trace.filename");
        double sampling = cfg.read<double>("trace.samplingPercent", 1);
        int seed = cfg.read<int>("trace.samplingSeed", 0);
        double scaling = cfg.read<double>("trace.objectScaling", 1);
        return new FacebookTaoSimpleParser(filename1, numRequests, sampling, seed, scaling);
    }
    else if (parserType == "MetaKV")
    {
        std::string filename1 = cfg.read<const char *>("trace.filename");
        double sampling = cfg.read<double>("trace.samplingPercent", 1);
        int seed = cfg.read<int>("trace.samplingSeed", 0);
        double scaling = cfg.read<double>("trace.objectScaling", 1);
        return new MetaKVParser(filename1, numRequests, sampling, seed, scaling);
    }
    else if (parserType == "Binary")
    {
        std::string filename1 = cfg.read<const char *>("trace.filename");
        double sampling = cfg.read<double>("trace.samplingPercent", 1);
        int seed = cfg.read<int>("trace.samplingSeed", 0);
        double scaling = cfg.read<double>("trace.objectScaling", 1);
        std::string fmt_str = cfg.read<const char *>("trace.formatString");
        // std::string fmt_str = "IQQBQB";
        return new BinaryParser(filename1, numRequests, fmt_str, fmt_str.length());
    }
    else if (parserType == "BlockBinary")
    {
        std::string filename1 = cfg.read<const char *>("trace.filename");
        double sampling = cfg.read<double>("trace.samplingPercent", 1);
        int seed = cfg.read<int>("trace.samplingSeed", 0);
        double scaling = cfg.read<double>("trace.objectScaling", 1);
        std::string fmt_str = cfg.read<const char *>("trace.formatString");
        int pageSize = cfg.read<int>("trace.pageSize");
        // std::string fmt_str = "IQQBQB";
        return new BlockBinaryParser(filename1, pageSize, numRequests, fmt_str, fmt_str.length());
    }
    else
    {
        std::cerr << "Unknown parser type: " << parserType << std::endl;
        assert(false);
    }
}
