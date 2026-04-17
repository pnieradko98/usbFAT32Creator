#pragma once

#include <QString>
#include <vector>

struct PhysicalDisk {
    QString devicePath;
    QString displayName;
    QString model;
    quint64 sizeBytes = 0;
    bool removable = false;
    bool likelyUsb = false;
};

struct PartitionSpec {
    QString fs;
    QString label;
    quint64 sizeBytes = 0;
};

struct PartitionPlan {
    QString summary;
    std::vector<PartitionSpec> partitions;
};

struct CommandPlan {
    QString title;
    std::vector<QString> commands;
};
