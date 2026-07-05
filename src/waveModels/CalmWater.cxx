#include "waveModels/CalmWater.hpp"
#include "addToRunTimeSelectionTable.H"

namespace Foam
{

defineTypeNameAndDebug(CalmWater, 0);
addToRunTimeSelectionTable(waveModel, CalmWater, dictionary);

}
