# 1. PandoraSketch P4 Implementation
1. define a 2-row sketch considering the limited number of pipeline stages in Tofino1
2. define flag, flow_key, value and inactivity as registers, respectively
3. Map the random number and probability to the range 0-255, and obtain the probability calculation through a lookup table
4. The control plane periodically updates inactivity at the end of each time window based on the flag status, and resets the flag for the next time window.

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
rm -rf /root/bf-sde-8.9.1/pkgsrc/p4-build/tofinopd/PandoraSketch
./configure --prefix=$SDE_INSTALL --with-tofino P4_NAME=PandoraSketch P4_PATH=/root/bf-sde-8.9.1/pkgsrc/p4-examples/programs/PandoraSketch/PandoraSketch.p4 P4_VERSION=p4-14 --enable-thrift
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
./run_switchd.sh -p PandoraSketch
```