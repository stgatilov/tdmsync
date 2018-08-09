#ifndef _TDM_SYNC_CURL_H_328817_
#define _TDM_SYNC_CURL_H_328817_

#include "tdmsync.h"
#include <string>

#include <curl/curl.h>

namespace TdmSync {

class CurlDownloader {
    BaseFile *downloadFile = nullptr;
    const UpdatePlan *plan = nullptr;
    std::string url;

    CURL *curl = nullptr;

    int64_t totalCount = 0, totalSize = 0;
    std::string ranges;

    std::string header, boundary;
    bool isHttp = false, acceptRanges = false;

    int64_t writtenSize = 0;

    static const int BufferSize = 16 << 10;
    std::vector<char> bufferData;
    int bufferAvail = 0;

public: //private!
    size_t headerWriteCallback(char *ptr, size_t size, size_t nmemb);
    size_t plainWriteCallback(char *ptr, size_t size, size_t nmemb);

    int performSingle();
    size_t singleWriteCallback(char *ptr, size_t size, size_t nmemb);

    int performMulti();
    size_t multiWriteCallback(char *ptr, size_t size, size_t nmemb);
    bool processBuffer(bool flush);
    int findBoundary(const char *ptr, int from, int to) const;

public:
    ~CurlDownloader();
    void downloadMeta(BaseFile &wrDownloadFile, const char *url);
    void downloadMissingParts(BaseFile &wrDownloadFile, const UpdatePlan &plan, const char *url);
};

}

#endif
