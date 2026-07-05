#include "ehEOM.hpp"
#include "fvMatrix.H"
#include "volFields.H"

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(ehEOM, 0);
    addToRunTimeSelectionTable
    (
        option,
        ehEOM,
        dictionary
    );
}
}

Foam::fv::ehEOM::ehEOM
(
    const word& name,
    const word& modelType,
    const dictionary& dict,
    const fvMesh& mesh
)
:
    fv::cellSetOption(name, modelType, dict, mesh),
    coeffVel_(1),
    coeffAlpha_(1),
    wave_(nullptr)
{
    read(dict);
}

void Foam::fv::ehEOM::addSup
(
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    addSup(volScalarField::null(), eqn, fieldi);
}

Foam::tmp<Foam::scalarField>
Foam::fv::ehEOM::calcSigma(const vectorField& C) const
{
    auto ts = tmp<scalarField>(new scalarField(cells_.size(), 0));
    auto& sigma = ts.ref();

    forAll(cells_, i)
    {
        const label celli = cells_[i];

        scalar s = 1.0;
        for (const auto& z : zones_)
        {
            scalar proj = (C[celli] - z.origin) & z.dirNorm;
            s = min(s, proj);
        }
        s = max(s, 0.0);
        // sigma[i] = (1.0 - s) * (1.0 - s);
        sigma[i] = 0.5 * (1.0 + cos(s * M_PI));
    }

    return ts;
}

void Foam::fv::ehEOM::addSup
(
    const volScalarField& rho,
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    const vectorField& C = mesh_.C();
    const scalarField& V = mesh_.V();
    const vectorField& U = eqn.psi();
    vectorField& source = eqn.source();
    const bool useRho = notNull(rho);

    const label nc = cells_.size();
    if (nc == 0) return;

    const auto tsigma = calcSigma(C);
    const scalarField& sigma = tsigma();

    // --- count active cells (sigma > 0) ---
    label nActive = 0;
    forAll(cells_, i) if (sigma[i] > 0) nActive++;
    if (nActive == 0) return;

    // --- assemble active cell data + query eta (one pass) ---
    const scalar t = mesh_.time().value();
    wave_->updateTime(t);

    labelList activeMap(nActive);
    Eigen::MatrixXd pts(3, nActive);
    label ai = 0;
    forAll(cells_, i)
    {
        if (sigma[i] == 0) continue;
        const label celli = cells_[i];
        activeMap[ai] = i;
        pts(0, ai) = C[celli].x();
        pts(1, ai) = C[celli].y();
        pts(2, ai) = C[celli].z();
        ai++;
    }

    Eigen::VectorXd etaCell(nActive);
    wave_->getEta(pts, etaCell);

    // --- count wet cells (z < eta) and query Uwave ---
    label nWet = 0;
    for (label j = 0; j < nActive; j++)
    {
        if (pts(2, j) < etaCell(j)) nWet++;
    }
    if (nWet == 0) return;

    Eigen::MatrixXd ptsWet(3, nWet);
    labelList wetMap(nWet);
    label wj = 0;
    for (label j = 0; j < nActive; j++)
    {
        if (pts(2, j) >= etaCell(j)) continue;
        wetMap[wj] = j;
        ptsWet(0, wj) = pts(0, j);
        ptsWet(1, wj) = pts(1, j);
        ptsWet(2, wj) = pts(2, j);
        wj++;
    }

    Eigen::MatrixXd Uwave(3, nWet);
    wave_->getVel(ptsWet, Uwave);

    // --- apply U source for wet cells ---
    for (label wj = 0; wj < nWet; wj++)
    {
        const label j = wetMap[wj];
        const label i = activeMap[j];
        const label celli = cells_[i];

        scalar wi = coeffVel_ * sigma[i] * V[celli];
        if (useRho) wi *= rho[celli];

        const vector Uw(Uwave(0, wj), Uwave(1, wj), Uwave(2, wj));
        source[celli] += wi * (U[celli] - Uw);
    }

    // --- relax alpha toward target (1 underwater, 0 above) ---
    if (alphaRelax_)
    {
        label nOuterCorr = 1;
        if (mesh_.solutionDict().found("PIMPLE"))
        {
            nOuterCorr = mesh_.solutionDict().subDict("PIMPLE")
                .getOrDefault<label>("nOuterCorrectors", 1);
        }
        const scalar alphaDt = coeffAlpha_ * mesh_.time().deltaTValue() / nOuterCorr;

        volScalarField& alpha =
            mesh_.lookupObjectRef<volScalarField>("alpha.water");
        for (label j = 0; j < nActive; j++)
        {
            const label i = activeMap[j];
            const label celli = cells_[i];
            const scalar target = (pts(2, j) < etaCell(j)) ? 1.0 : 0.0;
            const scalar relax = alphaDt * sigma[i];
            alpha[celli] = (1.0 - relax) * alpha[celli] + relax * target;
        }
    }
}

bool Foam::fv::ehEOM::read(const dictionary& dict)
{
    if (!fv::cellSetOption::read(dict))
    {
        return false;
    }

    scalar characteristicFreq = coeffs_.getOrDefault<scalar>("characteristicFreq", 1);
    coeffVel_ = coeffs_.getOrDefault<scalar>("coeffVel", 1) * characteristicFreq;
    coeffAlpha_ = coeffs_.getOrDefault<scalar>("coeffAlpha", 1) * characteristicFreq;
    alphaRelax_ = coeffs_.getOrDefault<bool>("alphaRelax", false);

    List<dictionary> zoneDicts;
    coeffs_.readEntry("zones", zoneDicts);
    zones_.resize(zoneDicts.size());
    forAll(zones_, i)
    {
        const dictionary& zd = zoneDicts[i];
        zones_[i].origin = zd.get<vector>("origin");
        vector dir = zd.get<vector>("direction");
        zones_[i].dirNorm = dir / mag(dir) / zd.get<scalar>("width");
    }

    waveName_ = coeffs_.get<word>("waveName");

    if (!mesh_.foundObject<IOdictionary>("ehWaveProperties"))
    {
        new IOdictionary
        (
            IOobject
            (
                "ehWaveProperties",
                mesh_.time().constant(),
                mesh_,
                IOobject::MUST_READ,
                IOobject::NO_WRITE
            )
        );
    }
    const auto& waveProps = mesh_.lookupObject<IOdictionary>("ehWaveProperties");
    wave_ = &waveModel::getOrCreate(waveName_, waveProps.subDict(waveName_));
    wave_->restore(mesh_.time().value(), mesh_.time().timeName());

    fieldNames_.resize(1);
    fieldNames_.first() = "U";

    fv::option::resetApplied();

    return true;
}
