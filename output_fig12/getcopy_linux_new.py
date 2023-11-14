import sys
import csv
import os
import sys

cwd = os.getcwd()

#benchmarks = ["deepsjeng_s","leela_s","mcf_s","omnetpp_s","perlbench_s"]
benchmarks = ["leela_s","mcf_s","omnetpp_s","perlbench_s","g500_sssp","gaps","stream"]
#benchmarks = ["g500_sssp","gaps","stream"]
def get_files(dirname, files):
    for (dirpath, dirnames, filenames) in os.walk(cwd):
        if (cwd+'/linux/'+dirname in dirpath) and('hw' in dirpath or 'hwsw' in dirpath):
            for filename in filenames:
                if ("stats.txt" in filename):
                    filepath = os.path.join(dirpath,filename)
                    files.append(filepath)
    #print(files)

copy_size_all = {}
param_list = ["system.detailed_cpu.exec_context.thread_0.numUsrInsts","system.detailed_cpu.numUsrCycles","system.detailed_cpu.bitmapStores",
"system.detailed_cpu.lookupFull","system.detailed_cpu.evictStores","system.detailed_cpu.redundantStores","system.detailed_cpu.watermarkStores",
"system.detailed_cpu.stackStores","system.detailed_cpu.flushStores","system.detailed_cpu.exec_context.thread_0.numStoreInsts","simSeconds"]



def get_data(dirname,data,files):
    for fil in files:
        size = None
        size = fil.split('/')[-3]
        typename = "hw"
        if("hwsw" in fil):
            typename = "hwsw"
        with open(fil,"r") as f:
            for line in f:
                if (param_list[0] in line):
                    l = line.split()[1]
                    data[size][typename]['numUsrInsts'] = l 
                elif (param_list[1] in line):
                    l = line.split()[1]
                    data[size][typename]['numUsrCycles'] = l
                elif (param_list[2] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['bitmapStores'] == 0):
                            data[size][typename]['bitmapStores'] = l 
                    except:
                        pass
                elif (param_list[3] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['lookupFull'] == 0):
                            data[size][typename]['lookupFull'] = l 
                    except:
                        pass
                elif (param_list[4] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['evictStores'] == 0):
                            data[size][typename]['evictStores'] = l 
                    except:
                        pass
                elif (param_list[5] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['redundantStores'] == 0):
                            data[size][typename]['redundantStores'] = l 
                    except:
                        pass
                elif (param_list[6] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['watermarkStores'] == 0):
                            data[size][typename]['watermarkStores'] = l 
                    except:
                        pass
                elif (param_list[7] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['stackStores'] == 0):
                            data[size][typename]['stackStores'] = l 
                    except:
                        pass
                elif (param_list[8] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['flushStores'] == 0):
                            data[size][typename]['flushStores'] = l 
                    except:
                        pass
                elif (param_list[9] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['numStoreInsts'] == 0):
                            data[size][typename]['numStoreInsts'] = l 
                    except:
                        pass
                elif (param_list[10] in line):
                    l = line.split()[1]
                    try:
                        if(data[size][typename]['simSeconds'] == 0):
                            data[size][typename]['simSeconds'] = l 
                    except:
                        pass
     

if __name__=="__main__":
    final_data = {}
    for bench in benchmarks:
        files = []
        final_data[bench] = {"0bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}},
                "8bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}},
                "16bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}},
                "32bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}},
                "64bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}},
                "128bytes":{"hw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0},"hwsw":{"numUsrInsts":0,"numUsrCycles":0,"bitmapStores":0,"lookupFull":0,"evictStores":0,"redundantStores":0,"watermarkStores":0,"stackStores":0,"flushStores":0,"numStoreInsts":0,"simSeconds":0}}}
        get_files(bench, files)
        #print(files)
        get_data(bench, final_data[bench], files)
        #print(final_data)
    for k1,v1 in final_data.items():
        with open('{0}_result.csv'.format(k1), 'w', newline='') as csvfile:
            fieldnames = ['size', 'numUsrInsts','numUsrCycles']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            for k2,v2 in v1.items():
                if(k2 in ["16bytes","32bytes"]):
                    continue
                for k3,v3 in v2.items():
                    if(k3 =="hwsw"):
                        if(k2 == "0bytes"):
                            writer.writerow({'size':"Dirtybit",'numUsrInsts':v3["numUsrInsts"],'numUsrCycles':v3["numUsrCycles"]})
                        else:
                            writer.writerow({'size':k2,'numUsrInsts':v3["numUsrInsts"],'numUsrCycles':v3["numUsrCycles"]})
                            
