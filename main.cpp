#include <stdio.h>
#include <time.h>
#include "tdmsync.h"
#include "fileio.h"

using namespace TdmSync;

void exit_filenotfound() {
    fprintf(stderr, "File not found!");
    exit(1);
}

void exit_usage() {
    fprintf(stderr, "Wrong arguments!");
    exit(1);
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

            StdioFile dataFile;
            dataFile.open(dataFn.c_str(), StdioFile::Read);
            FileInfo info;
            info.computeFromFile(dataFile, blockSize);

            StdioFile metaFile;
            metaFile.open(metaFn.c_str(), StdioFile::Write);
            info.serialize(metaFile);
        }
        else {
            if (argc < 4) exit_usage();
            std::string localFn = argv[3];

            StdioFile metaFile;
            metaFile.open(metaFn.c_str(), StdioFile::Read);
            FileInfo info;
            info.deserialize(metaFile);

            StdioFile localFile;
            localFile.open(localFn.c_str(), StdioFile::Read);
            auto plan = info.createUpdatePlan(localFile);
            plan.print();
            printf("Analysis tool %0.2lf sec\n", double(clock() - starttime) / CLOCKS_PER_SEC);

            StdioFile remoteFile;
            remoteFile.open(dataFn.c_str(), StdioFile::Read);

            std::string resultFn = localFn + ".updated";
            StdioFile resultFile;
            resultFile.open(resultFn.c_str(), StdioFile::Write);
            plan.apply(localFile, remoteFile, resultFile);
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
