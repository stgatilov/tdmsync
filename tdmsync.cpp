#include "tdmsync.h"
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include "sha1.h"
#include "buzhash.h"
#include "binsearch.h"


namespace TdmSync {

std::string assertFailedMessage(const char *code, const char *file, int line) {
    char buff[256];
    sprintf(buff, "Assertion %s failed in %s on line %d", code, file, line);
    return buff;
}
#define TdmSyncAssert(cond) if (!(cond)) throw std::runtime_error(assertFailedMessage(#cond, __FILE__, __LINE__));

//===========================================================================

uint32_t checksumDigest(uint32_t value) {
    return value;
}

uint32_t checksumCompute(const uint8_t *bytes, size_t len) {
    TdmSyncAssert((len & 31) == 0);
    return buzhash_compute(bytes, len);
}

uint32_t checksumUpdate(uint32_t value, uint8_t added, uint8_t removed) {
    return buzhash_fast_update(value, added, removed);
}

void hashCompute(uint8_t hash[20], const uint8_t *bytes, uint32_t len) {
    SHA1_CTX sha;
    SHA1Init(&sha);
    SHA1Update(&sha, bytes, len);
    SHA1Final(hash, &sha);
}

//===========================================================================

std::vector<uint8_t> FileInfo::serialize() const {
    int blocksCount = blocks.size();
    int64_t dataSize = sizeof(fileSize) + sizeof(blockSize) + sizeof(blocksCount) + blocks.size() * sizeof(BlockInfo);
    TdmSyncAssert(dataSize < INT_MAX / 2);

    std::vector<uint8_t> data(dataSize);
    int ptr = 0, sz;

    sz = sizeof(fileSize);
    memcpy(&data[ptr], &fileSize, sz);
    ptr += sz;

    sz = sizeof(blockSize);
    memcpy(&data[ptr], &blockSize, sz);
    ptr += sz;

    sz = sizeof(blocksCount);
    memcpy(&data[ptr], &blocksCount, sz);
    ptr += sz;

    sz = blocks.size() * sizeof(BlockInfo);
    memcpy(&data[ptr], blocks.data(), sz);
    ptr += sz;

    TdmSyncAssert(ptr == dataSize);
    return data;
}

void FileInfo::deserialize(const std::vector<uint8_t> &data) {
    int64_t dataSize = data.size();
    int blocksCount;
    TdmSyncAssert(dataSize >= sizeof(fileSize) + sizeof(blockSize) + sizeof(blocksCount));

    int ptr = 0, sz;

    sz = sizeof(fileSize);
    memcpy(&fileSize, &data[ptr], sz);
    ptr += sz;

    sz = sizeof(blockSize);
    memcpy(&blockSize, &data[ptr], sz);
    ptr += sz;

    sz = sizeof(blocksCount);
    memcpy(&blocksCount, &data[ptr], sz);
    ptr += sz;

    sz = blocksCount * sizeof(BlockInfo);
    TdmSyncAssert(ptr + sz == dataSize);
    blocks.resize(blocksCount);
    memcpy(blocks.data(), &data[ptr], sz);
    ptr += sz;

    TdmSyncAssert(ptr == dataSize);
}


bool operator< (const BlockInfo &a, const BlockInfo &b) {
    return a.chksum < b.chksum;
}
void FileInfo::computeFromFile(const std::vector<uint8_t> &fileContents, int blockSize) {
    this->blockSize = blockSize;
    fileSize = fileContents.size();
    blocks.clear();

    if (fileSize < blockSize)
        return;

    int blockCount = (fileSize + blockSize-1) / blockSize;
    blocks.reserve(blockCount);

    for (int i = 0; i < blockCount; i++) {
        int64_t offset = int64_t(i) * blockSize;
        offset = std::min(offset, fileSize - blockSize);

        BlockInfo blk;
        blk.offset = offset;
        blk.chksum = checksumDigest(checksumCompute(&fileContents[offset], blockSize));
        hashCompute(blk.hash, &fileContents[offset], blockSize);

        blocks.push_back(blk);
    }

    std::sort(blocks.begin(), blocks.end());
}

bool operator< (const SegmentUse &a, const SegmentUse &b) {
    if (a.remote != b.remote)
        return int(a.remote) > int(b.remote);       //remote first
    return a.dstOffset < b.dstOffset;
}
SyncPlan FileInfo::createUpdatePlan(const std::vector<uint8_t> &fileContents) const {
    int64_t srcFileSize = fileContents.size();
    SyncPlan result;

    if (srcFileSize >= blockSize) {
        std::vector<uint32_t> checksums(blocks.size());
        for (int i = 0; i < checksums.size(); i++)
            checksums[i] = blocks[i].chksum;
        TdmSyncAssert(std::is_sorted(checksums.begin(), checksums.end()));
        tdm_bsb_info binsearcher;
        binary_search_branchless_precompute(&binsearcher, checksums.size());

        uint32_t currChksum = checksumCompute(&fileContents[0], blockSize);
        std::vector<bool> foundBlocks(blocks.size(), false);

        for (int64_t offset = 0; offset + blockSize <= srcFileSize; offset++) {
            //if ((offset & ((1<<20)-1)) == 0) fprintf(stderr, "%d\n", (int)offset);
            uint32_t digest = checksumDigest(currChksum);
            int left = binary_search_branchless_run(&binsearcher, checksums.data(), digest);
            int right = left;
            while (right < checksums.size() && checksums[right] == digest)
                right++;

            int newFound = 0;
            for (int j = left; j < right; j++) if (!foundBlocks[j])
                newFound++;

            if (newFound > 0) {
                BlockInfo tmp;
                hashCompute(tmp.hash, &fileContents[offset], blockSize);
                for (int j = left; j < right; j++) if (!foundBlocks[j]) {
                    if (memcmp(blocks[j].hash, tmp.hash, sizeof(tmp.hash)) != 0)
                        continue;

                    foundBlocks[j] = true;
                    SegmentUse seg;
                    seg.srcOffset = offset;
                    seg.dstOffset = blocks[j].offset;
                    seg.size = blockSize;
                    seg.remote = false;
                    result.segments.push_back(seg);
                }
            }

            if (offset + blockSize < srcFileSize)
                currChksum = checksumUpdate(currChksum, fileContents[offset + blockSize], fileContents[offset]);
        }
    }

    int n = 0;
    if (!result.segments.empty()) {
        std::sort(result.segments.begin(), result.segments.end());
        n = 1;
        for (int i = 1; i < result.segments.size(); i++) {
            const auto &curr = result.segments[i];
            auto &last = result.segments[n-1];
            if (last.dstOffset + last.size == curr.dstOffset && last.srcOffset + last.size == curr.srcOffset)
                last.size += curr.size;
            else
                result.segments[n++] = curr;
        }
        result.segments.resize(n);
    }

    int64_t lastCovered = 0;
    for (int i = 0; i <= n; i++) {
        int64_t offset = i < n ? result.segments[i].dstOffset : fileSize;
        int64_t size = i < n ? result.segments[i].size : 0;
        if (offset > lastCovered) {
            SegmentUse seg;
            seg.srcOffset = lastCovered;
            seg.dstOffset = lastCovered;
            seg.size = offset - lastCovered;
            seg.remote = true;
            result.segments.push_back(seg);
        }
        lastCovered = offset + size;
    }

    n = result.segments.size();
    for (int i = 0; i < n; i++) {
        const auto &seg = result.segments[i];
        (seg.remote ? result.bytesRemote : result.bytesLocal) += seg.size;
    }

    return result;
}

//===========================================================================

void SyncPlan::print(FILE *f) const {
    fprintf(f, "Total bytes:  local=%" PRId64 "  remote=%" PRId64 "\n", bytesLocal, bytesRemote);
    fprintf(f, "Segments = %d:\n", (int)segments.size());
    for (int i = 0; i < segments.size(); i++) {
        const auto &seg = segments[i];
        fprintf(f, "  %c %08X: %08" PRIX64 " <- %08" PRIX64 "\n", (seg.remote ? 'R' : 'L'), (int)seg.size, seg.dstOffset, seg.srcOffset);
    }
    fprintf(f, "\n");
}

}
