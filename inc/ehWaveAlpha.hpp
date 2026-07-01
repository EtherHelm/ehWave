#ifndef ehWaveAlpha_H
#define ehWaveAlpha_H

#include "inletOutletFvPatchFields.H"
#include "addToRunTimeSelectionTable.H"
#include "waveModel.hpp"

namespace Foam
{

class ehWaveAlpha
:
    public inletOutletFvPatchScalarField
{
    waveModel* wave_;
    word waveName_;

public:
    TypeName("ehWaveAlpha");

    ehWaveAlpha
    (
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF
    );

    ehWaveAlpha
    (
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF,
        const dictionary& dict
    );

    ehWaveAlpha
    (
        const ehWaveAlpha& ptf,
        const fvPatch& p,
        const DimensionedField<scalar, volMesh>& iF,
        const fvPatchFieldMapper& mapper
    );

    ehWaveAlpha
    (
        const ehWaveAlpha& ptf
    );

    ehWaveAlpha
    (
        const ehWaveAlpha& ptf,
        const DimensionedField<scalar, volMesh>& iF
    );

    virtual tmp<fvPatchField<scalar>> clone() const
    {
        return fvPatchField<scalar>::Clone(*this);
    }

    virtual tmp<fvPatchField<scalar>> clone
    (
        const DimensionedField<scalar, volMesh>& iF
    ) const
    {
        return fvPatchField<scalar>::Clone(*this, iF);
    }

    virtual void updateCoeffs();
    virtual void write(Ostream& os) const;
};

}

#endif
