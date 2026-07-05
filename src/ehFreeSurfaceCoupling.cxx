#include "ehFreeSurfaceCoupling.hpp"
#include "addToRunTimeSelectionTable.H"
#include "volFields.H"
#include "volPointInterpolation.H"
#include "isoSurfaceCell.H"
#include "waveModel.hpp"
#include "waveModels/GPLWave.hpp"
#include "Pstream.H"
#include <Eigen/Core>

namespace Foam
{

defineTypeNameAndDebug(ehTwoWayCoupling, 0);
addToRunTimeSelectionTable(functionObject, ehTwoWayCoupling, dictionary);

ehTwoWayCoupling::ehTwoWayCoupling
(
    const word& name,
    const Time& runTime,
    const dictionary& dict
)
:
    fvMeshFunctionObject(name, runTime, dict),
    waveName_(dict.get<word>("waveName"))
{}

bool ehTwoWayCoupling::execute()
{
    const scalar t = mesh_.time().value();

    const volScalarField& alpha = mesh_.lookupObject<volScalarField>("alpha.water");

    const volPointInterpolation& pointInterp = volPointInterpolation::New(mesh_);
    tmp<pointScalarField> tAlphaPoint = pointInterp.interpolate(alpha);

    isoSurfaceCell isoSurf
    (
        mesh_,
        alpha.primitiveField(),
        tAlphaPoint().primitiveField(),
        0.5
    );

    const pointField& surfPoints = isoSurf.points();
    label nPts = surfPoints.size();

    if (nPts == 0) return true;

    Eigen::MatrixXd pts(3, nPts);
    for (label i = 0; i < nPts; ++i)
    {
        pts(0, i) = surfPoints[i].x();
        pts(1, i) = surfPoints[i].y();
        pts(2, i) = surfPoints[i].z();
    }

    const auto& waveProps = mesh_.lookupObject<IOdictionary>("ehWaveProperties");
    waveModel& wm = waveModel::getOrCreate(waveName_, waveProps.subDict(waveName_));
    dynamic_cast<const GPLWave&>(wm).sendFreeSurface(t, pts);

    return true;
}

bool ehTwoWayCoupling::write()
{
    return true;
}

bool ehTwoWayCoupling::read(const dictionary& dict)
{
    return true;
}

}
