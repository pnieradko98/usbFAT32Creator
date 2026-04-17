#include "DiskManager.h"
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

QString DiskManager::runAndRead(const QString& program, const QStringList& args) {
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        return {};
    }
    const QByteArray out = proc.readAllStandardOutput();
    return QString::fromUtf8(out);
}

std::vector<PhysicalDisk> DiskManager::listPhysicalDevices() const {
#if defined(Q_OS_MACOS)
    return parseMacDisks();
#elif defined(Q_OS_LINUX)
    return parseLinuxDisks();
#elif defined(Q_OS_WIN)
    return parseWindowsDisks();
#else
    return {};
#endif
}

std::vector<PhysicalDisk> DiskManager::parseMacDisks() {
    std::vector<PhysicalDisk> out;
    const QString list = runAndRead("diskutil", {"list"});
    if (list.isEmpty()) {
        return out;
    }

    const QRegularExpression rx(R"((/dev/disk\d+)\s+\(([^\)]*)\):)");
    const auto lines = list.split('\n');
    for (const QString& line : lines) {
        const auto m = rx.match(line.trimmed());
        if (!m.hasMatch()) {
            continue;
        }

        const QString dev = m.captured(1);
        const QString attrs = m.captured(2).toLower();
        if (!attrs.contains("physical")) {
            continue;
        }

        PhysicalDisk disk;
        disk.devicePath = dev;
        disk.displayName = dev;
        disk.removable = attrs.contains("external") || attrs.contains("removable");
        disk.likelyUsb = attrs.contains("external");

        const QString info = runAndRead("diskutil", {"info", dev});
        QRegularExpression sizeRx(R"(Disk Size:\s+.*\((\d+)\s+Bytes\))");
        QRegularExpression modelRx(R"(Device\s*/\s*Media Name:\s*(.+)$)");

        const auto sizeMatch = sizeRx.match(info);
        if (sizeMatch.hasMatch()) {
            disk.sizeBytes = sizeMatch.captured(1).toULongLong();
        }
        const auto modelMatch = modelRx.match(info, 0, QRegularExpression::NormalMatch,
            QRegularExpression::AnchorAtOffsetMatchOption);
        if (modelMatch.hasMatch()) {
            disk.model = modelMatch.captured(1).trimmed();
        }

        out.push_back(disk);
    }

    return out;
}

std::vector<PhysicalDisk> DiskManager::parseLinuxDisks() {
    std::vector<PhysicalDisk> out;
    const QString text = runAndRead("lsblk", {"-b", "-dn", "-o", "NAME,SIZE,TYPE,TRAN,RM,MODEL"});
    if (text.isEmpty()) {
        return out;
    }

    const auto lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const auto parts = line.simplified().split(' ');
        if (parts.size() < 6) {
            continue;
        }

        const QString name = parts[0];
        const QString size = parts[1];
        const QString type = parts[2];
        const QString tran = parts[3].toLower();
        const QString rm = parts[4];
        const QString model = parts.mid(5).join(" ");

        if (type != "disk") {
            continue;
        }

        PhysicalDisk disk;
        disk.devicePath = "/dev/" + name;
        disk.displayName = disk.devicePath;
        disk.sizeBytes = size.toULongLong();
        disk.model = model;
        disk.removable = (rm == "1");
        disk.likelyUsb = (tran == "usb") || disk.removable;

        out.push_back(disk);
    }

    return out;
}

std::vector<PhysicalDisk> DiskManager::parseWindowsDisks() {
    std::vector<PhysicalDisk> out;

    const QString script =
        "Get-CimInstance Win32_DiskDrive | "
        "Select-Object DeviceID,Model,Size,InterfaceType | "
        "ConvertTo-Csv -NoTypeInformation";
    const QString csv = runAndRead("powershell", {"-NoProfile", "-Command", script});
    if (csv.isEmpty()) {
        return out;
    }

    const auto lines = csv.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return out;
    }

    for (int i = 1; i < lines.size(); ++i) {
        QString l = lines[i].trimmed();
        if (l.isEmpty()) {
            continue;
        }
        if (l.startsWith('"') && l.endsWith('"')) {
            l = l.mid(1, l.size() - 2);
        }
        const auto parts = l.split("\",\"");
        if (parts.size() < 4) {
            continue;
        }

        PhysicalDisk disk;
        disk.devicePath = parts[0];
        disk.displayName = parts[0];
        disk.model = parts[1];
        disk.sizeBytes = parts[2].toULongLong();
        disk.removable = parts[3].toLower().contains("usb");
        disk.likelyUsb = disk.removable;

        out.push_back(disk);
    }

    return out;
}
