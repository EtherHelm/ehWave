#include "waveModels/GPLWave.hpp"
#include "addToRunTimeSelectionTable.H"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <Pstream.H>

namespace
{

std::string generateUniqueShmName()
{
    static std::mt19937_64 rng(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist;
    return "/shm_" + std::to_string(dist(rng));
}

template<typename T>
size_t getBytes(T* arr, size_t count)
{
    return count * sizeof(T);
}

void ensureConnected(nng_socket& sock, bool& connected, const std::string& ipcUrl)
{
    if (!connected && !ipcUrl.empty())
    {
        nng_req0_open(&sock);
        nng_socket_set_ms(sock, NNG_OPT_SENDTIMEO, 60000);
        nng_socket_set_ms(sock, NNG_OPT_RECVTIMEO, 60000);
        nng_dial(sock, ipcUrl.c_str(), nullptr, 0);
        connected = true;
    }
}

bool sendAndRecv(nng_socket& sock, nng_msg* msg)
{
    int rv = nng_sendmsg(sock, msg, 0);
    if (rv != 0)
    {
        nng_msg_free(msg);
        std::cerr << "nng_sendmsg failed: " << nng_strerror(rv) << std::endl;
        return false;
    }
    rv = nng_recvmsg(sock, &msg, 0);
    if (rv != 0)
    {
        std::cerr << "nng_recvmsg timeout/failed: " << nng_strerror(rv) << std::endl;
        return false;
    }
    nng_msg_free(msg);
    return true;
}

} // anonymous namespace


class ShmWriter
{
public:
    ShmWriter(size_t size, nng_socket& sock, bool& connected, const std::string& ipcUrl)
        : size_(size), sock_(sock), connected_(connected), ipcUrl_(ipcUrl)
    {}

    template <typename T>
    void write(T* arr, size_t count)
    {
        data_.emplace_back(
            reinterpret_cast<void*>(const_cast<std::remove_const_t<T>*>(arr)),
            getBytes(arr, count)
        );
    }

    void pullResult(const std::string& router, void* result, size_t resultSize)
    {
        ensureConnected(sock_, connected_, ipcUrl_);

        std::string shm_name = generateUniqueShmName();
        size_t shm_size = sizeof(size_t);
        for (auto& d : data_)
            shm_size += d.second;

        shm_size = std::max(sizeof(float) * resultSize, shm_size);
        SharedMemory shm(shm_name, shm_size, true);
        shm.writeSize(size_);
        for (auto& d : data_)
            shm.write(d.first, d.second);

        size_t routerLen = router.size();
        size_t nameLen = shm_name.size();
        nng_msg* msg;
        nng_msg_alloc(&msg, routerLen + nameLen + 2 + sizeof(size_t));

        char* body = static_cast<char*>(nng_msg_body(msg));
        memcpy(body, router.c_str(), routerLen + 1);
        memcpy(body + routerLen + 1, shm_name.c_str(), nameLen + 1);
        memcpy(body + routerLen + nameLen + 2, &shm_size, sizeof(size_t));

        if (!sendAndRecv(sock_, msg))
        {
            shm.unlink();
            return;
        }

        shm.resetOffset();
        float* floatResult = static_cast<float*>(shm.data());
        double* doubleResult = static_cast<double*>(result);
        for (size_t i = 0; i < resultSize; i++)
            doubleResult[i] = static_cast<double>(floatResult[i]);

        shm.unlink();
    }

private:
    size_t size_;
    std::vector<std::pair<void*, size_t>> data_;
    std::string ipcUrl_;
    nng_socket& sock_;
    bool& connected_;
};


namespace Foam
{

defineTypeNameAndDebug(GPLWave, 0);
addToRunTimeSelectionTable(waveModel, GPLWave, dictionary);

GPLWave::GPLWave(const dictionary& dict)
:
    filePath_(std::filesystem::absolute(dict.get<fileName>("filePath").c_str()).lexically_normal().c_str()),
    ipcUrl_(dict.get<string>("ipcUrl")),
    serverPath_(dict.getOrDefault<string>("serverPath", "GPLServer")),
    logPath_(dict.getOrDefault<string>("logPath", "")),
    sock_(),
    connected_(false),
    t_(0),
    lastFileT_(-1),
    serverPid_(-1)
{
    if (UPstream::myProcNo() == 0)
    {
        startServer();
        sleep(1);
    }
    char dummy = 0;
    UPstream::broadcast(&dummy, 1, UPstream::worldComm);
}

GPLWave::GPLWave(const GPLWave& rhs)
:
    waveModel(rhs),
    filePath_(rhs.filePath_),
    ipcUrl_(rhs.ipcUrl_),
    serverPath_(rhs.serverPath_),
    logPath_(rhs.logPath_),
    sock_(),
    connected_(false),
    t_(rhs.t_),
    lastFileT_(rhs.lastFileT_),
    serverPid_(-1)
{}

GPLWave::~GPLWave()
{
    stopServer();
    if (connected_)
        nng_close(sock_);
}

autoPtr<waveModel> GPLWave::clone() const
{
    return autoPtr<waveModel>(new GPLWave(*this));
}

void GPLWave::startServer()
{
    serverPid_ = fork();
    if (serverPid_ == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        setsid();

        if (!logPath_.empty())
        {
            int fd = open(logPath_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0)
            {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        else
        {
            int fd = open("/dev/null", O_RDWR);
            if (fd >= 0)
            {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        int maxfd = sysconf(_SC_OPEN_MAX);
        for (int fd = 3; fd < maxfd; fd++)
            close(fd);

        execlp(serverPath_.c_str(), serverPath_.c_str(), ipcUrl_.c_str(), nullptr);
        std::cerr << "GPLWave: failed to start " << serverPath_
                  << ": " << strerror(errno) << std::endl;
        _exit(1);
    }
    else if (serverPid_ < 0)
    {
        std::cerr << "GPLWave: fork failed: " << strerror(errno) << std::endl;
        serverPid_ = -1;
    }
    else
    {
        std::cout << "GPLWave: started " << serverPath_ << " " << ipcUrl_
                  << " (PID " << serverPid_ << ")" << std::endl;
    }
}

void GPLWave::stopServer()
{
    if (serverPid_ <= 0)
        return;

    kill(serverPid_, SIGTERM);
    sleep(1);

    int status;
    if (waitpid(serverPid_, &status, WNOHANG) == 0)
    {
        kill(serverPid_, SIGKILL);
        waitpid(serverPid_, &status, 0);
    }
    serverPid_ = -1;
}

void GPLWave::sendStr(const char* router, const std::string& str) const
{
    ensureConnected(sock_, connected_, ipcUrl_);

    size_t routerLen = strlen(router);
    nng_msg* msg;
    nng_msg_alloc(&msg, routerLen + str.size() + 2);

    char* body = static_cast<char*>(nng_msg_body(msg));
    memcpy(body, router, routerLen + 1);
    memcpy(body + routerLen + 1, str.c_str(), str.size() + 1);

    sendAndRecv(sock_, msg);
}

void GPLWave::restore(scalar t, const word& timeName)
{
    if (UPstream::myProcNo() != 0 || lastFileT_ != -1)
        return;

    namespace fs = std::filesystem;
    fs::path checkPath = fs::current_path();
    if (UPstream::parRun())
    {
        checkPath = checkPath
            / ("processor" + std::to_string(UPstream::myProcNo()))
            / timeName.c_str()
            / "waveState.bin";
    }
    else
    {
        checkPath = checkPath / timeName.c_str() / "waveState.bin";
    }

    if (fs::exists(checkPath))
    {
        filePath_ = fileName(checkPath.string());
        std::cout << "GPLWave: restoring wave state from " << filePath_ << std::endl;
    }

    lastFileT_ = t;
    sendStr("setParams", "[file]" + filePath_);
}

void GPLWave::updateTime(scalar t)
{
    t_ = t;
}

void GPLWave::write(Ostream& os) const
{
    fileName timeDir = fileName(os.name()).path();
    fileName savePath = timeDir / "waveState.bin";
    if (savePath != filePath_)
    {
        filePath_ = savePath;
        sendStr("setParams", "[save]" + filePath_);
    }
}

void GPLWave::compute
(
    const std::string& router,
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    void* result,
    size_t resultSize
) const
{
    int size = points.cols();
    ShmWriter shm(size, sock_, connected_, ipcUrl_);
    shm.write(&t_, 1);
    shm.write(points.data(), size * 3);
    shm.pullResult(router, result, resultSize);
}

void GPLWave::computeVel
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::MatrixXd> result
) const
{
    compute("getVelVOF", points, result.data(), points.cols() * 3);
}

void GPLWave::computeEta
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::VectorXd> result
) const
{
    compute("getEtaVOF", points, result.data(), points.cols());
}

void GPLWave::sendFreeSurface
(
    scalar t,
    const Eigen::Ref<const Eigen::MatrixXd>& points
) const
{
    int nPoints = points.cols();
    if (nPoints == 0) return;

    int procId = UPstream::myProcNo();

    ShmWriter shm(nPoints, sock_, connected_, ipcUrl_);
    shm.write(&t, 1);
    shm.write(points.data(), nPoints * 3);
    shm.pullResult("pushEtaVOF", nullptr, 0);
}

}
