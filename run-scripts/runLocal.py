#!/usr/bin/python3

import glob, subprocess, sys, argparse, multiprocessing, os

parser = argparse.ArgumentParser()
# 要测试的配置文件所在的目录列表，可以有多个目录
# parser.add_argument("dirs", nargs="+", help="Experiment directories")
# nargs='*' 允许参数接受零个或多个值
parser.add_argument('--dirs', nargs='*',
                    default=['./configs/log/', './configs/set/', './configs/kangaroo/'],
                    help='Experiment directories')
# 并行运行的进程数
parser.add_argument("--jobs", default=1, type=int)
# # 解析命令行参数
args = parser.parse_args()

def runExperiment(cfg):
    print(f'Running {cfg}...')

    expName = os.path.splitext(cfg)[0]
    
    cmd = f'../simulator/bin/cache {cfg}'
    print (cmd)

    if subprocess.run(cmd, shell=True).returncode == 0:
        print(f'{cfg} finished successfully\n\n')
    else:
        print(f'Error running {cfg}!\n\n')
                
if __name__ == "__main__":
    dirs = args.dirs

    configs = []
    
    # 遍历每个目录
    for d in dirs:
        expDir = f'{d}'
        print('Running experiments in %s...' % expDir)
        # 使用glob.glob函数搜索当前目录expDir下所有以.cfg为扩展名的文件
        # 并将这些文件的路径追加到名为configs的列表中
        # glob.glob函数接受一个路径名模式（在这个例子中是expDir + '/*.cfg'）
        # 并返回所有匹配该模式的文件路径列表
        configs += glob.glob(expDir + '/*.cfg')
    
    # 使用multiprocessing.Pool类创建一个进程池
    # 进程池中的进程数由参数args.jobs指定
    with multiprocessing.Pool(args.jobs) as pool:
        pool.map(runExperiment, configs)

    print('Done.')
