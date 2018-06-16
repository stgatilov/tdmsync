#ifndef _TDM_SYNC_FILEIO_H_028375_
#define _TDM_SYNC_FILEIO_H_028375_

#include <stdint.h>

namespace TdmSync {

class BaseFile {
public:
    virtual ~BaseFile() {}

    virtual void read(void* data, size_t size) = 0;
    virtual void write(const void* data, size_t size) = 0;
    virtual void seek(uint64_t pos) = 0;
    virtual uint64_t tell() = 0;
    virtual uint64_t getSize() = 0;
};

class StdioFile : public BaseFile {
public:
    StdioFile();
    ~StdioFile();

    enum OpenMode { Read, Write };
    void open(const char *filename, OpenMode mode);

    virtual void read(void* data, size_t size) override;
    virtual void write(const void* data, size_t size) override;
    virtual void seek(uint64_t pos) override;
    virtual uint64_t tell() override;
    virtual uint64_t getSize() override;

private:
    OpenMode mode;
    void *fh;
};

}

#endif
