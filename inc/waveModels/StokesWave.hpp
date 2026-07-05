#ifndef StokesWave_H
#define StokesWave_H

#include "waveModel.hpp"

namespace Foam
{

class StokesWave
:
    public waveModel
{
    scalar H_, T_, k_;

    // Deep-water velocity-potential coefficients (Fenton 1985)
    scalar C1_, C2_, C3_;
    scalar a1_, a2_, a3_, a4_, a5_;

    void calcK();
    void calcCoeffs();

public:
    TypeName("StokesWave");

    StokesWave(const dictionary& dict);

    scalar k() const noexcept { return k_; }
    scalar omega() const noexcept { return 2.0 * M_PI / T_; }

    autoPtr<waveModel> clone() const override;
    void computeVel
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::MatrixXd> result
    ) const override;
    void computeEta
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::VectorXd> result
    ) const override;
};

}

#endif
