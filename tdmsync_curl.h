#ifndef _TDM_SYNC_CURL_H_328817_
#define _TDM_SYNC_CURL_H_328817_

#include "tdmsync.h"
#include <string>

#include <curl/curl.h>


namespace TdmSync {

struct HttpError : public BaseError {
    int code;
    HttpError(const char *message, int code) : BaseError(message + std::to_string(code)), code(code) {}
};

//implements tdmsync differential update over HTTP 1.1 protocol (using curl)
class CurlDownloader {
public:
    ~CurlDownloader();  //(frees curl context automatically)

    //download the metainfo file from specified url into specified file
    //you can then deserialize it and create an update plan for local file using it
    void downloadMeta(BaseFile &wrDownloadFile, const char *url);

    //download into specified file all the remote segments of the specified update plan from the specified url
    //this invokes multi-byte-range HTTP requests which needs proper web server support
    void downloadMissingParts(BaseFile &wrDownloadFile, const UpdatePlan &plan, const char *url);

private:
    size_t headerWriteCallback(char *ptr, size_t size, size_t nmemb);
    size_t plainWriteCallback(char *ptr, size_t size, size_t nmemb);

    int performSingle();
    size_t singleWriteCallback(char *ptr, size_t size, size_t nmemb);

    int performMulti();
    size_t multiWriteCallback(char *ptr, size_t size, size_t nmemb);
    bool processBuffer(bool flush);
    int findBoundary(const char *ptr, int from, int to) const;

private:
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
};

}

#endif
