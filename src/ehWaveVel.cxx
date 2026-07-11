#include "ehWaveVel.hpp"
#include "surfaceFields.H"

namespace Foam
{

ehWaveVel::ehWaveVel
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF
)
:
    mixedFvPatchVectorField(p, iF),
    wave_(nullptr)
{}

ehWaveVel::ehWaveVel
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchVectorField(p, iF),
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
    valueFraction() = Zero;

    if (dict.found("value"))
    {
        Field<vector>::operator=
        (
            Field<vector>("value", dict, this->size())
        );
    }
    else
    {
        this->patch().patchInternalField(this->internalField(), *this);
    }
}

ehWaveVel::ehWaveVel
(
    const ehWaveVel& ptf,
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    mixedFvPatchVectorField(ptf, p, iF, mapper),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

ehWaveVel::ehWaveVel
(
    const ehWaveVel& ptf
)
:
    mixedFvPatchVectorField(ptf),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

ehWaveVel::ehWaveVel
(
    const ehWaveVel& ptf,
    const DimensionedField<vector, volMesh>& iF
)
:
    mixedFvPatchVectorField(ptf, iF),
    waveName_(ptf.waveName_),
    wave_(ptf.wave_)
{}

void ehWaveVel::updateCoeffs()
{
    if (updated()) return;

    const scalar t = db().time().value();
    const label n = patch().size();

    if (n == 0)
    {
        mixedFvPatchVectorField::updateCoeffs();
        return;
    }

    // Vertex-based eta and z-eta
    const polyPatch& pp = patch().patch();
    const pointField& verts = pp.localPoints();

    wave_->updateTime(t);

    Eigen::Map<const Eigen::MatrixXd> Verts(
        reinterpret_cast<const scalar*>(verts.cdata()), 3, verts.size());
    Eigen::VectorXd zEta(verts.size());
    wave_->getEta(Verts, zEta);
    forAll(verts, i)
        zEta(i) = verts[i].z() - zEta(i);

    // Classify wet faces and query velocity
    const faceList& faces = pp.localFaces();
    const vectorField& fc = patch().Cf();

    DynamicList<label> wetFaces(n);
    for (label i = 0; i < n; ++i)
    {
        const face& f = faces[i];
        scalar sumBelow = 0;
        for (label j = 0; j < f.size(); ++j)
            sumBelow += max(-zEta[f[j]], scalar(0));
        const bool wet = sumBelow > 0;
        valueFraction()[i] = wet ? scalar(1) : scalar(0);
        if (wet) wetFaces.append(i);
    }

    const label nWet = wetFaces.size();
    if (nWet > 0)
    {
        vectorField wetPts(nWet);
        for (label i = 0; i < nWet; ++i)
            wetPts[i] = fc[wetFaces[i]];

        Eigen::Map<const Eigen::MatrixXd> Pts(
            reinterpret_cast<const scalar*>(wetPts.cdata()), 3, nWet);
        Eigen::MatrixXd Vel(3, nWet);
        wave_->getVel(Pts, Vel);

        for (label i = 0; i < nWet; ++i)
            refValue()[wetFaces[i]] = vector(Vel(0, i), Vel(1, i), Vel(2, i));
    }

    refGrad() = Zero;
    mixedFvPatchVectorField::updateCoeffs();
}

void ehWaveVel::write(Ostream& os) const
{
    fvPatchField<vector>::write(os);
    os.writeEntry("waveName", waveName_);
    writeEntry("value", os);
    if (wave_) wave_->write(os);
}

}

namespace Foam
{
    makePatchTypeField(fvPatchVectorField, ehWaveVel);
}
