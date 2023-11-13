# Prosper: Program Stack Persistence in Hybrid Memory Systems
Prosper is a hardware-software (OS) co-designed checkpoint approach for stack persistence. Prosper tracks stack changes at sub-page byte granularity in hardware, allowing symbiosis with OS to realize efficient checkpoints of the stack region.

### Building and Running Prosper
- Install Docker on your system, we used Ubuntu 20.04.3.
- Download Docker export file [prosper.tar](https://iitk-my.sharepoint.com/:u:/g/personal/kparun_iitk_ac_in/EQRtpc9JHR1JlMByP-Evg6QBudagDQNgeBV2I_aBUOTshQ?e=TCvZI7), size ~ 1GB
- Import downloaded **prosper.tar** using `docker import prosper.tar prosper:latest`.
- List the image using `docker image ls`.
- Start the docker container `docker run -it prosper:latest /bin/bash`.
- Clone this git repo using `git clone --recurse-submodules` inside the running container.
- All steps mentioned below are to be performed inside container.
- Download linux disk image (linux_disk.xz) from [OneDrive](https://iitk-my.sharepoint.com/:u:/g/personal/kparun_iitk_ac_in/Eb12ZLz_oe5Brc7WtYH9a7QBO_glbhSxE9gci0HldbFKQw?e=J5tLp2) to **disk_image** directory.
- Extract all **.xz** files in **disk_image** directory ( data_gapbs.img.xz, data_sssp.img.xz, data_workloadb.img.xz, gemos.img.xz, and linux_disk.xz), for example `xz -v -d data_gapbs.img.xz`
- **Run-scripts** corresponding to each plot in paper is named after the Figure number, **run_fig8.sh** corresponds to **Figure 8** in paper.
- **Run-scripts** build **gem5** and **gemOS kernel** required for running experiments and generate output in folders designated with Figure number, for example, `./run_fig8.sh` saves output in **output_fig8** for **Figure 8** in paper.
- Python script to parse output files is provided in corresponding output folder.
- Expected output file is provided in corresponding output folder for comparison.
- Execute **Run-scripts** corresponding to Figure 12 and 13 with **sudo** permissions to use **KVM**.
- This repository also contains implementation of state-of-the-art memory persistence mechanisms, **SSP** and **Romulus**, used for comparison with Prosper.
