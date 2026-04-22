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

std::vector<DiskPartInfo> DiskManager::listPartitions(const QString& devicePath) const {
#if defined(Q_OS_WIN)
    return listPartitionsWindows(devicePath);
#elif defined(Q_OS_MACOS)
    return listPartitionsMac(devicePath);
#elif defined(Q_OS_LINUX)
    return listPartitionsLinux(devicePath);
#else
    Q_UNUSED(devicePath)
    return {};
#endif
}

QStringList DiskManager::sampleDiskFiles(const QString& devicePath, int maxFiles) const {
    QStringList result;
    const auto parts = listPartitions(devicePath);
    for (const auto& p : parts) {
        const QString mount = p.mountPoint.trimmed();
        if (mount.isEmpty()) continue;
#if defined(Q_OS_WIN)
        const QString script = QString(
            "Get-ChildItem '%1\\' -Force -ErrorAction SilentlyContinue | "
            "Select-Object -First 8 -ExpandProperty Name"
        ).arg(mount);
        const QString out = runAndRead("powershell", {"-NoProfile", "-Command", script});
        for (const QString& f : out.split('\n', Qt::SkipEmptyParts)) {
            result.append(mount + "\\" + f.trimmed());
            if (result.size() >= maxFiles) return result;
        }
#else
        const QString out = runAndRead("/bin/sh",
            {"-c", QString("ls -1a -- '%1' 2>/dev/null | head -10").arg(mount)});
        for (const QString& f : out.split('\n', Qt::SkipEmptyParts)) {
            const QString name = f.trimmed();
            if (name == "." || name == "..") continue;
            result.append(mount + "/" + name);
            if (result.size() >= maxFiles) return result;
        }
#endif
    }
    return result;
}

// ─── Windows helpers ─────────────────────────────────────────────────────────

std::vector<DiskPartInfo> DiskManager::listPartitionsWindows(const QString& devicePath) {
    std::vector<DiskPartInfo> result;
    QRegularExpression rx(R"(PHYSICALDRIVE(\d+))");
    const auto m = rx.match(devicePath);
    if (!m.hasMatch()) return result;
    const QString diskNum = m.captured(1);

    const QString script = QString(
        "Get-Partition -DiskNumber %1 -ErrorAction SilentlyContinue | ForEach-Object { "
        "$p = $_; "
        "$v = Get-Volume -Partition $p -ErrorAction SilentlyContinue; "
        "[PSCustomObject]@{ "
        "PartitionNumber = $p.PartitionNumber; "
        "DriveLetter = $p.DriveLetter; "
        "Size = $p.Size; "
        "FileSystem = if($v){$v.FileSystem}else{''}; "
        "Label = if($v){$v.FileSystemLabel}else{''} "
        "} "
        "} | ConvertTo-Csv -NoTypeInformation"
    ).arg(diskNum);

    const QString csv = runAndRead("powershell", {"-NoProfile", "-Command", script});
    if (csv.isEmpty()) return result;

    const auto lines = csv.split('\n', Qt::SkipEmptyParts);
    for (int i = 1; i < lines.size(); ++i) {
        QString l = lines[i].trimmed();
        if (l.isEmpty()) continue;
        if (l.startsWith('"')) l = l.mid(1);
        if (l.endsWith('"')) l = l.left(l.size() - 1);
        const auto cols = l.split("\",\"");
        if (cols.size() < 5) continue;

        DiskPartInfo info;
        info.name = "Partition " + cols[0];
        info.fsType = cols[3].trimmed();
        info.label = cols[4].trimmed();
        info.sizeBytes = cols[2].toULongLong();
        const QString dl = cols[1].trimmed();
        if (!dl.isEmpty() && dl.size() == 1 && dl[0].isLetter())
            info.mountPoint = dl + ":";
        result.push_back(info);
    }
    return result;
}

// ─── Linux helpers ────────────────────────────────────────────────────────────

std::vector<DiskPartInfo> DiskManager::listPartitionsLinux(const QString& devicePath) {
    std::vector<DiskPartInfo> result;
    const QString out = runAndRead("lsblk",
        {"-b", "-n", "-l", "-o", "NAME,SIZE,FSTYPE,LABEL,MOUNTPOINT", devicePath});
    if (out.isEmpty()) return result;

    bool first = true;
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        if (first) { first = false; continue; } // skip the disk row itself
        const QStringList cols = line.simplified().split(' ');
        if (cols.isEmpty()) continue;
        DiskPartInfo info;
        info.name       = cols.value(0);
        info.sizeBytes  = cols.value(1).toULongLong();
        info.fsType     = cols.value(2);
        info.label      = cols.value(3);
        info.mountPoint = cols.value(4);
        result.push_back(info);
    }
    return result;
}

// ─── macOS helpers ────────────────────────────────────────────────────────────

std::vector<DiskPartInfo> DiskManager::listPartitionsMac(const QString& devicePath) {
    std::vector<DiskPartInfo> result;
    const QString out = runAndRead("diskutil", {"list", devicePath});
    if (out.isEmpty()) return result;

    // Example line: "   1:                        EFI EFI   209.7 MB   disk0s1"
    const QRegularExpression lineRx(
        R"(^\s+(\d+):\s+\S+\s+.*?\s+([\d.]+)\s+(KB|MB|GB|TB)\s+(\S+)\s*$)");

    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        const auto m = lineRx.match(line);
        if (!m.hasMatch()) continue;
        bool ok = false;
        double sz = m.captured(2).toDouble(&ok);
        if (ok) {
            const QString unit = m.captured(3);
            if      (unit == "KB") sz *= 1024.0;
            else if (unit == "MB") sz *= 1024.0 * 1024;
            else if (unit == "GB") sz *= 1024.0 * 1024 * 1024;
            else if (unit == "TB") sz *= 1024.0 * 1024 * 1024 * 1024;
        }
        DiskPartInfo info;
        info.name      = m.captured(4);
        info.sizeBytes = ok ? static_cast<quint64>(sz) : 0;
        // Check if there's a mounted volume for this identifier
        const QString mp = runAndRead("/bin/sh",
            {"-c", QString("diskutil info '%1' 2>/dev/null | grep 'Mount Point' | awk '{print $3}'")
                .arg(m.captured(4))}).trimmed();
        if (!mp.isEmpty() && mp != "Not") info.mountPoint = mp;
        result.push_back(info);
    }
    return result;
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

        // Detect macOS system disk: check if "/" is on this disk
        const QString rootWhole = runAndRead("/bin/sh",
            {"-c", "diskutil info / 2>/dev/null | grep 'Part of Whole' | awk '{print $NF}'"}).trimmed();
        disk.isSystem = (!rootWhole.isEmpty() && dev.contains(rootWhole));

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

        // Detect Linux system disk: check if "/" is on this device
        const QString rootSrc = runAndRead("/bin/sh",
            {"-c", "df / 2>/dev/null | awk 'NR==2{print $1}'"}).trimmed();
        // rootSrc is e.g. /dev/sda2 or /dev/nvme0n1p1 – check prefix match
        disk.isSystem = !rootSrc.isEmpty() && rootSrc.startsWith(disk.devicePath);

        out.push_back(disk);
    }

    return out;
}

std::vector<PhysicalDisk> DiskManager::parseWindowsDisks() {
    std::vector<PhysicalDisk> out;

    // Get list of system/boot disk numbers
    const QString sysScript =
        "try { (Get-Disk | Where-Object {$_.IsSystem -or $_.IsBoot}).Number -join ',' } catch { '' }";
    const QString sysOut = runAndRead("powershell", {"-NoProfile", "-Command", sysScript}).trimmed();
    std::vector<int> sysDiskNums;
    for (const QString& s : sysOut.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const int n = s.trimmed().toInt(&ok);
        if (ok) sysDiskNums.push_back(n);
    }

    const QString script =
        "Get-CimInstance Win32_DiskDrive | "
        "Select-Object DeviceID,Model,Size,InterfaceType | "
        "ConvertTo-Csv -NoTypeInformation";
    const QString csv = runAndRead("powershell", {"-NoProfile", "-Command", script});
    if (csv.isEmpty()) {
        return out;
    }

    const QRegularExpression numRx(R"(PHYSICALDRIVE(\d+))");
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

        // Check if this is a system/boot disk
        const auto numMatch = numRx.match(disk.devicePath);
        if (numMatch.hasMatch()) {
            bool ok = false;
            const int diskNum = numMatch.captured(1).toInt(&ok);
            if (ok) {
                for (int sn : sysDiskNums) {
                    if (sn == diskNum) { disk.isSystem = true; break; }
                }
            }
        }

        out.push_back(disk);
    }

    return out;
}
