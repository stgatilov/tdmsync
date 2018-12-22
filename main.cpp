#include <stdio.h>
#include <time.h>
#include "tdmsync.h"
#include "fileio.h"

#ifdef WITH_CURL
#include <curl/curl.h>
#include "tdmsync_curl.h"
#endif

using namespace TdmSync;

void exit_usage() {
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "  tdmsync prepare [file_path] (block_size=4096)\n");
    fprintf(stderr, "    takes local file at [file_path] and preprocess it\n");
    fprintf(stderr, "    saves metainformation into file [file_path].tdmsync\n");
    fprintf(stderr, "    optional parameter [block_size] specified granularity of updates\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  tdmsync update -file [source_file_path] [dest_file_path]\n");
    fprintf(stderr, "    takes local file at [source_file_path] with metainformation at [source_file_path].tdmsync\n");
    fprintf(stderr, "    synchronizes the local file at [dest_file_path] with it\n");
    fprintf(stderr, "\n");
#ifdef WITH_CURL
    fprintf(stderr, "  tdmsync update -url [source_file_url] [dest_file_path]\n");
    fprintf(stderr, "    takes remote file at [source_file_url] with metainformation at [source_file_url].tdmsync\n");
    fprintf(stderr, "    synchronizes the local file at [dest_file_path] with it, downloading only metainfo and some parts of source\n");
    fprintf(stderr, "\n");
#endif
    exit(1);
}

std::vector<std::string> arguments;

void commandPrepare() {
    if (arguments.size() < 2) {
        fprintf(stderr, "Prepare: missing file path argument\n\n");
        exit_usage();
    }

    std::string dataFn = arguments[1];
    std::string metaFn = dataFn + ".tdmsync";
    fprintf(stderr, "Writing metainfo of file %s into file %s\n", dataFn.c_str(), metaFn.c_str());

    int blockSize;
    if (!(arguments.size() >= 3 && sscanf(arguments[2].c_str(), "%d", &blockSize) == 1))
        blockSize = 4096;
    fprintf(stderr, "Block size: %d\n", blockSize);

    int starttime = clock();
    //===========================================

    StdioFile dataFile;
    dataFile.open(dataFn.c_str(), StdioFile::Read);
    FileInfo info;
    info.computeFromFile(dataFile, blockSize);

    StdioFile metaFile;
    metaFile.open(metaFn.c_str(), StdioFile::Write);
    info.serialize(metaFile);
    metaFile.flush();

    //===========================================
    int deltatime = clock() - starttime;
    printf("Finished in %0.2lf sec\n", double(deltatime) / CLOCKS_PER_SEC);
}

void commandUpdate() {
    if (arguments.size() < 4) {
        fprintf(stderr, "Update: missing type, source or destination argument\n\n");
        exit_usage();
    }

    bool isLocal;
    std::string type = arguments[1];
    if (type == "-file")
        isLocal = true;
    else if (type == "-url")
        isLocal = false;
    else {
        fprintf(stderr, "Wrong type of update \"%s\"\n\n", type.c_str());
        exit_usage();
    }

    std::string dataUri = arguments[2];
    std::string metaUri = dataUri + ".tdmsync";
    std::string metaFn = isLocal ? metaUri : "__temp.tdmsync";
    std::string localFn = arguments[3];
    std::string downFn = localFn + ".download";
    std::string resultFn = localFn + ".updated";

    fprintf(stderr, "Updating local file from %s file:\n", (isLocal ? "local" : "remote"));
    fprintf(stderr, "  %-40s  : local file to be updated\n", localFn.c_str());
    fprintf(stderr, "  %-40s  : updated version of local file\n", resultFn.c_str());
    fprintf(stderr, "  %-40s  : source file to be synchronized with\n", dataUri.c_str());
    fprintf(stderr, "  %-40s  : source file metainformation\n", metaUri.c_str());
    fprintf(stderr, "  %-40s  : local file with metainformation to be read\n", metaUri.c_str());
    fprintf(stderr, "  %-40s  : data downloaded from source file\n", downFn.c_str());

    int starttime = clock();
    //=======================================

    #ifdef WITH_CURL
    if (!isLocal) {
        int metadownload_starttime = clock();
        StdioFile metaFile;
        metaFile.open(metaFn.c_str(), StdioFile::Write);
        CurlDownloader curlWrapper;
        curlWrapper.downloadMeta(metaFile, metaUri.c_str());
        printf("Downloaded %0.0lf KB of metadata in %0.2lf sec\n", metaFile.getSize() / 1024.0, double(clock() - metadownload_starttime) / CLOCKS_PER_SEC);
    }
    #endif
    
    StdioFile metaFile;
    metaFile.open(metaFn.c_str(), StdioFile::Read);
    FileInfo info;
    info.deserialize(metaFile);

    int analysis_starttime = clock();
    StdioFile localFile;
    localFile.open(localFn.c_str(), StdioFile::Read);
    UpdatePlan plan = info.createUpdatePlan(localFile);
    plan.print();
    printf("Analyzed %0.0lf KB of local file in %0.2lf sec\n", localFile.getSize() / 1024.0, double(clock() - analysis_starttime) / CLOCKS_PER_SEC);
    
    if (isLocal) {
        StdioFile remoteFile;
        remoteFile.open(dataUri.c_str(), StdioFile::Read);
        StdioFile downloadFile;
        downloadFile.open(downFn.c_str(), StdioFile::Write);
        plan.createDownloadFile(remoteFile, downloadFile);
    }
    #ifdef WITH_CURL
    else {
        int updatedownload_starttime = clock();
        StdioFile downloadFile;
        downloadFile.open(downFn.c_str(), StdioFile::Write);
        CurlDownloader curlWrapper;
        curlWrapper.downloadMissingParts(downloadFile, plan, dataUri.c_str());
        printf("Downloaded %0.0lf KB of missing blocks in %0.2lf sec\n", downloadFile.getSize() / 1024.0, double(clock() - updatedownload_starttime) / CLOCKS_PER_SEC);
    }
    #endif

    int updatefile_starttime = clock();
    StdioFile downloadFile;
    downloadFile.open(downFn.c_str(), StdioFile::Read);
    StdioFile resultFile;
    resultFile.open(resultFn.c_str(), StdioFile::Write);
    plan.apply(localFile, downloadFile, resultFile);
    resultFile.flush();
    printf("Patched %0.0lf KB file in %0.2lf sec\n", resultFile.getSize() / 1024.0, double(clock() - updatefile_starttime) / CLOCKS_PER_SEC);

    //===========================================
    int deltatime = clock() - starttime;
    printf("Finished in %0.2lf sec\n", double(deltatime) / CLOCKS_PER_SEC);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        arguments.push_back(argv[i]);
    if (arguments.size() < 1) {
        fprintf(stderr, "Command not specified\n\n");
        exit_usage();
    }

    #ifdef WITH_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    #endif

    try {
        if (arguments[0] == "prepare") {
            commandPrepare();
        }
        else if (arguments[0] == "update") {
            commandUpdate();
        }
        else {
            fprintf(stderr, "Unknown command \"%s\"\n\n", arguments[0].c_str());
            exit_usage();
        }
    }
    catch(const std::exception &e) {
        printf("Exception!\n");
        printf("%s\n", e.what());
    }

    return 0;
}
