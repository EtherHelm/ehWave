#ifndef ehWave_H
#define ehWave_H

#include "mixedFvPatchFields.H"
#include "addToRunTimeSelectionTable.H"
#include "waveModel.hpp"

namespace Foam
{

class ehWaveVel
:
    public mixedFvPatchVectorField
{
    waveModel* wave_;
    word waveName_;

public:
    TypeName("ehWaveVel");

    ehWaveVel
    (
        const fvPatch& p,
        const DimensionedField<vector, volMesh>& iF
    );

    ehWaveVel
    (
        const fvPatch& p,
        const DimensionedField<vector, volMesh>& iF,
        const dictionary& dict
    );

    ehWaveVel
    (
        const ehWaveVel& ptf,
        const fvPatch& p,
        const DimensionedField<vector, volMesh>& iF,
        const fvPatchFieldMapper& mapper
    );

    ehWaveVel
    (
        const ehWaveVel& ptf
    );

    ehWaveVel
    (
        const ehWaveVel& ptf,
        const DimensionedField<vector, volMesh>& iF
    );

    virtual tmp<fvPatchField<vector>> clone() const
    {
        return fvPatchField<vector>::Clone(*this);
    }

    virtual tmp<fvPatchField<vector>> clone
    (
        const DimensionedField<vector, volMesh>& iF
    ) const
    {
        return fvPatchField<vector>::Clone(*this, iF);
    }

    virtual void updateCoeffs();
    virtual void write(Ostream& os) const;
};

}

#endif
