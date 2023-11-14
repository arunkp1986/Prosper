import sys
import csv
import os
import sys

cwd = os.getcwd()

benchmarks = ["quick","random","sparse","stream","probnorm","probpoisson"]

def get_files(dirname, files):
    for (dirpath, dirnames, filenames) in os.walk(cwd):
        if cwd+'/'+dirname+'/' in dirpath:
            for filename in filenames:
                if (("gemos.out" in filename) and ("interval" not in dirpath)):
                    filepath = os.path.join(dirpath,filename)
                    files.append(filepath)
    #print(files)


def get_data(dirname,data,files):
    for fil in files:
        gran = (fil.split("/")[-2])
        if(gran == "dirtybit"):
            gran = "4096"
        #print(fil)
        with open(fil,"r") as f:
            num = 0
            time = 0
            size = 0
            for line in f:
                if ('time' in line):
                    temp_line = line.replace(",","")
                    temp_line = temp_line.replace(":"," ")
                    l = temp_line.split()
                    #print(l)
                    time += int(l[3])
                    num += 1
                    size += int(l[5])
            #print(dirname, bench, num, time, size)
            #print(data[gran])
            data[gran]["number"] = num
            data[gran]["copy_time"] = time
            data[gran]["copy_size"] = size

if __name__=="__main__":
    #para = "copy_time"
    para = "copy_size"
    final_data = {}
    final_data = {"quick":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}},
            "random":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}},
            "sparse":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}},
            "stream":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}},
            "probnorm":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}},
            "probpoisson":{"128":{"number":0,"copy_time":0,"copy_size":0},"64":{"number":0,"copy_time":0,"copy_size":0},
            "32":{"number":0,"copy_time":0,"copy_size":0},"16":{"number":0,"copy_time":0,"copy_size":0},
            "8":{"number":0,"copy_time":0,"copy_size":0},"4096":{"number":0,"copy_time":0,"copy_size":0}}
            }
    for bench in benchmarks:
        files = []
        get_files(bench, files)
        get_data(bench, final_data[bench], files)
        #print(bench, final_data[bench])

    with open('result_checkpoint_size.csv', 'w', newline='') as csvfile:
        fieldnames = ['bench', '128-byte','64-byte','32-byte','16-byte','8-byte','Dirtybit']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        #print(final_data)
        d = {}
        for k1,v1 in final_data.items():
            if(k1 in benchmarks):
                d['bench'] = k1
                for k2,v2 in v1.items():
                    if(k2 != "4096"):
                        key = k2+"-byte"
                    else:
                        key ="Dirtybit"
                    d[key] = round(v2[para]/v2["number"])
                writer.writerow(d)
