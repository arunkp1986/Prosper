# Prosper: Program Stack Persistence in Hybrid Memory Systems
Prosper is a hardware-software (OS) co-designed checkpoint approach for stack persistence. Prosper tracks stack changes at sub-page byte granularity in hardware, allowing symbiosis with OS to realize efficient checkpoints of the stack region.

### Building and Running Prosper
- Start Docker using the export file [Prosper.tar]()
- clone this git repo along with submodules using  **git clone --recurse-submodules**
- Download linux disk imgae from [OneDrive](https://iitk-my.sharepoint.com/:u:/g/personal/kparun_iitk_ac_in/Eb12ZLz_oe5Brc7WtYH9a7QBO_glbhSxE9gci0HldbFKQw?e=J5tLp2) to **disk_image** directory.
- Extract disk image files in **disk_image** directory.
- **Run-scripts** corresponding to each plot in paper is named after the Figure number, **run_fig8.sh** corresponds to **Figure 8** in paper.
- Execute run script to generate output in folders designated with Figure number, **output_fig8** for **Figure 8** in paper.
- Python script to parse output files is provided in corresponding output folder.
- Expected output file is provided in corresponding output folder for comparison.
- Execute **Run-scripts** corresponding to Figure 12 and 13 with **sudo** permissions to use **KVM**.
- This repository also contains implementation of state-of-the-art memory persistence mechanisms, **SSP** and **Romulus**, used for comparison with Prosper.
