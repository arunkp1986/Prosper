import sys
import csv
import os
import sys

cwd = os.getcwd()

metrics = {"simInsts":"simInsts","numCycles":"system.cpu.numCycles","demandMisses":"system.cpu.dcache.demandMisses::cpu.data","demandAvgMissLatency":"system.l3.demandAvgMissLatency::cpu.data","numInsts":"system.cpu.exec_context.thread_0.numInsts"}



benchmarks = ["ssp_dirtybit_10ms_1000us","ssp_dirtybit_10ms_100us","ssp_dirtybit_10ms_10us","ssp_prosper_10ms_1000us","ssp_prosper_10ms_100us","ssp_prosper_10ms_10us","ssp_stack_heap_10ms_1000us","ssp_stack_heap_10ms_100us","ssp_stack_heap_10ms_10us","vanilla"]


def get_files(dirname, files):
    for (dirpath, dirnames, filenames) in os.walk(cwd):
        if cwd+'/'+dirname+'/' in dirpath:
            for filename in filenames:
                if ("stats.txt" in filename):
                    filepath = os.path.join(dirpath,filename)
                    files.append(filepath)
    #print(files)

def get_data(dirname,data,files):
    for fil in files:
        bench = fil.split('/')[-2]
        #print(bench)
        with open(fil,"r") as f:
            for line in f:
                if(metrics["simInsts"] in line):
                    value = line.split()[1]
                    try:
                        data[bench]["simInsts"].append(value)
                    except:
                        print(line)
                        exit()
                if(metrics["numCycles"] in line):
                    value = line.split()[1]
                    try:
                        data[bench]["numCycles"].append(value)
                    except:
                        print(line)
                        exit() 

def populate_data(final_data,bench,data):
    for k1,v1 in data.items():
        assert len(v1["simInsts"]) == 3, "missing data in simInsts,{0},{1}".format(k1,bench)
        instructions = v1["simInsts"]
        assert len(v1["numCycles"]) == 3, "missing data in numCycles,{0},{1}".format(k1,bench)
        cycles = v1["numCycles"]
        #Here we are only getting cycles taken, not calculating ipc
        final_data[k1]["ipc"] = [int(y) for x,y in zip(instructions, cycles)]
        #final_data[k1]["ipc"] = [int(x)/int(y) for x,y in zip(instructions, cycles)]


if __name__=="__main__":
    para = "ipc"
    final_data = {}
    data = {"ssp":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "romulus":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_stack_heap":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_dirtybit":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_prosper":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "prosper":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "dirtybit":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "vanilla":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_10us_10ms":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_100us_10ms":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_1000us_10ms":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_dirtybit_10ms_1000us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_dirtybit_10ms_100us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_dirtybit_10ms_10us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_prosper_10ms_1000us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_prosper_10ms_100us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_prosper_10ms_10us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_stack_heap_10ms_1000us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_stack_heap_10ms_100us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}},
            "ssp_stack_heap_10ms_10us":{"gapbs":{"simInsts":[],"numCycles":[]},
            "sssp":{"simInsts":[],"numCycles":[]},"workloadb":{"simInsts":[],"numCycles":[]},
            "random":{"simInsts":[],"numCycles":[]}, "sparse":{"simInsts":[],"numCycles":[]},
            "stream":{"simInsts":[],"numCycles":[]}}
            }
    final_data = {"ssp":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "romulus":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_stack_heap":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_dirtybit":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_prosper":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "prosper":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "dirtybit":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "vanilla":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_10us_10ms":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_100us_10ms":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_1000us_10ms":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_dirtybit_10ms_1000us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_dirtybit_10ms_100us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_dirtybit_10ms_10us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_prosper_10ms_1000us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_prosper_10ms_100us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_prosper_10ms_10us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_stack_heap_10ms_1000us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_stack_heap_10ms_100us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}},
            "ssp_stack_heap_10ms_10us":{"gapbs":{"ipc":[]},"sssp":{"ipc":[]},
            "workloadb":{"ipc":[]},"random":{"ipc":[]},
            "sparse":{"ipc":[]},"stream":{"ipc":[]}}
            }
    for bench in benchmarks:
        #print(bench)
        files = []
        get_files(bench, files)
        #print(files)
        get_data(bench, data[bench], files)
        populate_data(final_data[bench],bench,data[bench])
    #print(data)
    #print(final_data)
    f_dic = {"type":0,"gapbs":0,"sssp":0,"workloadb":0}
    with open('execution_time.csv', 'w', newline='') as csvfile:
        fieldnames = ['type', 'gapbs','sssp','workloadb']
        writer = csv.DictWriter(csvfile, f_dic.keys())
        writer.writeheader()
        d = {}
        for k1,v1 in final_data.items():
            if(k1 in benchmarks and k1 != "vanilla"):
                d['type'] = k1
                for k2,v2 in v1.items():
                    if(k2 in ["gapbs","sssp","workloadb"]):
                        d[k2] = round(v2[para][1]/final_data["vanilla"][k2][para][1],4)
                writer.writerow(d)
