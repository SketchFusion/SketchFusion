# 1. StableSketch P4 Implementation
1. define a 2-row sketch considering the limited number of pipeline stages in Tofino1
2. define flow_key, value and stable as registers, respectively
3. Map the random number and probability to the range 0-255, and obtain the probability calculation through a lookup table

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
rm -rf /root/bf-sde-8.9.1/pkgsrc/p4-build/tofinopd/StableSketch
./configure --prefix=$SDE_INSTALL --with-tofino P4_NAME=StableSketch P4_PATH=/root/bf-sde-8.9.1/pkgsrc/p4-examples/programs/StableSketch/StableSketch.p4 P4_VERSION=p4-14 --enable-thrift
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
./run_switchd.sh -p StableSketch
```