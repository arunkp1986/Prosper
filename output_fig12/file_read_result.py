import sys
import csv
import os
import sys

cwd = os.getcwd()
files=[]
data = {"mcf_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0}, "omnetpp_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},
        "perlbench_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"leela_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"g500_sssp":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"gaps":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"stream":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0}}

vanilla = {"mcf_s":0, "omnetpp_s":0,"perlbench_s":0,"leela_s":0,"g500_sssp":0,"gaps":0,"stream":0}

benchmarks = ["leela_s","mcf_s","omnetpp_s","perlbench_s","g500_sssp","gaps","stream"]
def get_files(files):
    for (dirpath, dirnames, filenames) in os.walk(cwd):
        if (cwd+'/linux' in dirpath):
            for filename in filenames:
                if ("result.out" in filename):
                    filepath = os.path.join(dirpath,filename)
                    files.append(filepath)

if __name__=="__main__":
    get_files(files)
    for fil in files:
        with open(fil,"r") as f:
            bench = (fil.split("/")[-1]).replace("_result.out","")
            if "vanilla" in fil:
                for line in f:
                    if "vanilla" in line:
                        instr = line.split()[1]
                        cycle = line.split()[2]
                        vanilla[bench] = float(instr)/float(cycle)
            else:
                for line in f:
                    size = line.split()[0]
                    instr = line.split()[1]
                    cycle = line.split()[2]
                    if size in data[bench].keys():
                        data[bench][size] = float(instr)/float(cycle)
    
    d = {"mcf_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0}, "omnetpp_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},
        "perlbench_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"leela_s":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"g500_sssp":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"gaps":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0},"stream":{"Dirtybit":0,"8bytes":0,"64bytes":0,"128bytes":0}}

    for k1,v1 in data.items():
        for k2,v2 in v1.items():
            d[k1][k2] = v2/vanilla[k1]
    
    fieldnames = ['bench']+list(d["mcf_s"].keys())
    with open('result.csv', 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for k1,v1 in d.items():
            x = {'bench':k1}
            z = {**x,**v1}
            writer.writerow(z)
