#include <stdint.h>
#include <stdio.h>
#include <vector>

namespace TdmSync {

struct SegmentUse {
    int64_t srcOffset = 0;
    int64_t dstOffset = 0;
    int64_t size = 0;
    bool remote = false;
};

struct SyncPlan {
    std::vector<SegmentUse> segments;
    int64_t bytesLocal = 0;
    int64_t bytesRemote = 0;

    void print(FILE *f = stdout) const;
};

#pragma pack(push, 1)
struct BlockInfo {
    static const int HASH_SIZE = 20;
    int64_t offset = 0;
    uint32_t chksum = 0;
    uint8_t hash[HASH_SIZE];
};
#pragma pack(pop)

struct FileInfo {
    int64_t fileSize = 0;
    int blockSize = 0;
    std::vector<BlockInfo> blocks;

    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t> &infoContents);

    void computeFromFile(const std::vector<uint8_t> &fileContents, int blockSize);
    SyncPlan createUpdatePlan(const std::vector<uint8_t> &fileContents) const;
};

}
