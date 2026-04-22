#include "CommandBuilder.h"

#include <QRegularExpression>
#include <QStringList>

namespace {
constexpr quint64 MiB = 1024ULL * 1024ULL;

QString mib(quint64 bytes) {
    return QString::number(bytes / MiB) + "MiB";
}
}

QString CommandBuilder::prettySize(quint64 bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    return QString::number(v, 'f', (u == 0 ? 0 : 1)) + " " + units[u];
}

CommandPlan CommandBuilder::build(const PhysicalDisk& disk, const PartitionPlan& plan) {
    CommandPlan cp;
    cp.title = "Plan poleceń dla " + disk.devicePath;

#if defined(Q_OS_MACOS)
    cp.commands.push_back("diskutil unmountDisk force " + disk.devicePath);
    if (plan.partitions.size() == 1) {
        const auto& p = plan.partitions[0];
        cp.commands.push_back("diskutil eraseDisk " + p.fs + " " + p.label + " MBRFormat " + disk.devicePath);
        return cp;
    }

    QStringList args;
    args << "diskutil" << "partitionDisk" << disk.devicePath << "GPT";
    for (const auto& p : plan.partitions) {
        args << p.fs << p.label;
        if (&p == &plan.partitions.back() && p.fs == "exFAT") {
            args << "R";
        } else {
            args << mib(p.sizeBytes);
        }
    }
    cp.commands.push_back(args.join(' '));

#elif defined(Q_OS_LINUX)
    cp.commands.push_back("wipefs -a " + disk.devicePath);
    cp.commands.push_back("parted -s " + disk.devicePath + " mklabel gpt");

    quint64 startMiB = 1;
    int idx = 1;
    for (const auto& p : plan.partitions) {
        const quint64 partMiB = p.sizeBytes / MiB;
        const quint64 endMiB = startMiB + partMiB;

        cp.commands.push_back(
            "parted -s " + disk.devicePath + " mkpart primary " + p.fs.toLower() + " "
            + QString::number(startMiB) + "MiB " + QString::number(endMiB) + "MiB");

        const QString partDev = disk.devicePath + QString::number(idx);
        if (p.fs == "FAT32") {
            cp.commands.push_back("mkfs.vfat -F 32 -n " + p.label + " " + partDev);
        } else {
            cp.commands.push_back("mkfs.exfat -n " + p.label + " " + partDev);
        }

        startMiB = endMiB;
        ++idx;
    }

#elif defined(Q_OS_WIN)
    {
        QRegularExpression diskNumRx("PHYSICALDRIVE(\\d+)", QRegularExpression::CaseInsensitiveOption);
        const auto diskMatch = diskNumRx.match(disk.devicePath);
        const QString diskNum = diskMatch.hasMatch() ? diskMatch.captured(1) : "?";

        cp.commands.push_back("select disk " + diskNum);
        cp.commands.push_back("clean");
        cp.commands.push_back("convert mbr");
        cp.commands.push_back("convert gpt");

        int idx = 1;
        for (const auto& p : plan.partitions) {
            cp.commands.push_back("create partition primary size=" + QString::number(p.sizeBytes / MiB));
            if (p.fs == "FAT32") {
                cp.commands.push_back("format fs=fat32 quick label=" + p.label);
            } else {
                cp.commands.push_back("format fs=exfat quick label=" + p.label);
            }
            cp.commands.push_back("assign");
            ++idx;
        }
    }

#else
    cp.commands.push_back("Unsupported OS");
#endif

    return cp;
}
