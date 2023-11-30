# Prosper: Program Stack Persistence in Hybrid Memory Systems
Prosper is a hardware-software (OS) co-designed checkpoint approach for stack persistence. Prosper tracks stack changes at sub-page byte granularity in hardware, allowing symbiosis with OS to realize efficient checkpoints of the stack region.

### Building and Running Prosper
- Install Docker on your system, we used Ubuntu 20.04.3.
- Download Docker export file [prosper.tar](https://drive.google.com/file/d/15zgZGVF875KMg2COBpXdJpEJAlfV88Jr/view?usp=sharing), size ~ 1GB
- Import downloaded **prosper.tar** using `docker import prosper.tar prosper:latest`
- List the image using `docker image ls`
- Start the docker container `docker run -it --privileged prosper:latest /bin/bash`
- All steps mentioned below are to be performed inside container.
- Change to home directory inside the docker container `cd /home`
- Clone this git repo using `git clone --recurse-submodules https://github.com/arunkp1986/Prosper.git` inside the running container.
- Download linux disk image (linux_disk.xz) `gdown https://drive.google.com/uc?id=1QPTfRPezp3P2YrPnOFRisFDhPBD0N3Es` to **Prosper/disk_image**.
- Extract all **.xz** files in **Prosper/disk_image** directory ( data_gapbs.img.xz, data_sssp.img.xz, data_workloadb.img.xz, gemos.img.xz, and linux_disk.xz), for example `xz -v -d data_gapbs.img.xz`
- Bash scripts to generate output corresponding to each plot in paper is named after the Figure number, **run_fig8.sh** corresponds to **Figure 8** in paper. Similarly remaining scripts are **run_fig9.sh**, **run_fig10.sh**, **run_fig11.sh**, **run_fig12.sh** and **run_fig13.sh**. You need to run only these scripts.
- Execute `./run_fig8.sh`
- Press **enter** to continue while asked for when running scripts (the question is part of gem5 building).
- Note that Linux experiments (**run_fig12.sh**, **run_fig13.sh**) requires 32GB DRAM (configured in gem5_prosper_linux/configs/spec_config/system/system.py)
- Note that romulus in **run_fig8.sh** takes ~20+ hours to run.
- Bash scripts build **gem5** and **gemOS kernel** required for running experiments and generate output in folders designated with Figure number, for example, `./run_fig8.sh` saves output in **output_fig8** for **Figure 8** in paper.
- Execute **format_output.sh** in output directory to run Python script and parse output files and generate **\*result.out**.
- Expected output file, **\*expected.out**, is provided in corresponding output folder for comparison.
- This repository also contains implementation of state-of-the-art memory persistence mechanisms, **SSP** and **Romulus**, used for comparison with Prosper.

