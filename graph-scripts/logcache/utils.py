import re
import json
import glob

def parse_file_name(file_name):
    # 定义每个参数的正则表达式模式
    patterns = {
        'flashSize': r'flashSize(\d+)MB',
        'cacheSize': r'cacheSize(\d+)MB',
        'cacheAlgo': r'cacheAlgo(\w+)',
        'logType': r'logType(\w+)',
        'blockSize': r'blockSize(\d+)',
        'segmentSize': r'segmentSizeMB(\d+)',
        'enableGC': r'enableGC(\d+)',
        'zipf': r'zipf([\d.]+)',
        'numKRequests': r'numKRequests(\d+)'
    }
    
    config = {}
    
    # 逐个匹配每个参数
    for key, pattern in patterns.items():
        match = re.search(pattern, file_name)
        if match:
            if key in ['flashSize', 'cacheSize', 'blockSize', 'segmentSize', 'enableGC', 'numKRequests']:
                config[key] = int(match.group(1))
            elif key == 'zipf':
                config[key] = float(match.group(1))
            else:
                config[key] = match.group(1)
        else:
            config[key] = None
    
    return config

def to_json_list(content):
    all_data = []
    # 将文件内容按行分割
    json_objects = json.loads('[' + content.replace('}\n{', '},{') + ']')
    # 解析JSON数组
    all_data.extend(json_objects)
    return all_data

def to_result_list(json_list):
    miss_ratio_list=[]
    write_amp_list=[]
    capacity_util_list=[]
    request_count_list=[]
    for entry in json_list:
        # print(entry)
        if 'totalAccesses' not in entry['global']:
            continue
        request_num=entry['global']['totalAccesses']
        # print(request_num)
        log = entry['log']
        miss_ratio=log['misses']/request_num
        write_amp=log['bytes_written']/log['request_bytes_written']
        capacity_util=log['current_size']/log['logCapacity']
        request_count_list.append(request_num)
        miss_ratio_list.append(miss_ratio)
        write_amp_list.append(write_amp)
        capacity_util_list.append(capacity_util)
    return request_count_list, miss_ratio_list, write_amp_list, capacity_util_list

if __name__ == '__main__':
    # 使用示例
    pattern = '../../run-scripts/logcache/output/LogCache-flashSize100MB-cacheSize90MB-cacheAlgoFIFO-blockSize4096-segmentSizeMB2-enableGC1-zipf0.9-numKRequests10000.out'
    json_list = read_json_files(pattern)

    print(f'Found {len(json_list)} entries')
    # print(data[0])
    # print(data[1])
    # print(data[len(data) - 1])

    print(json_list[1]['log'])

    exit()

    # 打印读取的数据
    for entry in data:
        print(entry)