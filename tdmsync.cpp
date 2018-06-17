#include "tdmsync.h"
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include "tsassert.h"

#include "sha1.h"
#include "buzhash.h"

#define USE_PHF

#ifndef USE_PHF
    #include "binsearch.h"
#else
    #include "phf.h"
#endif


namespace TdmSync {

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

static const char MAGIC_STRING[] = "tdmsync.";

void FileInfo::serialize(BaseFile &wrFile) const {
    wrFile.write(MAGIC_STRING, strlen(MAGIC_STRING));

    uint64_t blocksCount = blocks.size();
    wrFile.write(&fileSize, sizeof(fileSize));
    wrFile.write(&blockSize, sizeof(blockSize));
    wrFile.write(&blocksCount, sizeof(blocksCount));
    wrFile.write(blocks.data(), blocksCount * sizeof(BlockInfo));

    wrFile.write(MAGIC_STRING, strlen(MAGIC_STRING));
}

void FileInfo::deserialize(BaseFile &rdFile) {
    char magic[sizeof(MAGIC_STRING)] = {0};
    rdFile.read(magic, strlen(MAGIC_STRING));
    TdmSyncAssert(strcmp(magic, MAGIC_STRING) == 0);

    uint64_t blocksCount;
    rdFile.read(&fileSize, sizeof(fileSize));
    rdFile.read(&blockSize, sizeof(blockSize));
    rdFile.read(&blocksCount, sizeof(blocksCount));
    blocks.resize(blocksCount);
    rdFile.read(blocks.data(), blocksCount * sizeof(BlockInfo));

    rdFile.read(magic, strlen(MAGIC_STRING));
    TdmSyncAssert(strcmp(magic, MAGIC_STRING) == 0);
}


static void readToBuffer(BaseFile &rdFile, std::vector<uint8_t> &buffer, size_t readmore) {
    memmove(buffer.data(), buffer.data() + readmore, buffer.size() - readmore);
    rdFile.read(buffer.data() + buffer.size() - readmore, readmore);
}

bool operator< (const BlockInfo &a, const BlockInfo &b) {
    return a.chksum < b.chksum;
}
void FileInfo::computeFromFile(BaseFile &rdFile, int blockSize) {
    this->blockSize = blockSize;
    fileSize = rdFile.getSize();
    TdmSyncAssert(rdFile.tell() == 0);
    blocks.clear();

    if (fileSize < blockSize)
        return;

    int blockCount = (fileSize + blockSize-1) / blockSize;
    blocks.reserve(blockCount);

    std::vector<uint8_t> buffer(blockSize);
    int64_t offset = 0;
    for (int i = 0; i < blockCount; i++) {
        int64_t readmore = std::min(fileSize - offset, int64_t(blockSize));
        readToBuffer(rdFile, buffer, readmore);
        offset += readmore;

        BlockInfo blk;
        blk.offset = offset - blockSize;
        blk.chksum = checksumDigest(checksumCompute(buffer.data(), blockSize));
        hashCompute(blk.hash, buffer.data(), blockSize);

        blocks.push_back(blk);
    }
    TdmSyncAssert(offset == fileSize);
    TdmSyncAssert(rdFile.tell() == fileSize);

    std::sort(blocks.begin(), blocks.end());
}

bool operator< (const SegmentUse &a, const SegmentUse &b) {
    return a.dstOffset < b.dstOffset;
}
UpdatePlan FileInfo::createUpdatePlan(BaseFile &rdFile) const {
    int64_t srcFileSize = rdFile.getSize();
    TdmSyncAssert(rdFile.tell() == 0);
    UpdatePlan result;

    if (srcFileSize >= blockSize) {
        size_t num = blocks.size();
        std::vector<uint32_t> checksums(num);
        for (int i = 0; i < num; i++)
            checksums[i] = blocks[i].chksum;
        TdmSyncAssert(std::is_sorted(checksums.begin(), checksums.end()));
        #ifdef USE_PHF
        TdmPhf::PerfectHashFunc perfecthash;
        perfecthash.create(checksums.data(), num);
        #else
        tdm_bsb_info binsearcher;
        binary_search_branchless_precompute(&binsearcher, num);
        #endif

        std::vector<uint8_t> buffer(2 * blockSize);
        readToBuffer(rdFile, buffer, std::min((int64_t)buffer.size(), fileSize));
        uint32_t currChksum = checksumCompute(buffer.data(), blockSize);

        std::vector<char> foundBlocks(blocks.size(), false);
        uint64_t sumCount = 0;
        size_t buffPtr = blockSize;

        for (int64_t offset = 0; offset + blockSize <= srcFileSize; offset++) {
            //if ((offset & ((1<<20)-1)) == 0) fprintf(stderr, "%d\n", (int)offset);
            uint32_t digest = checksumDigest(currChksum);

            #ifdef USE_PHF
            size_t idx = perfecthash.evaluate(digest);
            #else
            size_t idx = binary_search_branchless_run(&binsearcher, checksums.data(), digest);
            #endif

            if (idx < num && checksums[idx] == digest) {
                uint32_t left = idx;
                uint32_t right = left;
                while (right < num && checksums[right] == digest)
                    right++;

                sumCount += (right - left);
                int newFound = 0;
                for (int j = left; j < right; j++) if (!foundBlocks[j])
                    newFound++;

                if (newFound > 0) {
                    uint8_t currHash[BlockInfo::HASH_SIZE];
                    hashCompute(currHash, &buffer[buffPtr - blockSize], blockSize);
                    for (int j = left; j < right; j++) if (!foundBlocks[j]) {
                        if (memcmp(blocks[j].hash, currHash, sizeof(currHash)) != 0)
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
            }

            if (offset + blockSize < srcFileSize) {
                if (buffPtr == buffer.size()) {
                    size_t readmore = std::min((int64_t)buffer.size() - blockSize, fileSize - offset - blockSize);
                    readToBuffer(rdFile, buffer, readmore);
                    buffPtr -= readmore;
                }
                currChksum = checksumUpdate(currChksum, buffer[buffPtr], buffer[buffPtr - blockSize]);
                buffPtr++;
            }
        }
        double avgCandidates = double(sumCount) / double(srcFileSize - blockSize + 1.0);
        //fprintf(stderr, "Average candidates per window: %0.3g\n", avgCandidates);
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

void UpdatePlan::print() const {
    printf("Total bytes:  local=%" PRId64 "  remote=%" PRId64 "\n", bytesLocal, bytesRemote);
    printf("Segments = %d:\n", (int)segments.size());
    for (int i = 0; i < segments.size(); i++) {
        const auto &seg = segments[i];
        printf("  %c %08X: %08" PRIX64 " <- %08" PRIX64 "\n", (seg.remote ? 'R' : 'L'), (int)seg.size, seg.dstOffset, seg.srcOffset);
    }
    printf("\n");
}

void UpdatePlan::apply(BaseFile &rdLocalFile, BaseFile &rdRemoteFile, BaseFile &wrResultFile) const {
/*    uint64_t resSize = 0;
    if (!segments.empty()) {
        const auto &last = segments.back();
        resSize = last.dstOffset + last.size;
    }
    std::vector<uint8_t> result(resSize, 0xFFU);

    for (int i = 0; i < segments.size(); i++) {
        const auto &seg = segments[i];
        const auto &fromData = seg.remote ? remoteData : localData;
        TdmSyncAssert(seg.dstOffset + seg.size <= result.size());
        TdmSyncAssert(seg.srcOffset + seg.size <= fromData.size());
        memcpy(result.data() + seg.dstOffset, fromData.data() + seg.srcOffset, seg.size);
    }

    return result;*/
}

}
