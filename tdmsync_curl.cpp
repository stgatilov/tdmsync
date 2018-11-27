#include "tdmsync_curl.h"
#include <inttypes.h>
#include <vector>
#include <algorithm>

#include "tsassert.h"
#undef min
#undef max

//This code relies on byte ranges concept in HTTP 1.1 protocol.
//Its specification is available in RFC 7233:
//  https://www.rfc-editor.org/rfc/pdfrfc/rfc7233.txt.pdf


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
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    TdmSyncAssertF(httpCode == 0 || httpCode / 100 == 2, "Downloading metafile failed: http response %d", (int)httpCode);
    TdmSyncAssertF(retCode == CURLE_OK, "Downloading metafile failed: curl error %d", retCode);
}
size_t CurlDownloader::plainWriteCallback(char *ptr, size_t size, size_t nmemb) {
    //note: we can download metainfo file without byte ranges support, but it will be useless then
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
    header += added;    //curl calls callback once per each line of header

    if (startsWith(added, "HTTP"))
        isHttp = true;          //this class is tied to HTTP multi-byte-range behavior
    if (startsWith(added, "Accept-Ranges: bytes"))
        acceptRanges = true;    //differential update is impossible without byte ranges

    //check if this is a multipart response for multi-byte-range request
    if (auto ptr = startsWith(added, "Content-Type: multipart/byteranges; boundary=")) {
        int pos = ptr - added.c_str();
        //save boundary string for parsing the response
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
    //note: single range and multiple range cases are completely different!
    if (totalCount == 0)
        return;                     //nothing to download: empty file is OK
    else if (totalCount == 1)
        retCode = performSingle();  //download with single byte-range
    else //totalCount > 1
        retCode = performMulti();   //download with multi-ranges

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    TdmSyncAssertF(httpCode == 0 || httpCode / 100 == 2, "Downloading missing parts failed: http response %d", (int)httpCode);
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
    //with single byte range request, curl returns only/exactly the requested data
    if (!isHttp || !acceptRanges)
        return 0;                   //fail early if accept-ranges clause not present in header
    size_t bytes = size * nmemb;
    if (writtenSize + bytes > totalSize)
        return 0;                   //protection against webserver sending the whole file to us
    downloadFile->write(ptr, bytes);
    writtenSize += bytes;
    return nmemb;
}

int CurlDownloader::performMulti() {
    bufferData.assign(BufferSize + 16, 0);

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
    //with multiple byte range request, curl returns data segments separated by some http headers
    //this is how curl multipart response looks like (without indents):
    /*================================================================

    --5b69c45c39b6
    Content-type: text/plain
    Content-range: bytes 100-200/5896303
    
    s8d7f8767fds765fg8765sdf87g65g87s65d8f765sd87f6g58s7d6f587s6d5f87s65df76s8df7g68s7df6g876sdf8g76g7677
    --5b69c45c39b6
    Content-type: text/plain
    Content-range: bytes 300-400/5896303
    
    87hdf98j5fg785jfg675j8f76ghj8675gh6j75fg87g5j8d7f6g587s65g87d6g5f8h67d5gf75d8fg675h8d6f7g5h6d75gf7865
    --5b69c45c39b6--
    ================================================================*/

    if (!isHttp || !acceptRanges || boundary.empty())
        return 0;                   //fail early if no boundary was specified in response header
    size_t bytes = size * nmemb;
    while (bytes > 0) {
        //push incoming data from curl into our internal buffer
        int added = std::min(BufferSize - bufferAvail, (int)bytes);
        memcpy(bufferData.data() + bufferAvail, ptr, added);
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
    //when this method is called, the following invariant holds:
    //  1. the start of the buffer is inside some segment's data (i.e. NOT inside the http header/boundary)
    //  2. one of the following is true:
    //     a. the buffer is full
    //     b. the buffer contains the very last bytes of the response, and flush = true

    bufferData[bufferAvail] = 0;    //null-terminate for string routines

    //size of transition zone at the end of the buffer
    //we postpone all boundaries inside transition zone until the next call (except when flush = true)
    //note: it is critically important that this constant is larger than any potential internal http header!
    static const int TailSize = 1024;

    int pos = 0;
    while (1) {
        int end = flush ? bufferAvail - boundary.size() : bufferAvail - TailSize;
        if (end < pos)
            break;      //last http header ended inside transition zone already
        int delim = findBoundary(bufferData.data(), pos, end);
        int until = delim == -1 ? end : delim;
        //write the data from current position to the next found boundary or to start of transition zone
        singleWriteCallback(&bufferData[pos], 1, until - pos);
        pos = until;
        if (delim == -1)
            break;      //no more boundaries in the buffer

        //handle http boundary
        pos += boundary.size();
        if (strncmp(&bufferData[pos], "--", 2) == 0) {
            //this is the final boundary in the response
            if (!flush)
                return false;   //premature "end of response"
            break;    //end of response
        }
        //handle http header (skip it)
        char *ptr = strstr(&bufferData[pos], "\r\n\r\n");
        if (ptr == 0)
            return false;   //internal header must fit into TailSize bytes
        pos = ptr + 4 - bufferData.data();
    }

    //copy the remaining bytes to the beginning of buffer
    //usually, this is the transition zone or a part of it
    memmove(&bufferData[0], &bufferData[pos], bufferAvail - pos);
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
