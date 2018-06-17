#ifndef _TDM_SYNC_H_028848_
#define _TDM_SYNC_H_028848_

#include <stdint.h>
#include <vector>
#include "fileio.h"


namespace TdmSync {

struct SegmentUse {
    int64_t srcOffset = 0;
    int64_t dstOffset = 0;
    int64_t size = 0;
    bool remote = false;
};

struct UpdatePlan {
    std::vector<SegmentUse> segments;
    int64_t bytesLocal = 0;
    int64_t bytesRemote = 0;

    void print() const;
    void apply(BaseFile &rdLocalFile, BaseFile &rdRemoteFile, BaseFile &wrResultFile) const;
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

    void serialize(BaseFile &wrFile) const;
    void deserialize(BaseFile &rdFile);

    void computeFromFile(BaseFile &rdFile, int blockSize);
    UpdatePlan createUpdatePlan(BaseFile &rdFile) const;
};

}

#endif
