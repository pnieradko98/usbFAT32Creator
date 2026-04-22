#pragma once

#include "Models.h"

#include <vector>

class DiskManager {
public:
    std::vector<PhysicalDisk> listPhysicalDevices() const;

private:
    static QString runAndRead(const QString& program, const QStringList& args);
    static std::vector<PhysicalDisk> parseMacDisks();
    static std::vector<PhysicalDisk> parseLinuxDisks();
    static std::vector<PhysicalDisk> parseWindowsDisks();
};
