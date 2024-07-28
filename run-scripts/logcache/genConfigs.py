#!/usr/bin/python3
from datetime import datetime
from pathlib import Path
import argparse
import math
import sys

sys.path.append('../')
from config import *

# parameters
memSizes = [ 17 ]
flashSizes = [ 950 ]
setCapacity = 4
numKRequests = -1
overhead = 30 / 8

template = Config().parseFromFile('template.cfg')

# we ultimately produce a list of (name, cfg) pairs
class Experiment:
    def __init__(self, name, cfg):
        self.name = name
        self.cfg = cfg.clone()

    def clone(self):
        return Experiment(self.name, self.cfg)

template = Experiment('LogCache', template)

# functions to manipulate configs
# 对exps中原有的配置添加新配置，首先遍历原有配置数量，然后fn中会根据要添加的配置数量再进行遍历
# 若要添加的新配置有多个值，则最后生成的配置数量为原有配置数量*要添加的配置数量
def expand(exps, fn, *args, **kwargs):
    out = []

    # 遍历exps列表中的元素
    for exp in exps:
        copy = exp.clone()
        # 对每个元素进行fn操作，参数为*args, **kwargs
        expanded = fn(copy, *args, **kwargs)
        # 添加到输出列表
        out.append(expanded)
    
    # flatten，将列表中的列表元素展开
    return [ exp for l in out for exp in l ]

# 缓存预热
def slow_warmup(exp):
    exp_mod = exp.clone()
    exp_mod.cfg['cache.slowWarmup'] = 1
    return [exp_mod]

def flash_sizes(exp, flash_sizes_mb):
    exp.name += '-flashSize'
    out = []

    for size in flash_sizes_mb:
        szExp = exp.clone()
        szExp.cfg['cache.flashSizeMB'] = size;
        szExp.name += f'{size}MB'
        out.append(szExp)

    return out

def cache_sizes(exp, cache_size_mb):
    exp.name += '-cacheSize'
    out = []

    for size in cache_size_mb:
        szExp = exp.clone()
        szExp.cfg['cache.cacheSizeMB'] = size;
        szExp.name += f'{size}MB'
        out.append(szExp)

    return out

def cache_algos(exp, cache_algo_name):
    exp.name += '-cacheAlgo'

    out = []
    for algo_name in cache_algo_name:
        szExp = exp.clone()
        szExp.cfg['cache.cacheAlgoName'] = f'{algo_name}'
        # print(algo_name)
        szExp.name += algo_name
        # print(szExp.name)
        out.append(szExp)

    return out

def mem_sizes(exp, mem_sizes_mb):
    exp.name += '-memSize'
    out = []

    for size in mem_sizes_mb:
        szExp = exp.clone()
        szExp.cfg['cache.memorySizeMB'] = size
        szExp.name += f'{size}MB'
        out.append(szExp)


    # [print(f'name: {exp.name} \ncfg: {exp.cfg}') for exp in out]
    # exit()
    ''' 例子
    name: 
        kangaroo-memSize5MB 
    cfg: 
        cache = {
            memorySizeMB = 5;
        };
    '''

    return out

def limit_requests(exp, kRequests):
    out = []
    for reqs in kRequests:
        new_exp = exp.clone()
        new_exp.name += f'-numKRequests{reqs}'
        new_exp.cfg['trace.totalKAccesses'] = int(reqs) 
        out.append(new_exp)
    return out 

def block_size(exp, block_size):
    out = []
    for size in block_size:
        new_exp = exp.clone()
        new_exp.name += f'-blockSize{size}'
        new_exp.cfg['log.blockSize'] = int(size) 
        out.append(new_exp)
    return out 

def segment_size(exp, segment_size):
    out = []
    for size in segment_size:
        new_exp = exp.clone()
        new_exp.name += f'-segmentSizeMB{size}'
        new_exp.cfg['log.segmentSizeMB'] = int(size) 
        out.append(new_exp)
    return out 

def log_type(exp, log_type): 
    out = []
    for type in log_type:
        new_exp = exp.clone()
        new_exp.name += f'-logType{type}'
        new_exp.cfg['log.logType'] = f'{type}'
        out.append(new_exp)
    return out 

def enable_GC(exp, enable_GC):
    out = []
    for enable in enable_GC:
        new_exp = exp.clone()
        new_exp.name += f'-enableGC{enable}'
        new_exp.cfg['log.enableGC'] = int(enable) 
        out.append(new_exp)
    return out 

def log_dram_ratio(avg_object_size, scaling):
    return overhead / (avg_object_size * scaling)

def zipf(exp, alphas):
    out = []
    # 对每个alpha值生成一个配置
    for alpha in alphas:
        exp2 = exp.clone()
        exp2.name += f'-zipf{alpha}'
        exp2.cfg['trace.totalKAccesses'] = 10000
        exp2.cfg['trace.alpha'] = alpha
        exp2.cfg['trace.numKObjects'] = 100
        exp2.cfg['trace.format'] = 'Zipf'
        out.append(exp2)
    return out

def churn(exp):
    exp.name += '-churn'
    exp.cfg['trace.totalKAccesses'] = 10000
    exp.cfg['trace.alpha'] = 0.8
    exp.cfg['trace.numObjects'] = 1000
    exp.cfg['trace.numActiveObjects'] = 10
    exp.cfg['trace.format'] = 'Churn'
     
    out = []
    for prob in [ 0., 0.01, 0.05, 0.1 ]:
        exp2 = exp.clone()
        exp2.name += f'{prob:.2}'
        exp2.cfg['trace.churn'] = prob
        out.append(exp2)
    return out

def fb_tao_simple(exp, scaling):
    avg_obj_size = 291
    exp.name += '-fbTaoSimple'
    exp.cfg['trace.totalKAccesses'] = numKRequests
    exp.cfg['trace.filename'] = "fb-sampled.csv"
    exp.cfg['trace.format'] = 'FacebookTaoSimple'

    out = []
    for s in scaling:
        exp2 = exp.clone()
        if scaling != 1:
            exp2.cfg['cache.memOverheadRatio'] = log_dram_ratio(avg_obj_size, s)
            exp2.cfg['trace.objectScaling'] = float(s)
            exp2.name += f'-scaling{s}'
        out.append(exp2)
    return out

def meta_kv(exp, scaling):
    exp.name += '-metaKV'
    exp.cfg['trace.totalKAccesses'] = numKRequests
    exp.cfg['trace.filename'] = "/media/wzj/Data/traces/meta_kv/202206/kvcache_traces_1.csv"
    exp.cfg['trace.format'] = 'MetaKV'

    out = []
    for s in scaling:
        exp2 = exp.clone()
        if scaling != 1:
            exp2.cfg['trace.objectScaling'] = float(s)
            exp2.name += f'-scaling{s}'
        out.append(exp2)
    return out

def binary(exp, formatString, scaling):
    exp.name += '-binary'
    exp.cfg['trace.totalKAccesses'] = numKRequests
    exp.cfg['trace.filename'] = "/media/wzj/Data/traces/ibm_cos/compress/IBMObjectStoreTrace005Part0.bin.zst"
    exp.cfg['trace.format'] = 'Binary'
    exp.cfg['trace.formatString'] = f'{formatString}'

    out = []
    for s in scaling:
        exp2 = exp.clone()
        if scaling != 1:
            exp2.cfg['trace.objectScaling'] = float(s)
            exp2.name += f'-scaling{s}'
        out.append(exp2)
    return out

def block_binary(exp, formatStrings, pageSize):
    out = []
    # 对每个pageSize值生成一个配置
    for formatString in formatStrings:
        exp2 = exp.clone()
        exp2.name += '-BlockBinary'
        exp2.cfg['trace.totalKAccesses'] = numKRequests
        # exp2.cfg['trace.filename'] = "/media/wzj/Data/traces/ali_block/oracle_compress_BIT/10_BIT_oracle.bin.zst"
        exp2.cfg['trace.filename'] = "/media/wzj/Data/traces/ali_block/oracle_compress_BIT/215_BIT_oracle.bin.zst"
        exp2.cfg['trace.pageSize'] = int(pageSize)
        exp2.cfg['trace.format'] = 'BlockBinary'
        exp2.cfg['trace.formatString'] = f'{formatString}'
        out.append(exp2)

    return out

def stat_outfile(exp, dirpath, stats_interval=None):
    exp.cfg['stats.outputFile'] = f'{dirpath}/output/{exp.name}.out'
    if stats_interval:
        exp.cfg['stats.collectionIntervalPower'] = stats_interval
    return [exp]

def generate_base_exps(args):
    base_exps = [template]
    
    # print(args)
    # print(base_exps[0].name)
    # print(base_exps[0].cfg)
    # exit()
    
    if args.mem_size_mb:
        base_exps = expand(base_exps, mem_sizes, args.mem_size_mb)
    # else:
    #     base_exps = expand(base_exps, mem_sizes, memSizes)

    base_exps = expand(base_exps, slow_warmup)

    if args.flash_size_mb:
        base_exps = expand(base_exps, flash_sizes, args.flash_size_mb)
    else:
        base_exps = expand(base_exps, flash_sizes, flashSizes)
    
    if args.cache_size_mb:
        base_exps = expand(base_exps, cache_sizes, args.cache_size_mb)
        
    if args.cache_algo_name:
        base_exps = expand(base_exps, cache_algos, args.cache_algo_name)
    
    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in base_exps]
    # exit()
    
    return base_exps

def add_log(exps, log_args):
    # new_exps = []
    
    if log_args == None:
        log_args={}
    
    if 'block_size' not in log_args:
        log_args['block_size'] = [4096]
    
    if 'segment_size' not in log_args:
        log_args['segment_sizeMB'] = [2]
        
    if 'log_type' not in log_args:
        log_args['log_type'] = ['FIFOLog']
    
    exps = expand(exps, block_size, log_args['block_size'])
    exps = expand(exps, segment_size, log_args['segment_sizeMB'])
    exps = expand(exps, log_type, log_args['log_type'])
    
    if 'enable_GC' in log_args:
        exps = expand(exps, enable_GC, log_args['enable_GC'])
    
    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in exps]
    # exit()
    
    return exps

def add_traces(exps, trace_args):
    new_exps = []
    traceClass=''
    
    if 'zipf' in trace_args:# 将zipf负载配置添加到现有配置，组成新的配置，并把新配置添加到新配置列表末尾
        new_exps.extend(expand(exps, zipf, trace_args['zipf']))
        traceClass = 'zipf'

    if 'scaling' not in trace_args:
        trace_args['scaling'] = [1]

    if 'fb_tao_simple' in trace_args:
        new_exps.extend(expand(exps, fb_tao_simple, trace_args['scaling']))
        traceClass = 'fb_tao_simple'

    if 'meta_kv' in trace_args:
        new_exps.extend(expand(exps, meta_kv, trace_args['scaling']))
        traceClass = 'meta_kv'

    if 'binary' in trace_args:
        new_exps.extend(expand(exps, binary, trace_args['binary'], trace_args['scaling']))
        traceClass = 'binary'

    if 'block_binary' in trace_args:
        pagesize=trace_args['page_size'][0] if 'page_size' in trace_args else 4096
        new_exps.extend(expand(exps, block_binary, trace_args['block_binary'], pagesize))
        traceClass = 'block_binary'

    if 'simpling_seed' in trace_args:
        return NotImplemented

    if 'limit_requests' in trace_args:# 请求数量限制
        new_exps = expand(new_exps, limit_requests, trace_args['limit_requests'])
    return new_exps, traceClass


class LayeredAction(argparse.Action):
    # 这个类的主要作用是在解析命令行参数时，将参数的值存储在一个字典中，而不是直接存储在 namespace 对象的属性中。
    # 这样可以方便地处理多个参数共享一个命名空间的情况
    def __init__(self, option_strings, dest, const=None, default=None, nargs=0, **kwargs):
        self.default_value = default
        self.const_name = const
        super(LayeredAction, self).__init__(option_strings, dest, nargs, **kwargs)
    # 接收四个参数，包括 parser（解析器对象），namespace（命令行参数的命名空间），
    # values（命令行参数的值），以及 option_string（命令行选项字符串）
    def __call__(self, parser, namespace, values, option_string=None):
        # 尝试获取 namespace 中名为 self.dest 的属性的值
        current = getattr(namespace, self.dest)
        if not current:
            # 在 namespace 中设置一个名为 self.dest 的新属性，其值为一个空字典，然后再次获取该属性的值
            setattr(namespace, self.dest, {})
            current = getattr(namespace, self.dest)
        # 将 values（如果 values 为假，则使用 self.default_value）赋给 current 字典中名为 self.const_name 的键
        current[self.const_name] = values if values else self.default_value


'''
`add_argument` 方法用于向解析器添加程序参数，它包含以下参数：

- `name` 或 `flags`：命令行选项名或标志，例如 `-f` 或 `--file`。
- `action`：当参数在命令行中出现时执行的动作。默认值是 `store`。
- `nargs`：命令行参数应当消耗的数目。
        N：一个整数。命令行参数应收集 N 个值。
        '?'：零个或一个。如果可能，命令行参数应收集一个值，否则默认为 None。
        '*'：零个或多个。命令行参数应收集所有值。
        '+'：一个或多个。命令行参数应收集至少一个值。
        argparse.REMAINDER：所有剩余的命令行参数都应被收集。
- `const`：某些动作或参数个数选项所需的常数值。
- `default`：如果命令行中没有出现该参数时的默认值。
- `type`：命令行参数应当被转换成的类型。
- `choices`：参数的可选值的容器。
- `required`：是否命令行选项可以省略（仅选项可用）。
- `help`：参数的简短描述。
- `metavar`：在使用方法消息中使用的参数值示例。
- `dest`：被添加到 parse_args() 所返回对象上的属性名。
'''

'''
`action` 参数在 `argparse` 库中定义了当参数在命令行中出现时应该执行的动作。以下是一些常见的 `action` 值：

- `'store'`：这是默认动作。当这个动作被指定时，参数的值会被保存下来。如果参数的类型（`type`）被指定，那么参数的值会被转换为指定的类型。

- `'store_const'`：这个动作用于实现像 `--verbose` 这样的标志，这些标志没有值，但表示某种行为。当使用 `'store_const'` 时，需要提供一个 `const` 参数来指定存储的值。

- `'store_true'` 和 `'store_false'`：这两个动作用于实现布尔开关。例如，如果你有一个 `--verbose` 开关，你可以使用 `'store_true'` 动作，那么当 `--verbose` 出现在命令行中时，它的值为 `True`，否则为 `False`。

- `'append'`：这个动作用于收集值到列表中。每次参数在命令行中出现时，都会将其值添加到列表中。

- `'append_const'`：这个动作用于将指定的 `const` 值添加到列表中。

- `'count'`：这个动作用于计数参数在命令行中出现的次数。

- `'help'`：这个动作用于打印完整的帮助信息，然后退出。

- `'version'`：这个动作用于打印版本信息，然后退出。
'''

def arg_parser():
    parser = argparse.ArgumentParser()

    # 第一个参数为name
    parser.add_argument('name', help='name for experiment generation')
    
    # 闪存缓存大小，使用--flash-size-MB来指定这个参数的值，可以指定多个值
    parser.add_argument('--flash-size-MB', dest='flash_size_mb', type=int,
            nargs='+', metavar='size', help='defaults to correct size for tao') 
    
    # 逻辑缓存大小，使用--cache-size-MB来指定这个参数的值，可以指定多个值
    parser.add_argument('--cache-size-MB', dest='cache_size_mb', type=int,
            nargs='+', metavar='size', help='logical cache size in MB') 
    
    # 逻辑缓存算法，使用--cache-algo来指定这个参数的值，可以指定多个值
    parser.add_argument('--cache-algo', dest='cache_algo_name', type=str,
            nargs='+', metavar='algo', help='logical cache replacement algo') 
    
    # 内存缓存大小，使用--mem-size-MB来指定这个参数的值，可以指定多个值
    parser.add_argument('--mem-size-MB', dest='mem_size_mb', type=int, 
            nargs='+', metavar='size', help='defaults to correct size for tao') 
    
    # 生成的配置文件保存到的文件夹路径，使用--directory来指定
    # parser.add_argument('--directory', action='store_true', help=f'generate config in specified directory')
    parser.add_argument('--directory', help=f'generate config in specified directory')
    
    # 统计信息的输出间隔，使用--stats-interval来指定
    parser.add_argument('--stats-interval', help='10**(STATS_INTERVAL)', type=int)

    # 负载类型
    traces = parser.add_argument_group('trace types (at least one required)')
    # facebook tao负载
    traces.add_argument('--fb-simple', dest='traces', action=LayeredAction, const='fb_tao_simple')
    # zipf负载，之后可以指定多个alpha值，默认为0.8
    traces.add_argument('--zipf', dest='traces', action=LayeredAction, 
            const='zipf', default=[0.8], nargs='*', type=float, metavar='alpha', help='defaults to .8')
    # binary负载，之后可以指定多个formatString值，默认为'IQQB'
    traces.add_argument('--binary', dest='traces', action=LayeredAction, 
            const='binary', default=['IQQB'], nargs='*', type=str, metavar='formatString', help='defaults to IQQB')
    # block binary负载，之后可以指定多个formatString值，默认为'IQQB'
    traces.add_argument('--block-binary', dest='traces', action=LayeredAction, 
            const='block_binary', default=['IQQB'], nargs='*', type=str, metavar='formatString', help='defaults to IQQB')
    # page size，用于block负载的分页处理，默认为4096
    traces.add_argument('--page-size', dest='traces', action=LayeredAction, 
            const='page_size', default=4096, nargs=1, type=int, metavar='pageSize', help='defaults to 4096')
    # 限制最多读取多少个请求
    traces.add_argument('--limit-requests', dest='traces', action=LayeredAction, const='limit_requests', 
            nargs=1, default=-1, metavar='numKRequests')
    # 采样随机种子，可以指定多个
    traces.add_argument('--sampling-seed', dest='traces', action=LayeredAction, const='sampling_seed', help='defaults to no scaling', 
            nargs='*', metavar='samplingSeed', default=[0], type=float)
    # 对象大小缩放比例，可以指定多个
    traces.add_argument('--obj-scaling', dest='traces', action=LayeredAction, const='scaling', help='defaults to no scaling', 
            nargs='+', metavar='multiple', default=[1], type=float)

    log = parser.add_argument_group('log')
    # 日志块大小
    log.add_argument('--block-size', dest='log', action=LayeredAction, const='block_size',
            type=int, nargs='+', metavar='size', help='block size')
    # 日志段大小
    log.add_argument('--segment-size-MB', dest='log', type=int, nargs='+',
            action=LayeredAction, const='segment_sizeMB', help='segment_size')
    # 是否启用逻辑层以及GC，0或1
    log.add_argument('--enable-GC', dest='log', action=LayeredAction, const='enable_GC', 
                     nargs='+', type=int, help='enable logical tier and GC')
    log.add_argument('--log-type', dest='log', action=LayeredAction, const='log_type', 
                     nargs='+', type=str, help='log type')


    args = parser.parse_args()
    return args

def main():
    # 
    args = arg_parser()
    # print(args)
    # exit()
    if not args.traces:
        raise ValueError("Need at least one trace type")
    
    # 基础config，cfg文件中cache部分的内容
    exps = generate_base_exps(args)
    
    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in exps]
    # exit()
    
    exps = add_log(exps, args.log)
    
    # 添加负载配置信息
    exps, traceClass = add_traces(exps, args.traces)

    now = datetime.now()
    time_str = now.strftime('%Y-%m-%d_%H-%M-%S')
    # dirname = f'/n/corgi/results/kangaroo/{time_str}-{args.name}'
    dirname = f'./{time_str}-{args.name}'
    # print(dirname)
    
    if not args.directory:
        dirname = '.'
    else:
        dirname = args.directory
    
    print(dirname)
    # exit()
    
    configs_dir=Path(dirname)/f'{traceClass}'/'configs'
    output_dir=Path(dirname)/f'{traceClass}'/'output'
    
    (configs_dir).mkdir(exist_ok=True, parents=True)
    (output_dir).mkdir(exist_ok=True)
    
    if args.stats_interval:
        exps = expand(exps, stat_outfile, dirname, args.stats_interval)
    else: 
        exps = expand(exps, stat_outfile, dirname)

    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in exps]
    # exit()
    
    for exp in exps:
        print(f'{configs_dir}/{exp.name}.cfg', '...')
        exp.cfg.writeToFile(f'{configs_dir}/{exp.name}.cfg')

if __name__ == "__main__":
    main()
