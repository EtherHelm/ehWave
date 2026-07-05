#ifndef GPLWave_H
#define GPLWave_H

#include "waveModel.hpp"
#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#endif

class SharedMemory
{
public:
    SharedMemory(const std::string& name, size_t size, bool create)
        : shm_name(name), shm_size(size)
    {
#ifdef _WIN32
        hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            static_cast<DWORD>(size >> 32), static_cast<DWORD>(size & 0xFFFFFFFF),
            name.c_str());
        if (!hMapFile)
            throw std::runtime_error("CreateFileMappingA failed: " + std::to_string(GetLastError()));
        shm_ptr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (!shm_ptr)
        {
            CloseHandle(hMapFile);
            throw std::runtime_error("MapViewOfFile failed");
        }
#else
        int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
        shm_fd = shm_open(name.c_str(), flags, 0666);
        if (shm_fd < 0)
            throw std::runtime_error("shm_open failed");
        if (create && ftruncate(shm_fd, size) != 0)
            throw std::runtime_error("ftruncate failed");
        shm_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED)
            throw std::runtime_error("mmap failed");
#endif
    }

    ~SharedMemory()
    {
        close();
    }

    void* data() { return shm_ptr; }
    size_t size() const { return shm_size; }

    void write(void* ptr, size_t count)
    {
        memcpy(static_cast<char*>(shm_ptr) + offset, ptr, count);
        offset += count;
    }

    void writeSize(size_t size)
    {
        memcpy(static_cast<char*>(shm_ptr) + offset, &size, sizeof(size_t));
        offset += sizeof(size_t);
    }

    void resetOffset() { offset = 0; }

    void close()
    {
#ifdef _WIN32
        if (shm_ptr)
        {
            UnmapViewOfFile(shm_ptr);
            shm_ptr = nullptr;
        }
        if (hMapFile)
        {
            CloseHandle(hMapFile);
            hMapFile = nullptr;
        }
#else
        if (shm_ptr)
        {
            munmap(shm_ptr, shm_size);
            shm_ptr = nullptr;
        }
        if (shm_fd >= 0)
        {
            ::close(shm_fd);
            shm_fd = -1;
        }
#endif
    }

    void unlink()
    {
#ifndef _WIN32
        shm_unlink(shm_name.c_str());
#endif
    }

private:
    std::string shm_name;
    size_t shm_size;
    void* shm_ptr = nullptr;
    size_t offset = 0;

#ifdef _WIN32
    void* hMapFile = nullptr;
#else
    int shm_fd = -1;
#endif
};

namespace Foam
{

class GPLWave
:
    public waveModel
{

public:
    TypeName("GPLWave");

    GPLWave(const dictionary& dict);
    GPLWave(const GPLWave&);
    ~GPLWave() override;

    const fileName& filePath() const { return filePath_; }
    const string& ipcUrl() const { return ipcUrl_; }

    autoPtr<waveModel> clone() const override;
    void updateTime(scalar t) override;
    void restore(scalar t, const word& timeName) override;
    void write(Ostream& os) const override;
    void computeVel
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::MatrixXd> result
    ) const override;
    void computeEta
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::VectorXd> result
    ) const override;

private:
    void sendStr(const char* router, const std::string& str) const;
    void compute
    (
        const std::string& router,
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        void* result,
        size_t resultSize
    ) const;

    void startServer();
    void stopServer();

    mutable fileName filePath_;
    string ipcUrl_;
    string serverPath_;
    string logPath_;
    mutable nng_socket sock_;
    mutable bool connected_;
    scalar t_;
    scalar lastFileT_;
    int serverPid_;
};

}

#endif
