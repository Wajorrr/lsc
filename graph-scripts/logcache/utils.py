import re
import json
import glob
import os 

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
        'enabledRWPartition': r'enabledRWPartition(\d+)',
        'zipf': r'zipf([\d.]+)',
        'numKRequests': r'numKRequests(\d+)'
    }
    
    config = {}
    
    # 逐个匹配每个参数
    for key, pattern in patterns.items():
        match = re.search(pattern, file_name)
        if match:
            if key in ['flashSize', 'cacheSize', 'blockSize', 'segmentSize', 'enableGC', 'enabledRWPartition', 'numKRequests']:
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
        if(log!=None):
            if request_num==0:
                continue
                # miss_ratio=0
                # write_amp=0
                # capacity_util=0
            else:
                miss_ratio=(0 if 'misses' not in log else log['misses'])/request_num
                write_amp=log['bytes_written']/log['request_bytes_written']
                capacity_util=log['current_size']/log['logCapacity']
            request_count_list.append(request_num)
            miss_ratio_list.append(miss_ratio)
            write_amp_list.append(write_amp)
            capacity_util_list.append(capacity_util)
        else:
            read_cache=entry['read cache']
            write_cache=entry['write cache']
            if request_num==0:
                continue
                # miss_ratio=0
                # write_amp=0
                # capacity_util=0
            else:
                # print(entry['global']['misses'])
                miss_ratio=(0 if 'misses' not in entry['global'] else entry['global']['misses'])/request_num
                # print(miss_ratio)
                a = 0 if 'bytes_written' not in read_cache else read_cache['bytes_written']
                b = 0 if 'bytes_written' not in write_cache else write_cache['bytes_written']
                c = 0 if 'request_bytes_written' not in read_cache else read_cache['request_bytes_written']
                d = 0 if 'request_bytes_written' not in write_cache else write_cache['request_bytes_written']
                write_amp=(a+b)/(c+d)
                # print(read_cache['current_size'],read_cache['logCapacity'])
                # print(0 if 'current_size' not in write_cache else write_cache['current_size'],write_cache['logCapacity'])
                # print(read_cache['current_size'] + (0 if 'current_size' not in write_cache else write_cache['current_size']),read_cache['logCapacity']+write_cache['logCapacity'])
                capacity_util=(read_cache['current_size'] + (0 if 'current_size' not in write_cache else write_cache['current_size']))/(read_cache['logCapacity']+write_cache['logCapacity'])
                # print(capacity_util)
            request_count_list.append(request_num)
            miss_ratio_list.append(miss_ratio)
            write_amp_list.append(write_amp)
            capacity_util_list.append(capacity_util)
            
    return request_count_list, miss_ratio_list, write_amp_list, capacity_util_list

if __name__ == '__main__':
    # 使用示例
    # file = '../../run-scripts/logcache/zipf/zipf0.9-obj100k/output/LogCache-flashSize100MB-cacheSize90MB-blockSize4096-segmentSizeMB2-logTypeRIPQ-zipf0.9-numKRequests10000.out'
    file = '../../run-scripts/logcache/zipf/zipf0.9-obj100k/output/LogCache-flashSize100MB-cacheSize90MB-cacheAlgoFIFO-blockSize4096-segmentSizeMB2-logTypeFIFOLog-enabledRWPartition1-zipf0.9-numKRequests10000.out'
    # json_list = read_json_files(pattern)
    
    result_list=[]
    config=parse_file_name(os.path.basename(file))
    print(config)
    with open(file, 'r') as f:
        content = f.read()
        json_list=to_json_list(content)
        request_count_list, miss_ratio_list, write_amp_list, capacity_util_list = to_result_list(json_list)
        result_list.append((config, miss_ratio_list, write_amp_list, capacity_util_list))

    print(f'Found {len(json_list)} entries')
    # print(data[0])
    # print(data[1])
    # print(data[len(data) - 1])

    print(json_list[1]['log'])

    exit()

    # 打印读取的数据
    for entry in data:
        print(entry)