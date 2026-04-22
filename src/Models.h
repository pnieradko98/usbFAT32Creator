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
    bool isSystem = false;
};

struct DiskPartInfo {
    QString name;
    quint64 sizeBytes = 0;
    QString fsType;
    QString label;
    QString mountPoint; // drive letter on Windows (e.g. "C:"), path on Linux/macOS
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
