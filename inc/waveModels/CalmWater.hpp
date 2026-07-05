#ifndef CalmWater_H
#define CalmWater_H

#include "waveModel.hpp"

namespace Foam
{

class CalmWater
:
    public waveModel
{
public:
    TypeName("CalmWater");

    CalmWater(const dictionary&) {}

    autoPtr<waveModel> clone() const override
    {
        return autoPtr<waveModel>(new CalmWater(*this));
    }

    void computeVel
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::MatrixXd> result
    ) const override
    {
        result.setZero();
    }

    void computeEta
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::VectorXd> result
    ) const override
    {
        result.setZero();
    }
};

}

#endif
