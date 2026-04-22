#pragma once

#include "Models.h"

#include <QStringList>
#include <vector>

class DiskManager {
public:
    std::vector<PhysicalDisk> listPhysicalDevices() const;
    std::vector<DiskPartInfo> listPartitions(const QString& devicePath) const;
    QStringList sampleDiskFiles(const QString& devicePath, int maxFiles = 20) const;

private:
    static QString runAndRead(const QString& program, const QStringList& args);
    static std::vector<PhysicalDisk> parseMacDisks();
    static std::vector<PhysicalDisk> parseLinuxDisks();
    static std::vector<PhysicalDisk> parseWindowsDisks();
    static std::vector<DiskPartInfo> listPartitionsWindows(const QString& devicePath);
    static std::vector<DiskPartInfo> listPartitionsLinux(const QString& devicePath);
    static std::vector<DiskPartInfo> listPartitionsMac(const QString& devicePath);
};
