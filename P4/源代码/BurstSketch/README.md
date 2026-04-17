# 1. BurstSketch P4 Implementation
1. In stage 2, each bucket's cell1 and cell2 each form a register, and a single hash lookup retrieves the two cells corresponding to that bucket.
2. In stage 1, define one row sketch to filter potential burst flows.
3. The data plane updates the time window approximately every second, while using two window registers to alternately store frequency counts.

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
rm -rf /root/bf-sde-8.9.1/pkgsrc/p4-build/tofinopd/BurstSketch
./configure --prefix=$SDE_INSTALL --with-tofino P4_NAME=BurstSketch P4_PATH=/root/bf-sde-8.9.1/pkgsrc/p4-examples/programs/BurstSketch/BurstSketch.p4 P4_VERSION=p4-14 --enable-thrift
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
./run_switchd.sh -p BurstSketch
```