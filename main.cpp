#include <stdio.h>
#include <time.h>
#include "tdmsync.h"
#include "fileio.h"


void exit_filenotfound() {
    fprintf(stderr, "File not found!");
    exit(1);
}

void exit_usage() {
    fprintf(stderr, "Wrong arguments!");
    exit(1);
}

std::vector<uint8_t> readFile(const std::string &fn) {
    using namespace TdmSync;
    StdioFile fh;
    if (!fh.open(fn.c_str(), StdioFile::Read))
        exit_filenotfound();
    size_t filelen = fh.getSize();

    std::vector<uint8_t> buffer(filelen);
    fh.read(buffer.data(), filelen);
    return buffer;
}

void writeFile(const std::string &fn, const std::vector<uint8_t> &buffer) {
    using namespace TdmSync;
    StdioFile fh;
    if (!fh.open(fn.c_str(), StdioFile::Write))
        exit_filenotfound();
    size_t filelen = buffer.size();
    fh.write(buffer.data(), filelen);
}


int main(int argc, char **argv) {
    if (argc < 3)
        exit_usage();

    bool prepare;
    if (strcmp(argv[1], "prepare") == 0) {
        prepare = true;
    }
    else if (strcmp(argv[1], "update") == 0) {
        prepare = false;
    }
    else {
        exit_usage();
    }

    std::string dataFn = argv[2];
    std::string metaFn = dataFn + ".tdmsync";

    int starttime = clock();
    try {
        if (prepare) {
            int blockSize = argc >= 4 ? atoi(argv[3]) : 4096;

            auto dataBuf = readFile(dataFn);
            TdmSync::FileInfo info;
            info.computeFromFile(dataBuf, blockSize);

            auto metaBuf = info.serialize();
            writeFile(metaFn, metaBuf);
        }
        else {
            if (argc < 4) exit_usage();
            std::string localFn = argv[3];

            auto metaBuf = readFile(metaFn);
            TdmSync::FileInfo info;
            info.deserialize(metaBuf);

            auto localBuf = readFile(localFn);
            auto plan = info.createUpdatePlan(localBuf);
            plan.print();
            printf("Analysis tool %0.2lf sec\n", double(clock() - starttime) / CLOCKS_PER_SEC);

            auto remoteBuf = readFile(dataFn);
            auto resultData = plan.apply(localBuf, remoteBuf);
            writeFile(localFn, resultData);
        }
    }
    catch(const std::exception &e) {
        printf("Exception!\n");
        printf("%s\n", e.what());
    }
    int deltatime = clock() - starttime;

    printf("Finished in %0.2lf sec\n", double(deltatime) / CLOCKS_PER_SEC);

    return 0;
}
