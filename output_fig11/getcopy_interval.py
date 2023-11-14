import sys
import csv
import os
import sys

cwd = os.getcwd()

benchmarks = ["quick","recursive_4","recursive_8","recursive_16"]

def get_files(dirname, files):
    for (dirpath, dirnames, filenames) in os.walk(cwd):
        if (cwd+'/'+dirname+'/' in dirpath):
            for filename in filenames:
                if ("gemos.out" in filename and ("interval" in dirpath or "1ms" in dirpath or "5ms" in dirpath or "10ms" in dirpath)):
                    filepath = os.path.join(dirpath,filename)
                    files.append(filepath)
    #print(files)

def get_data(data,files):
    #print("get data")
    #print(files)
    for fil in files:
        scheme = None
        interval = None
        if "quick" in fil:
            scheme = "quick"
        else:
            scheme = fil.split('/')[-3]
        interval = fil.split('/')[-2]
        #print(scheme, interval)
        with open(fil,"r") as f:
            num = 0
            copy_time = 0
            copy_time_total = 0
            copy_size = 0
            for line in f:
                if ('time' in line):
                    temp_line = line.replace(",","")
                    temp_line = temp_line.replace(":"," ")
                    l = temp_line.split()
                    _size  = int(l[5])
                    copy_size += _size
                    num += 1
                    copy_time_total += int(l[3])
                    if(_size > 0):
                        copy_time += int(l[3])
                    #copy_size += _size
            #print(scheme,interval, num, copy_size, copy_time, copy_time_total)
            data[scheme][interval]["number"] = num
            data[scheme][interval]["copy_time"] = int(copy_time/num)
            data[scheme][interval]["copy_size"] = int(copy_size/num)

if __name__=="__main__":
    para = "copy_size"
    final_data = {}
    files = []
    for bench in benchmarks:
        final_data[bench] = {"1ms":{"number":0,"copy_time":0,"copy_size":0},
            "5ms":{"number":0,"copy_time":0,"copy_size":0},
            "10ms":{"number":0,"copy_time":0,"copy_size":0}}
        get_files(bench, files)
    get_data(final_data, files)
    #print(final_data)
    
    with open('{0}_interval.csv'.format(para), 'w', newline='') as csvfile:
        fieldnames = ['bench', '1ms','5ms','10ms']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        #print(final_data)
        for k1,v1 in final_data.items():
            d = {}
            for k2,v2 in v1.items():
                d[k2] = v2[para]
            writer.writerow({'bench':k1,'1ms':d["1ms"],'5ms':d["5ms"],
                '10ms':d["10ms"]})
