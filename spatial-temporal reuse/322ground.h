#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

struct FlowKey {
    std::string src_ip;
    std::string dst_ip;

    FlowKey() = default;
    FlowKey(const std::string& s, const std::string& d) : src_ip(s), dst_ip(d) {}

    bool operator==(const FlowKey& other) const {
        return src_ip == other.src_ip && dst_ip == other.dst_ip;
    }
};

struct FlowKeyHash {
    std::size_t operator()(const FlowKey& k) const {
        std::size_t h1 = std::hash<std::string>{}(k.src_ip);
        std::size_t h2 = std::hash<std::string>{}(k.dst_ip);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

class GroundTruth {
public:
    bool loadABCDLabels(const std::string& filename);

    char getLabel(const std::string& src_ip, const std::string& dst_ip) const;
    int getJobABCD(const std::string& src_ip, const std::string& dst_ip) const;

    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& getGtA() const { return gtA_packets; }
    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& getGtB() const { return gtB_bytes; }
    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& getGtC() const { return gtC_persist; }
    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& getGtD() const { return gtD_periodic; }
    const std::unordered_map<uint32_t, uint64_t>& getGtE() const { return gtE_card; }
    const std::unordered_set<FlowKey, FlowKeyHash>& getGtDBinary() const { return gtD_binary; }
    const std::unordered_set<FlowKey, FlowKeyHash>& getGtFBinary() const { return gtF_binary; }

    uint64_t totalA() const { return totalA_packets; }
    uint64_t totalB() const { return totalB_bytes; }
    uint64_t totalC() const { return totalC_persist; }
    uint64_t totalD() const { return totalD_periodic; }
    uint64_t totalE() const { return totalE_card; }

private:
    std::unordered_map<FlowKey, char, FlowKeyHash> flowLabels;
    std::unordered_map<FlowKey, uint64_t, FlowKeyHash> gtA_packets;
    std::unordered_map<FlowKey, uint64_t, FlowKeyHash> gtB_bytes;
    std::unordered_map<FlowKey, uint64_t, FlowKeyHash> gtC_persist;
    std::unordered_map<FlowKey, uint64_t, FlowKeyHash> gtD_periodic;
    std::unordered_map<uint32_t, uint64_t> gtE_card;
    std::unordered_set<FlowKey, FlowKeyHash> gtD_binary;
    std::unordered_set<FlowKey, FlowKeyHash> gtF_binary;

    uint64_t totalA_packets = 0;
    uint64_t totalB_bytes = 0;
    uint64_t totalC_persist = 0;
    uint64_t totalD_periodic = 0;
    uint64_t totalE_card = 0;
};
