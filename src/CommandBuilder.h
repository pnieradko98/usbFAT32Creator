#pragma once

#include "Models.h"

class CommandBuilder {
public:
    static CommandPlan build(const PhysicalDisk& disk, const PartitionPlan& plan);
    static QString prettySize(quint64 bytes);
};
