#include <stdio.h>
#include <time.h>
#include "tdmsync.h"
#include "fileio.h"

#ifdef WITH_CURL
#include <curl/curl.h>
#endif

using namespace TdmSync;

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

    #ifdef WITH_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl");
        exit(2);
    }
    #endif

    std::string dataFn = argv[2];
    std::string metaFn = dataFn + ".tdmsync";
    std::string downFn = dataFn + ".download";

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

            #ifdef WITH_CURL
            FILE *fp = fopen("downloaded.txt", "wt");
            curl_easy_setopt(curl, CURLOPT_URL, "http://stackoverflow.com");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            int res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            fclose(fp);
            exit(0);
            #endif

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
            StdioFile downloadFile;
            downloadFile.open(downFn.c_str(), StdioFile::Write);
            plan.createDownloadFile(remoteFile, downloadFile);
            downloadFile.open(downFn.c_str(), StdioFile::Read);

            std::string resultFn = localFn + ".updated";
            StdioFile resultFile;
            resultFile.open(resultFn.c_str(), StdioFile::Write);
            plan.apply(localFile, downloadFile, resultFile);
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
