#pragma once

#include "Models.h"

class VirtualPenPlanner {
public:
    static PartitionPlan buildManyFat32ThenExfat(quint64 totalBytes, int requestedFat32Parts);
};
