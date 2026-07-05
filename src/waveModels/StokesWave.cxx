#include "waveModels/StokesWave.hpp"
#include "addToRunTimeSelectionTable.H"
#include <cmath>

namespace Foam
{

defineTypeNameAndDebug(StokesWave, 0);
addToRunTimeSelectionTable(waveModel, StokesWave, dictionary);

StokesWave::StokesWave(const dictionary& dict)
:
    H_(dict.get<scalar>("waveHeight")),
    T_(dict.get<scalar>("wavePeriod"))
{
    calcK();
    calcCoeffs();
}

void StokesWave::calcK()
{
    constexpr scalar g = 9.81;
    const scalar om = omega();

    // Initial guess: deep-water linear dispersion  k = om^2 / g
    k_ = om * om / g;

    // Refine with Stokes 5th-order deep-water dispersion:
    //   om = (1 + eps^2/2 + eps^4/8) * sqrt(g*k),   eps = k*H/2
    for (int i = 0; i < 10; ++i)
    {
        const scalar eps = 0.5 * k_ * H_;
        const scalar eps2 = eps * eps;
        const scalar eps3 = eps2 * eps;
        const scalar eps4 = eps2 * eps2;
        const scalar sqgk = sqrt(g * k_);
        const scalar sigma = 1.0 + 0.5*eps2 + 0.125*eps4;
        const scalar f = sigma * sqgk - om;
        const scalar df =
            (eps + 0.5*eps3) * (0.5*H_) * sqgk
          + sigma * 0.5 * g / sqgk;
        if (mag(df) < 1e-15) break;
        k_ -= f / df;
    }
}

void StokesWave::calcCoeffs()
{
    const scalar eps = 0.5 * k_ * H_;
    const scalar eps2 = eps * eps;
    const scalar eps3 = eps2 * eps;
    const scalar eps4 = eps3 * eps;
    const scalar eps5 = eps4 * eps;

    C1_ = eps - 0.5*eps3 - (37.0/24.0)*eps5;
    C2_ = 0.5*eps4;
    C3_ = (1.0/12.0)*eps5;

    a1_ = eps - 3.0/8.0*eps3 - 211.0/192.0*eps5;
    a2_ = 0.5*eps2 + 1.0/3.0*eps4;
    a3_ = 3.0/8.0*eps3 + 99.0/128.0*eps5;
    a4_ = 1.0/3.0*eps4;
    a5_ = 125.0/384.0*eps5;
}

autoPtr<waveModel> StokesWave::clone() const
{
    return autoPtr<waveModel>(new StokesWave(*this));
}

void StokesWave::computeVel
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::MatrixXd> result
) const
{
    const auto n = points.cols();
    result.row(0).setZero();
    result.row(1).setZero();
    result.row(2).setZero();

    constexpr scalar g = 9.81;
    const scalar om = omega();
    const scalar c0 = sqrt(g / k_);
    const scalar C[3] = {C1_, C2_, C3_};

    const auto& z = points.row(2).array();
    const auto psi = k_ * points.row(0).array() - om * t_;

    Eigen::ArrayXd u = Eigen::ArrayXd::Zero(n);
    Eigen::ArrayXd w = Eigen::ArrayXd::Zero(n);

    for (int j = 1; j <= 3; ++j)
    {
        const scalar coeff = j * C[j-1] * c0;
        const auto decay = (j * k_ * z).exp();
        const auto jpsi = j * psi;
        u += coeff * decay * jpsi.cos();
        w += coeff * decay * jpsi.sin();
    }

    result.row(0) = u.matrix();
    result.row(1).setZero();
    result.row(2) = w.matrix();
}

void StokesWave::computeEta
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::VectorXd> result
) const
{
    const scalar om = omega();
    const auto psi = k_ * points.row(0).array() - om * t_;

    result.array() =
        (a1_ * psi.cos()
       + a2_ * (2.0 * psi).cos()
       + a3_ * (3.0 * psi).cos()
       + a4_ * (4.0 * psi).cos()
       + a5_ * (5.0 * psi).cos()) / k_;
}

}
