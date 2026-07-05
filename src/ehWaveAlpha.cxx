#include "ehWaveAlpha.hpp"
#include "surfaceFields.H"

namespace Foam
{

ehWaveAlpha::ehWaveAlpha
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF
)
:
    inletOutletFvPatchScalarField(p, iF),
    wave_(nullptr)
{}

ehWaveAlpha::ehWaveAlpha
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const dictionary& dict
)
:
    inletOutletFvPatchScalarField(p, iF),
    waveName_(dict.get<word>("waveName"))
{
    if (!db().foundObject<IOdictionary>("ehWaveProperties"))
    {
        new IOdictionary
        (
            IOobject
            (
                "ehWaveProperties",
                db().time().constant(),
                db(),
                IOobject::MUST_READ,
                IOobject::NO_WRITE
            )
        );
    }
    const auto& waveProps = db().lookupObject<IOdictionary>("ehWaveProperties");
    wave_ = &waveModel::getOrCreate(waveName_, waveProps.subDict(waveName_));
    wave_->restore(db().time().value(), db().time().timeName());

    refValue() = Zero;
    refGrad() = Zero;
    valueFraction() = 0.0;

    if (!this->readValueEntry(dict))
    {
        fvPatchField<scalar>::extrapolateInternal();
    }
}

ehWaveAlpha::ehWaveAlpha
(
    const ehWaveAlpha& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    inletOutletFvPatchScalarField(ptf, p, iF, mapper),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

ehWaveAlpha::ehWaveAlpha
(
    const ehWaveAlpha& ptf
)
:
    inletOutletFvPatchScalarField(ptf),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

ehWaveAlpha::ehWaveAlpha
(
    const ehWaveAlpha& ptf,
    const DimensionedField<scalar, volMesh>& iF
)
:
    inletOutletFvPatchScalarField(ptf, iF),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

void ehWaveAlpha::updateCoeffs()
{
    if (updated()) return;

    const scalar t = db().time().value();
    const label n = patch().size();

    if (n == 0)
    {
        inletOutletFvPatchScalarField::updateCoeffs();
        return;
    }

    const polyPatch& pp = patch().patch();
    const pointField& pts = pp.localPoints();
    const label nPts = pts.size();

    Eigen::MatrixXd verts(3, nPts);
    for (label i = 0; i < nPts; ++i)
    {
        verts(0, i) = pts[i].x();
        verts(1, i) = pts[i].y();
        verts(2, i) = pts[i].z();
    }

    wave_->updateTime(t);
    Eigen::VectorXd etaVert(nPts);
    wave_->getEta(verts, etaVert);

    const faceList& faces = pp.localFaces();
    for (label i = 0; i < n; ++i)
    {
        const face& f = faces[i];
        scalar sumBelow = 0;
        scalar sumAbs = 0;
        for (label j = 0; j < f.size(); ++j)
        {
            const label ptI = f[j];
            const scalar dz = pts[ptI].z() - etaVert(ptI);
            sumBelow += max(-dz, scalar(0));
            sumAbs += mag(dz);
        }
        refValue()[i] = (sumAbs == 0) ? scalar(0) : (sumBelow / sumAbs);
    }

    const scalarField& phip =
        patch().lookupPatchField<surfaceScalarField, scalar>("phi");
    valueFraction() = neg(phip);

    inletOutletFvPatchScalarField::updateCoeffs();
}

void ehWaveAlpha::write(Ostream& os) const
{
    fvPatchField<scalar>::write(os);
    os.writeEntry("waveName", waveName_);
    writeEntry("value", os);
    if (wave_) wave_->write(os);
}

}

namespace Foam
{
    makePatchTypeField(fvPatchScalarField, ehWaveAlpha);
}
