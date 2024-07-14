#!/usr/bin/python3
from datetime import datetime
from pathlib import Path
import argparse
import math

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

template = Experiment('kangaroo', template)

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

# 
def victim_cache(exp, sizes):
    out = []
    for size in sizes:
        exp_mod = exp.clone()
        exp_mod.name += f'-victim{size}MB'
        exp_mod.cfg['victimCache.sizeMB'] = int(size)
        exp_mod.cfg['cache.flashSizeMB'] -= int(size)
        out.append(exp_mod)
    return out

# 是否统计不同set间的命中分布
def set_distribution(exp):
    exp_mod = exp.clone()
    exp_mod.cfg['cache.recordSetDistribution'] = 1
    return [exp_mod]

def set_hits_distribution(exp):
    print("sit hits dist")
    exp_mod = exp.clone()
    exp_mod.cfg['sets.hitDistribution'] = 1
    return [exp_mod]

def zipf(exp, alphas):
    out = []
    # 对每个alpha值生成一个配置
    for alpha in alphas:
        exp2 = exp.clone()
        exp2.name += f'-zipf{alpha}'
        exp2.cfg['trace.totalKAccesses'] = 10000
        exp2.cfg['trace.alpha'] = alpha
        exp2.cfg['trace.numObjects'] = 1000
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

def random_admission_pre_log(exp, ratios):
    if 'log' not in exp.cfg:
        return []

    out = []
    for prob in ratios:
        exp2 = exp.clone()
        if not math.isclose(1,float(prob)):
            exp2.name += '-randomAdmissionPreLog'
            exp2.name += f'{prob}'
            exp2.cfg['preLogAdmission.policy'] = 'Random'
            exp2.cfg['preLogAdmission.admitRatio'] = prob
        out.append(exp2)
    return out
 
def random_admission_pre_set(exp, ratios):
    out = []
    for prob in ratios:
        exp2 = exp.clone()
        if not math.isclose(1,float(prob)):
            exp2.name += '-randomAdmissionPreSet'
            exp2.name += f'{prob}'
            exp2.cfg['preSetAdmission.policy'] = 'Random'
            exp2.cfg['preSetAdmission.admitRatio'] = prob
        out.append(exp2)
    return out

def threshold(exp, thresholds):
    if 'log' not in exp.cfg:
        return []

    out = []
    for count in thresholds:
        thresh_exp = exp.clone()
        if count == 1:
            out.append(thresh_exp)
            continue
        thresh_exp.cfg['preSetAdmission.threshold'] = count
        thresh_exp.cfg['preSetAdmission.policy'] = 'Threshold'
        thresh_exp.name += f'-threshold{count}'
        out.append(thresh_exp)
    return out

def readmission(exp, thresholds):
    if 'preSetAdmission' not in exp.cfg:
        return [exp]
    
    out = []
    for num in thresholds:
        readmit_exp = exp.clone()
        if not num:
            out.append(exp)
            continue
        readmit_exp.cfg['log.readmit'] = num
        readmit_exp.name += f'-readmit{num}'
        out.append(readmit_exp)
    return out
 
def log(exp, adjust_flash_cap, percents):
    exp.name += '-logPer'
    out = []
    for percent in percents:
        log_exp = exp.clone()
        if not math.isclose(0,float(percent)):
            log_exp.cfg['log.percentLog'] = float(percent)
            log_exp.name += f'{percent}'
        if adjust_flash_cap:
            log_exp.name += f'-adjustFlashCap'
            log_exp.cfg['log.adjustFlashSizeUp'] = 1
        out.append(log_exp)
    return out

def multilog(exp, num_logs):
    if 'log' not in exp.cfg:
        return []

    out = []
    for number in num_logs:
        new_exp = exp.clone()
        new_exp.name += f'-mulitLog{number}'
        new_exp.cfg['log.multiLog'] = int(number)
        out.append(new_exp)
    return out
   
def rotating_log(exp, block_sizes):
    if 'log' not in exp.cfg:
        return []
    
    out = []
    for num in block_sizes:
        new_exp = exp.clone()
        new_exp.name += f'-flushSize{num}'
        new_exp.cfg['log.flushBlockSizeKB'] = int(num)
        out.append(new_exp)
    return out

def add_rrip(exp, rrip_bits):
    out = []
    for bits in rrip_bits:
        new_exp = exp.clone()
        new_exp.name += f'-rripBits{bits}'
        new_exp.cfg['sets.rripBits'] = int(bits)
        out.append(new_exp)
    return out

def add_rrip_promotion_only(exp):
    if '-rripMixedPromotion' in exp.name:
        return []
    new_exp = exp.clone()
    new_exp.name += f'-rripPromotionOnly'
    new_exp.cfg['sets.promotionOnly'] = 1
    return [new_exp]

def add_rrip_mixed_promotion(exp):
    if '-rripPromotionOnly' in exp.name:
        return []
    new_exp = exp.clone()
    new_exp.name += f'-rripMixedPromotion'
    new_exp.cfg['sets.mixedRRIP'] = 1
    return [new_exp]

def sets(exp, nru, set_capacities=[]):
    out = []

    if not set_capacities:
        set_capacities = [setCapacity]
    set_capacities = [int(cap) * 1024 for cap in set_capacities]

    for cap in set_capacities:
        new_exp = exp.clone()
        new_exp.name += '-setCapacity'
        new_exp.cfg['sets.setCapacity'] = cap
        new_exp.name += f'{cap}B'
        if nru:
            new_exp.cfg['sets.trackHitsPerItem'] = 1
            new_exp.name += '-nru'
        out.append(new_exp)
    return out

def flash_sizes(exp, flash_sizes_mb):
    exp.name += '-flashSize'
    out = []

    for size in flash_sizes_mb:
        szExp = exp.clone()
        szExp.cfg['cache.flashSizeMB'] = size;
        szExp.name += f'{size}MB'
        out.append(szExp)

    return out

def mem_sizes(exp, mem_sizes_mb):
    exp.name += '-memSize'
    out = []

    for size in mem_sizes_mb:
        szExp = exp.clone()
        szExp.cfg['cache.memorySizeMB'] = size;
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

def log_dram_ratio(avg_object_size, scaling):
    return overhead / (avg_object_size * scaling)

def fb_tao_simple(exp, scaling):
    avg_obj_size = 291
    exp.name += '-fbTaoSimple'
    exp.cfg['trace.totalKAccesses'] = numKRequests
    exp.cfg['trace.filename'] = "fb-sampled.csv"
    exp.cfg['trace.samplingSeed'] = 0
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
    else:
        base_exps = expand(base_exps, mem_sizes, memSizes)

    base_exps = expand(base_exps, slow_warmup)
    if args.cache_setup and 'set_dist' in args.cache_setup:
        base_exps = expand(base_exps, set_distribution)
    if args.cache_setup and 'set_hits_dist' in args.cache_setup:
        base_exps = expand(base_exps, set_hits_distribution)
    if args.cache_setup and 'pools' in args.cache_setup:
        base_exps = expand(base_exps, pooled_lru, args.cache_setup['pools'])

    if not args.cache_setup or 'no_flash' not in args.cache_setup :
        if args.flash_size_mb:
            base_exps = expand(base_exps, flash_sizes, args.flash_size_mb)
        else:
            base_exps = expand(base_exps, flash_sizes, flashSizes)

        if args.cache_setup and 'victim_cache' in args.cache_setup:
            sizes = args.cache_setup['victim_cache']
            base_exps = expand(base_exps, victim_cache, sizes)

        if not args.cache_setup or 'no_sets' not in args.cache_setup:
            nru = 'nru' in args.cache_setup if args.cache_setup else False
            rrip = 'rrip' in args.cache_setup if args.cache_setup else False
            if rrip and nru:
                raise ValueError("Cannot have both nru and rrip sets")
            rrip_poss = []
            if rrip:
                rrip_poss = args.cache_setup['rrip']
            set_capacities = []
            if args.cache_setup and 'set_capacities' in args.cache_setup:
                set_capacities = args.cache_setup['set_capacities']
            base_exps = expand(base_exps, sets, nru, set_capacities)
            if rrip:
                rrip_exps = []
                rrip_exps.extend(expand(base_exps, add_rrip_mixed_promotion))
                base_exps = expand(rrip_exps, add_rrip, rrip_poss)
        elif args.cache_setup and 'no_sets' in args.cache_setup:
            args.cache_setup['log_percents'] = [100]

        if args.cache_setup and 'log_percents' in args.cache_setup:
            base_exps = expand(
                    base_exps,
                    log, 
                    'adjust_flash_cap' in args.cache_setup,
                    args.cache_setup['log_percents'],
            )
            if 'rotating' in args.cache_setup:
                base_exps = expand(base_exps, rotating_log, args.cache_setup['rotating'])
            elif 'multilog' in args.cache_setup:
                base_exps = expand(base_exps, multilog, args.cache_setup['multilog'])
                
    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in base_exps]
    # exit()
    
    return base_exps

def add_traces(exps, trace_args):
    new_exps = []
    if 'zipf' in trace_args:# 将zipf负载配置添加到现有配置，组成新的配置，并把新配置添加到新配置列表末尾
        new_exps.extend(expand(exps, zipf, trace_args['zipf']))

    if 'scaling' not in trace_args:
        trace_args['scaling'] = [1]

    if 'fb_tao_simple' in trace_args:
        new_exps.extend(expand(exps, fb_tao_simple, trace_args['scaling']))

    if 'limit_requests' in trace_args:# 请求数量限制
        new_exps = expand(new_exps, limit_requests, trace_args['limit_requests'])
    return new_exps

def add_preset_admission_policies(exps, admission_args):
    new_exps = []
    if 'pre_set_random' in admission_args:
        new_exps.extend(expand(exps, random_admission_pre_set, admission_args['pre_set_random']))
    if 'threshold' in admission_args:
        new_exps.extend(expand(exps, threshold, admission_args['threshold']))
    if 'readmission' in admission_args:
        new_exps = expand(new_exps, readmission, admission_args['readmission'])
    return new_exps

def add_prelog_admission_policies(exps, admission_args):
    new_exps = []
    if 'pre_log_random' in admission_args:
        new_exps.extend(expand(exps, random_admission_pre_log, admission_args['pre_log_random']))
    return new_exps

def add_admission_policies(exps, admission_args):
    new_exps = add_prelog_admission_policies(exps, admission_args)
    if 'multiple_admission_policies' in admission_args:
        new_exps = add_preset_admission_policies(new_exps, admission_args)
    else:
        new_exps.extend(add_preset_admission_policies(exps, admission_args))

    # other options relevant to admission
    if 'none' in admission_args:
        new_exps.extend(exps)
    return new_exps

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
    
    # 内存缓存大小，使用--mem-size-MB来指定这个参数的值，可以指定多个值
    parser.add_argument('--mem-size-MB', dest='mem_size_mb', type=int, 
            nargs='+', metavar='size', help='defaults to correct size for tao') 
    
    # 生成的配置文件保存到的文件夹路径，使用--directory来指定
    parser.add_argument('--directory', action='store_true', help=f'generate config in specified directory')
    
    # 统计信息的输出间隔，使用--stats-interval来指定
    parser.add_argument('--stats-interval', help='10**(STATS_INTERVAL)', type=int)

    # 负载类型
    traces = parser.add_argument_group('trace types (at least one required)')
    # facebook tao负载
    traces.add_argument('--fb-simple', dest='traces', action=LayeredAction, const='fb_tao_simple')
    # zipf负载，之后可以指定多个alpha值，默认为0.8
    traces.add_argument('--zipf', dest='traces', action=LayeredAction, 
            const='zipf', default=[0.8], nargs='*', type=float, metavar='alpha', help='defaults to .8')
    # 限制最多读取多少个请求
    traces.add_argument('--limit-requests', dest='traces', action=LayeredAction, const='limit_requests', 
            nargs=1, metavar='numKRequests')
    # 对象大小缩放比例，可以指定多个
    traces.add_argument('--obj-scaling', dest='traces', action=LayeredAction, const='scaling', help='defaults to no scaling', 
            nargs='+', metavar='multiple', default=[1], type=float)

    cache_setup = parser.add_argument_group('cache setup')
    # 日志缓存比例，可以指定多个值
    cache_setup.add_argument('--log', dest='cache_setup', action=LayeredAction, const='log_percents',
            nargs='+', metavar='percent')
    # ？
    cache_setup.add_argument('--adjust-up-flash-capacity', dest='cache_setup', 
            action=LayeredAction, const='adjust_flash_cap')
    # ？
    cache_setup.add_argument('--no-flash', dest='cache_setup', action=LayeredAction, const='no_flash')
    # 
    cache_setup.add_argument('--rotating-kb', dest='cache_setup', action=LayeredAction, const='rotating', nargs='+')
    # 
    cache_setup.add_argument('--set-caps', dest='cache_setup', action=LayeredAction, const='set_capacities', nargs='+')
    # rrip使用的bit数
    cache_setup.add_argument('--rrip', dest='cache_setup', action=LayeredAction, const='rrip', metavar='bits', nargs='+')
    cache_setup.add_argument('--no-sets', dest='cache_setup', action=LayeredAction, const='no_sets')

    admission = parser.add_argument_group('admission policies')
    # 准入阈值
    admission.add_argument('--threshold', dest='admission_policies', action=LayeredAction, 
            const='threshold', nargs='+', metavar='num', type=int)
    # set使用随机准入，并指定准入比例
    admission.add_argument('--pre-set-random', dest='admission_policies', action=LayeredAction, 
            const='pre_set_random', nargs='+', metavar='admit ratio', type=float)
    # log使用随机准入，并指定准入比例
    admission.add_argument('--pre-log-random', dest='admission_policies', action=LayeredAction, 
            const='pre_log_random', nargs='+', metavar='admit ratio', type=float)
    # 不使用准入策略
    admission.add_argument('--no-admission', dest='admission_policies', action=LayeredAction, const='none')
    # 使用多个准入策略
    admission.add_argument('--multiple-admission-policies', dest='admission_policies', 
            action=LayeredAction, const='multiple_admission_policies', help='Needed for both pre-log and pre-set admission policy')
    # 重新进行准入的阈值，对象被命中多少次后允许重新准入
    admission.add_argument('--readmission', dest='admission_policies', action=LayeredAction, 
            const='readmission', nargs='+', metavar='numHits', type=int)


    args = parser.parse_args()
    return args

def main():
    # 
    args = arg_parser()
    # print(args)
    # exit()
    if not args.traces:
        raise ValueError("Need at least one trace type")
    
    exps = generate_base_exps(args)
    # [print(f'name:\n  {i.name} \ncfg:\n{i.cfg}') for i in exps]
    # exit()
    
    # 添加准入策略配置信息
    if args.admission_policies is not None:
        exps = add_admission_policies(exps, args.admission_policies)
        
    # 添加负载配置信息
    exps = add_traces(exps, args.traces)

    now = datetime.now()
    time_str = now.strftime('%Y-%m-%d_%H-%M-%S')
    dirname = f'/n/corgi/results/kangaroo/{time_str}-{args.name}'
    if not args.directory:
        dirname = '.'
    (Path(dirname)/'configs').mkdir(exist_ok=True, parents=True)
    (Path(dirname)/'output').mkdir(exist_ok=True)
    if args.stats_interval:
        exps = expand(exps, stat_outfile, dirname, args.stats_interval)
    else: 
        exps = expand(exps, stat_outfile, dirname)

    for exp in exps:
        print(f'{dirname}/configs/{exp.name}.cfg', '...')
        exp.cfg.writeToFile(f'{dirname}/configs/{exp.name}.cfg')

if __name__ == "__main__":
    main()
