#include "waveModel.hpp"

namespace Foam
{

defineTypeNameAndDebug(waveModel, 0);
defineRunTimeSelectionTable(waveModel, dictionary);

HashTable<autoPtr<waveModel>> waveModel::registry_;

void waveModel::updateTime(scalar t)
{
    t_ = t;
}

void waveModel::getEta
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::VectorXd> result
) const
{
    computeEta(points, result);
}

void waveModel::getVel
(
    const Eigen::Ref<const Eigen::MatrixXd>& points,
    Eigen::Ref<Eigen::MatrixXd> result
) const
{
    computeVel(points, result);
}

autoPtr<waveModel> waveModel::New(const dictionary& dict)
{
    const word modelType(dict.get<word>("waveType"));

    auto* ctorPtr = dictionaryConstructorTablePtr_;

    auto cstrIter = ctorPtr->cfind(modelType);

    if (!cstrIter.found())
    {
        FatalIOErrorInFunction(dict)
            << "Unknown waveModel type " << modelType << nl
            << "Valid waveModel types:" << nl
            << ctorPtr->sortedToc()
            << exit(FatalIOError);
    }

    return cstrIter()(dict);
}

waveModel& waveModel::getOrCreate(const word& name, const dictionary& dict)
{
    auto iter = registry_.find(name);
    if (iter.found())
    {
        return *iter();
    }
    autoPtr<waveModel> ptr = New(dict);
    registry_.insert(name, std::move(ptr));
    return *registry_.find(name)();
}

}
