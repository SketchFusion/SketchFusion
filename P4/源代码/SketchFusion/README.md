# 1. SketchFusion P4 Implementation
1. define a 2-row sketch considering the limited number of pipeline stages in Tofino1
2. assume that each item belongs to only one type of task
3. define common task_id, flow_key, and value registers for four different types of sketches: StableSketch, PandoraSketch, AionSketch, and BurstSketch. The task_id ranges from 0 to 3, corresponding to different task types; flow_key stores the fingerprint of each data item; and value represents the frequency for the first three sketches and the cur_win size for the last one.
4. define two additional registers, extra_reg1 and extra_reg2. Specifically, extra_reg1 is used to store stable, inactivity, period, and pre_win, while extra_reg2 is used to store the additional flag and time fields required by PandoraSketch and BurstSketch.

# 2 P4 Program Compilation, Installation, and Configuration

## 2.1 Configure Environment Variables
```bash
export SDE=/root/bf-sde-8.9.1
export SDE_INSTALL=/root/bf-sde-8.9.1/install
export PATH=$PATH:$SDE_INSTALL/bin
```

## 2.2 Load Driver
```bash
bash /root/bf-sde-8.9.1/install/bin/bf_kdrv_mod_load $SDE_INSTALL/
```

## 2.3 Compile the P4 Program
```bash
cd /root/bf-sde-8.9.1/pkgsrc/p4-build
rm -rf /root/bf-sde-8.9.1/pkgsrc/p4-build/tofinopd/SketchFusion
./configure --prefix=$SDE_INSTALL --with-tofino P4_NAME=SketchFusion P4_PATH=/root/bf-sde-8.9.1/pkgsrc/p4-examples/programs/SketchFusion/SketchFusion.p4 P4_VERSION=p4-14 --enable-thrift
make
make install
```

## 2.4 Generate Configuration Files
```bash
cd /root/bf-sde-8.9.1/pkgsrc/p4-examples
./configure --prefix=$SDE_INSTALL

make
make install
```

## 2.5 Run
```bash
cd ~/bf-sde-8.9.1
./run_switchd.sh -p SketchFusion
```