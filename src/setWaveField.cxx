#include "argList.H"
#include "Time.H"
#include "fvMesh.H"

#include <dlfcn.h>
#include <string>
#include <cstring>
#include <unistd.h>

using namespace Foam;

// Find and load libehWave.so:
//   1) Already loaded (e.g. by OpenFOAM's controlDict 'libs')
//   2) Same directory as the setWaveField executable
//   3) Entries listed in system/controlDict 'libs'
//   4) FatalError if not found
static void* loadLibehWave(const Time& runTime)
{
    // --- 0) Already loaded (e.g. by OpenFOAM from controlDict libs) ---
    {
        void* handle = dlopen("libehWave.so", RTLD_LAZY | RTLD_NOLOAD);
        if (handle)
        {
            Info << "setWaveField: using already-loaded libehWave.so" << endl;
            return handle;
        }
    }

    // --- 1) Same directory as executable ---
    {
        char buf[4096];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len != -1)
        {
            buf[len] = '\0';
            std::string exePath(buf);
            auto pos = exePath.rfind('/');
            if (pos != std::string::npos)
            {
                std::string libPath = exePath.substr(0, pos + 1) + "libehWave.so";
                void* handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
                if (handle)
                {
                    Info << "setWaveField: loaded " << libPath.c_str() << endl;
                    return handle;
                }
            }
        }
    }

    // --- 2) From controlDict 'libs' entries ---
    {
        const dictionary& controlDict = runTime.controlDict();
        List<fileName> libs;
        if (controlDict.readIfPresent("libs", libs))
        {
            for (const auto& lib : libs)
            {
                std::string libStr(lib);
                if (libStr.find("libehWave.so") != std::string::npos)
                {
                    void* handle = dlopen(libStr.c_str(), RTLD_LAZY | RTLD_LOCAL);
                    if (handle)
                    {
                        Info << "setWaveField: loaded " << libStr.c_str() << endl;
                        return handle;
                    }
                }
            }
        }
    }

    // --- 3) Not found ---
    FatalErrorInFunction
        << "Cannot locate libehWave.so" << nl
        << "Searched: already-loaded, directory of the executable" << nl
        << "  and entries in system/controlDict 'libs'"
        << exit(FatalError);

    return nullptr;
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char* argv[])
{
    #include "setRootCaseLists.H"
    #include "createTime.H"

    Info << "Create mesh" << nl << endl;
    fvMesh mesh
    (
        IOobject
        (
            fvMesh::defaultRegion,
            runTime.timeName(),
            runTime,
            Foam::IOobject::MUST_READ
        )
    );

    // Load libehWave.so and resolve the setWaveFieldFields symbol
    void* libHandle = loadLibehWave(runTime);

    typedef void (*SetWaveFieldsFunc_t)(Foam::fvMesh*, Foam::Time*);
    auto* setWaveFields = reinterpret_cast<SetWaveFieldsFunc_t>(
        dlsym(libHandle, "setWaveFieldFields"));

    if (!setWaveFields)
    {
        FatalErrorInFunction
            << "dlsym(setWaveFieldFields) failed: " << dlerror()
            << exit(FatalError);
    }

    // Delegate to libehWave.so
    setWaveFields(&mesh, &runTime);

    dlclose(libHandle);

    Info << "setWaveField: done" << endl;
    return 0;
}
