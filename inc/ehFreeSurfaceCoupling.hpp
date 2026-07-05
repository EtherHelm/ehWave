#ifndef ehTwoWayCoupling_H
#define ehTwoWayCoupling_H

#include "fvMeshFunctionObject.H"
#include "dictionary.H"

namespace Foam
{

class ehTwoWayCoupling : public functionObjects::fvMeshFunctionObject
{
    const word waveName_;

public:
    TypeName("ehTwoWayCoupling");

    ehTwoWayCoupling(const word& name, const Time& runTime, const dictionary& dict);

    virtual bool execute();
    virtual bool write();
    virtual bool read(const dictionary& dict);
};

}

#endif
