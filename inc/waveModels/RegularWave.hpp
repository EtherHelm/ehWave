#ifndef RegularWave_H
#define RegularWave_H

#include "waveModel.hpp"
#include <Eigen/Core>

namespace Foam
{

class RegularWave
:
    public waveModel
{
    scalar L_, D_, H_, waveCrestIni_;
    scalar waveDirX_ = 1.0, waveDirY_ = 0.0;
    Eigen::VectorXd eta_, b_, x_eta_;
    scalar k0_, kd_;
    scalar deff_, exp_top_;
    scalar ce_;

    int nx_ = 8192;   // core points
    int nz_ = 36;     // core points
    int nxp_, nzp_;   // padded size = core + 4
    Eigen::VectorXd xgrid_, zgrid_;
    Eigen::MatrixXd Vx_, Vz_;

    void computeSSGW();
    void buildGrids();

    double cubic1d(const Eigen::VectorXd& x, const Eigen::VectorXd& f,
                   double xq) const;
    double bicubic2d(const Eigen::VectorXd& x, const Eigen::VectorXd& z,
                     const Eigen::MatrixXd& F, double xq, double zq) const;

public:
    TypeName("RegularWave");

    RegularWave(const dictionary& dict);

    autoPtr<waveModel> clone() const override;

    scalar L() const noexcept { return L_; }
    scalar D() const noexcept { return D_; }
    scalar H() const noexcept { return H_; }
    scalar k0() const noexcept { return k0_; }
    scalar kd() const noexcept { return kd_; }
    const Eigen::VectorXd& eta() const noexcept { return eta_; }
    const Eigen::MatrixXd& Vx() const noexcept { return Vx_; }
    const Eigen::MatrixXd& Vz() const noexcept { return Vz_; }
    const Eigen::VectorXd& b() const noexcept { return b_; }
    

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
