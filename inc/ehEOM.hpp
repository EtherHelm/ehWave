#ifndef ehEOM_H
#define ehEOM_H

#include "cellSetOption.H"
#include "addToRunTimeSelectionTable.H"
#include "waveModel.hpp"

namespace Foam
{
namespace fv
{

struct relaxationZone
{
    vector origin;
    vector dirNorm;
};

class ehEOM
:
    public cellSetOption
{
    scalar coeffVel_;
    scalar coeffAlpha_;
    List<relaxationZone> zones_;
    bool alphaRelax_;
    word waveName_;
    waveModel* wave_;

    tmp<scalarField> calcSigma(const vectorField& C) const;

public:
    TypeName("ehEOM");

    ehEOM
    (
        const word& name,
        const word& modelType,
        const dictionary& dict,
        const fvMesh& mesh
    );

    virtual ~ehEOM() = default;

    virtual void addSup
    (
        fvMatrix<vector>& eqn,
        const label fieldi
    );

    virtual void addSup
    (
        const volScalarField& rho,
        fvMatrix<vector>& eqn,
        const label fieldi
    );

    virtual bool read(const dictionary& dict);
};

}
}

#endif
