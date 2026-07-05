#ifndef waveModel_H
#define waveModel_H

#include <Eigen/Core>
#include "autoPtr.H"
#include "dictionary.H"
#include "runTimeSelectionTables.H"

namespace Foam
{

class waveModel
{
protected:
    scalar t_ = 0;

    static HashTable<autoPtr<waveModel>> registry_;

public:
    TypeName("waveModel");

    declareRunTimeSelectionTable
    (
        autoPtr,
        waveModel,
        dictionary,
        (const dictionary& dict),
        (dict)
    );

    static autoPtr<waveModel> New(const dictionary& dict);
    static waveModel& getOrCreate(const word& name, const dictionary& dict);

    virtual autoPtr<waveModel> clone() const = 0;

    virtual void updateTime(scalar t);
    virtual void restore(scalar t, const word& timeName) {}

    void getVel
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::MatrixXd> result
    ) const;
    void getEta
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::VectorXd> result
    ) const;

    virtual void computeVel
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::MatrixXd> result
    ) const = 0;
    virtual void computeEta
    (
        const Eigen::Ref<const Eigen::MatrixXd>& points,
        Eigen::Ref<Eigen::VectorXd> result
    ) const = 0;

    virtual void write(Ostream& os) const {}

    virtual ~waveModel() = default;
};

}

#endif
