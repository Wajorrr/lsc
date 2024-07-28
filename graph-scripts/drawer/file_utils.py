import shutil
import time
import json
import os
import csv
from os.path import exists, dirname, basename, join, isfile, isdir

import pandas as pd


def load_json(path: str, verbose=False):
    start = time.time()
    if not exists(path):
        raise FileNotFoundError(f"{path} not found!")
    with open(path, "r") as f_json:
        log_data = json.load(f_json)
    if verbose:
        print(f"[INFO] File {path} loaded! Time {time.time() - start:.2f}s")
    return log_data


def save_json(obj, path, indent=0, verbose=False):
    with open(path, "w") as f:
        json.dump(obj, fp=f, indent=indent, ensure_ascii=False)
        if verbose:
            print(f"[INFO] File saved at {path}!")


def save_csv(obj, path, verbose=False):
    df = pd.DataFrame(obj)
    df.to_csv(path)
    if verbose:
        print(f"[INFO] File saved at {path}!")


def save_excel(obj, path, verbose=False):
    df = pd.DataFrame(obj)
    df.to_excel(path)
    if verbose:
        print(f"[INFO] File saved at {path}!")


def create_dir(path, verbose=False, remove_exist=False):
    if exists(path):
        if remove_exist:
            remove_dir(path, verbose=verbose)
        else:
            print(f"Directory:{path} already exist!")
            return

    os.makedirs(path)
    if verbose:
        print(f"Directory crated at {path}")


def remove_file(path, verbose=False):
    if exists(path):
        os.remove(path)
        if verbose:
            print(f"[DEBUG] File: {path} removed!")


def remove_dir(dir_path, verbose=False):
    if exists(dir_path):
        shutil.rmtree(dir_path)
        if verbose:
            print(f"[DEBUG] File: {dir_path} removed!")


def read_csv(path):
    data = []
    with open(path, 'r', newline='') as fo:
        cr = csv.reader(fo)
        for row in cr:
            data.append(row)
    return data[0], data[1:]  # header, records


def write_csv(outpath, header, records):
    with open(outpath, 'w', encoding='utf-8', newline='') as file_obj:
        writer = csv.writer(file_obj)
        if header is not None:
            writer.writerow(header)
        for record in records:
            writer.writerow(record)


def read_mape_file(mape_path):
    mape_ls = []
    with open(mape_path, 'r') as fo:
        for line in fo.readlines():
            mape_ls.append(float(line.strip()))
    return mape_ls


def write_mape_file(mape_path, mape_ls):
    with open(mape_path, 'w') as fo:
        for mape in mape_ls:
            fo.write(f'{mape}\n')


def read_sample_file(sample_file, sample_func,
                    observation_points, calc_mape_func,
                    model_func):
    header, records = read_csv(sample_file)
    sample_x, sample_y = [], []
    ret = []
    for num in observation_points:
        for i in range(len(sample_x), num):
            config = list(map(float, records[i]))
            sample_x.append(config)
            sample_y.append(sample_func(config))
        mape = calc_mape_func(sample_x, sample_y, model_func)
        ret.append(mape)
    return ret


def write_sample_file(sample_path, sample_ls):
    header=['xfersize', 'rdpct', 'seekpct', 'threads', 'cpucore', 'gc']
    write_csv(outpath=sample_path, header=header, records=sample_ls)


def read_pccs_file(pccs_path):
    pccs_ls = []
    with open(pccs_path, 'r') as fo:
        lines = fo.readlines()
        linelen = len(lines)
        for i in range(0, linelen, 2):
            val = lines[i].strip()[2:-1].split()[-1]
            pccs_ls.append(float(val))
    return pccs_ls


def write_pccs_file(pccs_path, pccs_ls):
    with open(pccs_path, 'w') as fo:
        for pccs in pccs_ls:
            fo.write(f'{pccs}\n')
