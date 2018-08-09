#include "tdmsync_curl.h"
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include "tsassert.h"
#undef min
#undef max


namespace TdmSync {

CurlDownloader::~CurlDownloader() {
    if (curl)
        curl_easy_cleanup(curl);
}


void CurlDownloader::downloadMeta(BaseFile &wrDownloadFile, const char *url_) {
    TdmSyncAssertF(!curl, "Each CurlDownloader object must be used only once");
    curl = curl_easy_init();
    TdmSyncAssertF(curl, "Failed to initialize curl");

    downloadFile = &wrDownloadFile;
    url = url_;

    auto header_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->headerWriteCallback(ptr, size, nmemb);
    };
    auto plain_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->plainWriteCallback(ptr, size, nmemb);
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)header_write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)plain_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)this);

    int retCode = curl_easy_perform(curl);
    TdmSyncAssertF(retCode == CURLE_OK, "Downloading metafile failed: curl error %d", retCode);
}
size_t CurlDownloader::plainWriteCallback(char *ptr, size_t size, size_t nmemb) {
    if (!isHttp || !acceptRanges)
        return 0;
    downloadFile->write(ptr, size * nmemb);
    return nmemb;
}

static const char *startsWith(const std::string &line, const char *prefix){
    int len = strlen(prefix);
    if (strncmp(line.c_str(), prefix, len) == 0)
        return line.c_str() + len;
    return nullptr;
}
size_t CurlDownloader::headerWriteCallback(char *ptr, size_t size, size_t nmemb) {
    size_t bytes = size * nmemb;
    std::string added(ptr, ptr + bytes);
    header += added;

    if (startsWith(added, "HTTP"))
        isHttp = true;
    if (startsWith(added, "Accept-Ranges: bytes"))
        acceptRanges = true;
    if (auto ptr = startsWith(added, "Content-Type: multipart/byteranges; boundary=")) {
        int pos = ptr - added.c_str();
        std::string token = added.substr(pos, added.size() - 2 - pos);
        boundary = "\r\n--" + token;// + "\r\n";
    }

    return nmemb;
}


void CurlDownloader::downloadMissingParts(BaseFile &wrDownloadFile, const UpdatePlan &plan_, const char *url_) {
    TdmSyncAssertF(!curl, "Each CurlDownloader object must be used only once");
    curl = curl_easy_init();
    TdmSyncAssertF(curl, "Failed to initialize curl");

    downloadFile = &wrDownloadFile;
    plan = &plan_;
    url = url_;

    //create http byte-ranges string
    ranges.clear();
    for (int i = 0; i < plan->segments.size(); i++) {
        const auto &seg = plan->segments[i];
        if (seg.remote) {
            char buff[256];
            sprintf(buff, "%" PRId64 "-%" PRId64, seg.dstOffset, seg.dstOffset + seg.size - 1);
            if (!ranges.empty())
                ranges += ',';
            ranges += buff;
            totalCount++;
            totalSize += seg.size;
        }
    }
    TdmSyncAssert(totalSize == plan->bytesRemote);

    int retCode = -1;
    if (totalCount == 0)
        return;                     //nothing to download: empty file is OK
    else if (totalCount == 1)
        retCode = performSingle();  //download with single byte-range
    else //totalCount > 1
        retCode = performMulti();   //download with multi-ranges

    TdmSyncAssertF(retCode == CURLE_OK, "Downloading missing parts failed: curl error %d", retCode);
    TdmSyncAssertF(writtenSize == totalSize, "Size of output file is wrong: " PRId64 " instead of " PRId64, writtenSize, totalSize);
}

int CurlDownloader::performSingle() {
    auto header_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->headerWriteCallback(ptr, size, nmemb);
    };
    auto single_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->singleWriteCallback(ptr, size, nmemb);
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)header_write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)single_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)this);
    curl_easy_setopt(curl, CURLOPT_RANGE, ranges.c_str());
    return curl_easy_perform(curl);
}
size_t CurlDownloader::singleWriteCallback(char *ptr, size_t size, size_t nmemb) {
    if (!isHttp || !acceptRanges)
        return 0;
    size_t bytes = size * nmemb;
    if (writtenSize + bytes > totalSize)
        return 0;
    downloadFile->write(ptr, bytes);
    writtenSize += bytes;
    return nmemb;
}

int CurlDownloader::performMulti() {
    memset(bufferData, 0, sizeof(bufferData));

    auto header_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->headerWriteCallback(ptr, size, nmemb);
    };
    auto multi_write_callback = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t {
        return ((CurlDownloader*)userdata)->multiWriteCallback(ptr, size, nmemb);
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (curl_write_callback)header_write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)this);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (curl_write_callback)multi_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)this);
    curl_easy_setopt(curl, CURLOPT_RANGE, ranges.c_str());

    int retCode = curl_easy_perform(curl);
    processBuffer(true);    //flush our own buffer
    return retCode;
}
size_t CurlDownloader::multiWriteCallback(char *ptr, size_t size, size_t nmemb) {
    if (!isHttp || !acceptRanges || boundary.empty())
        return 0;
    size_t bytes = size * nmemb;
    while (bytes > 0) {
        int added = std::min(BufferSize - bufferAvail, (int)bytes);
        memcpy(bufferData + bufferAvail, ptr, added);
        ptr += added;
        bytes -= added;
        bufferAvail += added;
        if (bufferAvail == BufferSize)
            if (!processBuffer(false))
                return 0;
    }
    return nmemb;
}
bool CurlDownloader::processBuffer(bool flush) {
    bufferData[bufferAvail] = 0;

    int pos = 0;
    while (1) {
        static const int TailSize = 1024;
        int end = flush ? bufferAvail - boundary.size() : bufferAvail - TailSize;
        int delim = findBoundary(bufferData, pos, end);
        int until = delim == -1 ? end : delim;

        singleWriteCallback(bufferData + pos, 1, until - pos);
        pos = until;
        if (delim == -1)
            break;
        
        pos += boundary.size();
        if (strncmp(bufferData + pos, "--", 2) == 0) {
            if (!flush)
                return false;   //premature "end of message"
            break;    //end of message
        }

        char *ptr = strstr(bufferData + pos, "\r\n\r\n");
        if (ptr == 0)
            return false;   //internal header must fit into TailSize bytes
        pos = ptr + 4 - bufferData;
    }

    //copy remaining bytes to start of buffer
    memmove(bufferData, bufferData + pos, bufferAvail - pos);
    bufferAvail -= pos;

    return true;
}
int CurlDownloader::findBoundary(const char *ptr, int from, int to) const {
    for (int i = from; i < to; i++)
        if (ptr[i + 4] == boundary[4])  //first char of boundary after two hyphens
            if (memcmp(ptr + i, boundary.data(), boundary.size()) == 0)
                return i;
    return -1;
}

}
