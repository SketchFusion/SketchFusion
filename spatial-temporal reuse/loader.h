#pragma once
#include <stdint.h>
#include <string>
#include <arpa/inet.h>

// 压缩后的单条数据（与Sketch接口一致）
typedef struct {
    uint64_t item;          // (src<<32)|dst, 均为host order
    uint32_t packetsize;    // Length(bytes)
    double ts;              // 时间戳（秒），来自pcap记录头：ts_sec + ts_frac
} dataItem;

// 数据加载类
class Loader {
    typedef struct {
        unsigned char* databuffer = nullptr;
        uint64_t cnt = 0;
        uint64_t cur = 0;
        unsigned char* ptr = nullptr;
        uint64_t buf_size = 0;
    } loader;

public:
    Loader(std::string filename, uint64_t buffersize);
    ~Loader();

    int GetNext(dataItem* dataitem);
    void Reset();
    uint64_t GetDataSize();
    uint64_t GetCurrent();

    loader* data = nullptr;
};
