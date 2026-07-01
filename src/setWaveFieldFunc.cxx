#include "Time.H"
#include "fvMesh.H"
#include "volFields.H"
#include "surfaceFields.H"
#include "IOdictionary.H"

#include "waveModel.hpp"
#include <Eigen/Core>

using namespace Foam;

extern "C" void setWaveFieldFields(Foam::fvMesh* meshPtr, Foam::Time* runTimePtr)
{
    fvMesh& mesh = *meshPtr;
    Time& runTime = *runTimePtr;

    // --- Read configuration ---
    IOdictionary setWaveFieldDict
    (
        IOobject
        (
            "setWaveFieldDict",
            runTime.system(),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        )
    );
    const word waveName = setWaveFieldDict.get<word>("waveName");

    // --- Read wave properties and create wave model ---
    IOdictionary ehWaveProps
    (
        IOobject
        (
            "ehWaveProperties",
            runTime.constant(),
            mesh,
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        )
    );
    autoPtr<waveModel> wave = waveModel::New(ehWaveProps.subDict(waveName));

    // --- Set wave time to startTime ---
    const scalar t = runTime.startTime().value();
    wave->updateTime(t);

    const label nCells = mesh.nCells();
    const label nPts = mesh.nPoints();
    const pointField& pts = mesh.points();

    // --- Wave eta at mesh vertices ---
    Eigen::MatrixXd verts(3, nPts);
    for (label i = 0; i < nPts; ++i)
    {
        verts(0, i) = pts[i].x();
        verts(1, i) = pts[i].y();
        verts(2, i) = pts[i].z();
    }
    Eigen::VectorXd etaVert(nPts);
    wave->getEta(verts, etaVert);

    // ================================================================
    // alpha.water
    // ================================================================
    volScalarField alpha
    (
        IOobject
        (
            "alpha.water",
            runTime.timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    );

    // Internal field: vertex-based volume fraction
    {
        const labelListList& cellPoints = mesh.cellPoints();
        auto& alphaIn = alpha.primitiveFieldRef();
        for (label celli = 0; celli < nCells; ++celli)
        {
            const labelList& cPts = cellPoints[celli];
            scalar sumBelow = 0;
            scalar sumAbs = 0;
            for (label j = 0; j < cPts.size(); ++j)
            {
                const scalar dz = pts[cPts[j]].z() - etaVert(cPts[j]);
                sumBelow += Foam::max(-dz, scalar(0));
                sumAbs += Foam::mag(dz);
            }
            alphaIn[celli] = (sumAbs == 0) ? scalar(0) : (sumBelow / sumAbs);
        }
    }

    // Boundary field: vertex-based alpha on patch faces (reuses etaVert via meshPoints mapping)
    forAll(alpha.boundaryField(), patchi)
    {
        auto& ap = alpha.boundaryFieldRef()[patchi];
        const fvPatch& patch = ap.patch();
        if (patch.size() == 0) continue;

        const labelList& meshPts = patch.patch().meshPoints();
        const faceList& localFaces = patch.patch().localFaces();
        forAll(ap, facei)
        {
            const face& f = localFaces[facei];
            scalar sumBelow = 0;
            scalar sumAbs = 0;
            for (label j = 0; j < f.size(); ++j)
            {
                const label ptI = meshPts[f[j]];
                const scalar dz = pts[ptI].z() - etaVert(ptI);
                sumBelow += Foam::max(-dz, scalar(0));
                sumAbs += Foam::mag(dz);
            }
            ap[facei] = (sumAbs == 0) ? scalar(0) : (sumBelow / sumAbs);
        }
    }

    alpha.write();
    Info << "setWaveField: wrote alpha.water" << endl;

    // ================================================================
    // U
    // ================================================================
    {
        volVectorField U
        (
            IOobject
            (
                "U",
                runTime.timeName(),
                mesh,
                IOobject::MUST_READ,
                IOobject::AUTO_WRITE
            ),
            mesh
        );

        const auto& alphaIn = alpha.primitiveField();
        auto& UIn = U.primitiveFieldRef();

        label nActive = 0;
        for (label celli = 0; celli < nCells; ++celli)
        {
            if (alphaIn[celli] > 0) ++nActive;
            else UIn[celli] = vector::zero;
        }

        label nQuery = nActive;
        forAll(U.boundaryField(), patchi)
            nQuery += U.boundaryField()[patchi].size();

        if (nQuery > 0)
        {
            const auto& cc = mesh.C();
            Eigen::MatrixXd allCentres(3, nQuery);

            label col = 0;
            for (label celli = 0; celli < nCells; ++celli)
            {
                if (alphaIn[celli] == 0) continue;
                allCentres(0, col) = cc[celli].x();
                allCentres(1, col) = cc[celli].y();
                allCentres(2, col) = cc[celli].z();
                ++col;
            }

            forAll(U.boundaryField(), patchi)
            {
                const auto& bf = U.boundaryField()[patchi];
                const label nFaces = bf.size();
                if (nFaces == 0) continue;
                const auto& fc = bf.patch().Cf();
                for (label i = 0; i < nFaces; ++i)
                {
                    allCentres(0, col + i) = fc[i].x();
                    allCentres(1, col + i) = fc[i].y();
                    allCentres(2, col + i) = fc[i].z();
                }
                col += nFaces;
            }

            Eigen::MatrixXd allVel(3, nQuery);
            wave->getVel(allCentres, allVel);

            col = 0;
            for (label celli = 0; celli < nCells; ++celli)
            {
                if (alphaIn[celli] == 0) continue;
                UIn[celli] = vector(allVel(0, col), allVel(1, col), allVel(2, col));
                ++col;
            }

            forAll(U.boundaryField(), patchi)
            {
                auto& Up = U.boundaryFieldRef()[patchi];
                const label nFaces = Up.size();
                if (nFaces == 0) continue;
                for (label i = 0; i < nFaces; ++i)
                    Up[i] = vector(allVel(0, col + i), allVel(1, col + i), allVel(2, col + i));
                col += nFaces;
            }
        }

        U.write();
        Info << "setWaveField: wrote U" << endl;
    }
}
