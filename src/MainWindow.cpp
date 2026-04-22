#include "MainWindow.h"

#include "CommandBuilder.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

namespace {
constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
constexpr quint64 FAT32_MAX = 32ULL * GiB;
constexpr quint64 MIN_EXFAT = 256ULL * 1024ULL * 1024ULL;

quint64 maxFat32CountForDisk(quint64 totalBytes) {
    if (totalBytes == 0) {
        return 0;
    }
    return (totalBytes + FAT32_MAX - 1) / FAT32_MAX;
}
}

MainWindow::MainWindow() {
    setupUi();

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(3000);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::autoRefreshDevices);
    m_autoRefreshTimer->start();

    refreshDevices();
}

void MainWindow::setupUi() {
    setWindowTitle("USB FAT32 Creator v1.3.0");
    resize(980, 640);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel("Urządzenie fizyczne:"));

    m_deviceCombo = new QComboBox();
    top->addWidget(m_deviceCombo, 1);

    top->addWidget(new QLabel("Liczba FAT32 (0=auto):"));
    m_virtualCount = new QSpinBox();
    m_virtualCount->setRange(0, 64);
    m_virtualCount->setValue(1);
    top->addWidget(m_virtualCount);

    top->addWidget(new QLabel("Reszta miejsca:"));
    m_tailFsCombo = new QComboBox();
    m_tailFsCombo->addItem("exFAT", "exfat");
    m_tailFsCombo->addItem("FAT32", "fat32");
    m_tailFsCombo->addItem("Brak partycji", "none");
    top->addWidget(m_tailFsCombo);

    m_maxFat32Label = new QLabel("Maks FAT32: -");
    top->addWidget(m_maxFat32Label);

    m_refreshBtn    = new QPushButton("Odśwież");
    m_partitionsBtn = new QPushButton("Pokaż partycje");
    m_exportLogBtn  = new QPushButton("Zapisz log");
    m_planBtn       = new QPushButton("Zbuduj plan");
    m_testBtn       = new QPushButton("Testuj");
    m_execBtn       = new QPushButton("Wykonaj");

    top->addWidget(m_refreshBtn);
    top->addWidget(m_partitionsBtn);
    top->addWidget(m_exportLogBtn);
    top->addWidget(m_planBtn);
    top->addWidget(m_testBtn);
    top->addWidget(m_execBtn);

    root->addLayout(top);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(6);
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_output = new QPlainTextEdit();
    m_output->setReadOnly(true);
    root->addWidget(m_output, 1);

    setCentralWidget(central);

    connect(m_refreshBtn,    &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_partitionsBtn, &QPushButton::clicked, this, &MainWindow::showCurrentPartitions);
    connect(m_exportLogBtn,  &QPushButton::clicked, this, &MainWindow::exportLog);
    connect(m_planBtn,       &QPushButton::clicked, this, &MainWindow::buildPlan);
    connect(m_testBtn,       &QPushButton::clicked, this, &MainWindow::testPlan);
    connect(m_execBtn,       &QPushButton::clicked, this, &MainWindow::executePlan);
    connect(m_deviceCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateFat32Info);
    connect(m_tailFsCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateFat32Info);
}

void MainWindow::autoRefreshDevices() {
    if (m_isBusy) {
        return;
    }
    refreshDevicesImpl(false);
}

void MainWindow::refreshDevices() {
    refreshDevicesImpl(true);
}

void MainWindow::refreshDevicesImpl(bool verboseLog) {
    const QString selectedPath = (m_deviceCombo->currentIndex() >= 0
        && m_deviceCombo->currentIndex() < static_cast<int>(m_disks.size()))
        ? m_disks[static_cast<size_t>(m_deviceCombo->currentIndex())].devicePath
        : QString();

    const auto newDisks = m_diskManager.listPhysicalDevices();
    bool changed = (newDisks.size() != m_disks.size());
    if (!changed) {
        for (size_t i = 0; i < newDisks.size(); ++i) {
            if (newDisks[i].devicePath != m_disks[i].devicePath
                || newDisks[i].sizeBytes != m_disks[i].sizeBytes
                || newDisks[i].isSystem != m_disks[i].isSystem) {
                changed = true;
                break;
            }
        }
    }

    if (!verboseLog && !changed) {
        return;
    }

    m_disks = newDisks;
    m_deviceCombo->clear();

    for (const auto& d : m_disks) {
        const QString sysTag = d.isSystem ? " [DYSK SYSTEMOWY]" : "";
        const QString row = QString("%1 | %2 | %3 | %4%5")
            .arg(d.devicePath)
            .arg(d.model.isEmpty() ? "(unknown model)" : d.model)
            .arg(CommandBuilder::prettySize(d.sizeBytes))
            .arg(d.likelyUsb ? "USB/wymienny" : "wewnętrzny?")
            .arg(sysTag);
        m_deviceCombo->addItem(row);
    }

    if (!selectedPath.isEmpty()) {
        for (int i = 0; i < static_cast<int>(m_disks.size()); ++i) {
            if (m_disks[static_cast<size_t>(i)].devicePath == selectedPath) {
                m_deviceCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    if (verboseLog) {
        m_output->appendPlainText("[" + QDateTime::currentDateTime().toString() + "] znaleziono urządzeń: "
            + QString::number(m_disks.size()));
    } else {
        m_output->appendPlainText("[AUTO] Zaktualizowano listę urządzeń: " + QString::number(m_disks.size()));
    }
    updateFat32Info();
}

void MainWindow::setUiBusy(bool busy) {
    m_isBusy = busy;
    m_refreshBtn->setEnabled(!busy);
    m_partitionsBtn->setEnabled(!busy);
    m_exportLogBtn->setEnabled(!busy);
    m_planBtn->setEnabled(!busy);
    m_testBtn->setEnabled(!busy);
    m_execBtn->setEnabled(!busy);
    m_deviceCombo->setEnabled(!busy);
    m_virtualCount->setEnabled(!busy);
    m_tailFsCombo->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (!busy) {
        m_progress->setRange(0, 1);
        m_progress->setValue(1);
    }
}

bool MainWindow::isSystemDiskSelected() const {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) return false;
    return m_disks[static_cast<size_t>(idx)].isSystem;
}

void MainWindow::updateFat32Info() {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) {
        m_maxFat32Label->setText("Maks FAT32: -");
        m_virtualCount->setRange(0, 64);
        return;
    }

    const auto& disk = m_disks[static_cast<size_t>(idx)];
    const quint64 maxFat32 = maxFat32CountForDisk(disk.sizeBytes);
    const int spinMax = static_cast<int>(maxFat32 > 0 ? maxFat32 : 1);
    m_virtualCount->setRange(0, spinMax);
    m_maxFat32Label->setText(QString("Maks FAT32: %1").arg(maxFat32));
}

void MainWindow::exportLog() {
    const QString defName = "usb_fat32_creator_log_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Zapisz log",
        QDir::homePath() + "/" + defName,
        "Plik tekstowy (*.txt)");
    if (path.isEmpty()) {
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Błąd zapisu", "Nie udało się zapisać logu do pliku.");
        return;
    }
    QTextStream s(&f);
    s << m_output->toPlainText();
    f.close();
    m_output->appendPlainText("[OK] Zapisano log: " + path);
}

void MainWindow::verifyAfterExecution(const PhysicalDisk& disk) {
    m_output->appendPlainText("\n=== WERYFIKACJA PO WYKONANIU ===");
    const auto actual = m_diskManager.listPartitions(disk.devicePath);
    if (actual.empty()) {
        m_output->appendPlainText("[WARN] Nie udało się odczytać partycji po wykonaniu.");
        return;
    }

    auto fsMatch = [](const QString& expected, const QString& actualFs) {
        const QString e = expected.toUpper();
        const QString a = actualFs.toUpper();
        if (e == "FAT32") {
            return a.contains("FAT32") || a == "FAT" || a.contains("VFAT") || a.contains("MSDOS");
        }
        if (e == "EXFAT") {
            return a.contains("EXFAT");
        }
        return e == a;
    };

    QStringList issues;
    if (actual.size() != m_lastPartitionPlan.partitions.size()) {
        issues << QString("Liczba partycji różni się: plan=%1, rzeczywiste=%2")
            .arg(m_lastPartitionPlan.partitions.size())
            .arg(actual.size());
    }

    const size_t minCount = std::min(actual.size(), m_lastPartitionPlan.partitions.size());
    const quint64 tolerance = 64ULL * 1024ULL * 1024ULL;
    for (size_t i = 0; i < minCount; ++i) {
        const auto& exp = m_lastPartitionPlan.partitions[i];
        const auto& act = actual[i];
        const quint64 diff = (exp.sizeBytes > act.sizeBytes)
            ? (exp.sizeBytes - act.sizeBytes)
            : (act.sizeBytes - exp.sizeBytes);

        if (!act.fsType.isEmpty() && !fsMatch(exp.fs, act.fsType)) {
            issues << QString("Partycja %1: oczekiwany FS=%2, wykryty FS=%3")
                .arg(i + 1)
                .arg(exp.fs)
                .arg(act.fsType);
        }
        if (diff > tolerance) {
            issues << QString("Partycja %1: różny rozmiar (plan=%2, rzeczywisty=%3)")
                .arg(i + 1)
                .arg(CommandBuilder::prettySize(exp.sizeBytes))
                .arg(CommandBuilder::prettySize(act.sizeBytes));
        }
    }

    if (issues.isEmpty()) {
        m_output->appendPlainText("[OK] Weryfikacja zakończona sukcesem.");
    } else {
        m_output->appendPlainText("[WARN] Weryfikacja wykryła różnice:");
        for (const QString& i : issues) {
            m_output->appendPlainText(" - " + i);
        }
    }
}

void MainWindow::showCurrentPartitions() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    setUiBusy(true);
    m_output->appendPlainText("\n=== AKTUALNE PARTYCJE na " + disk.devicePath + " ===");

    const auto parts = m_diskManager.listPartitions(disk.devicePath);
    if (parts.empty()) {
        m_output->appendPlainText("(brak partycji lub brak dostępu)");
    } else {
        for (const auto& p : parts) {
            const QString mp = p.mountPoint.isEmpty() ? "(bez litery/punktu)" : p.mountPoint;
            m_output->appendPlainText(QString("  %1 | %2 | %3 | %4")
                .arg(p.name)
                .arg(p.fsType.isEmpty() ? "?" : p.fsType)
                .arg(CommandBuilder::prettySize(p.sizeBytes))
                .arg(mp));
        }
    }
    setUiBusy(false);
}

bool MainWindow::hasSelectedDisk(PhysicalDisk& outDisk) const {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) {
        return false;
    }
    outDisk = m_disks[static_cast<size_t>(idx)];
    return true;
}

PartitionPlan MainWindow::buildCurrentPartitionPlan() const {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) {
        return {};
    }

    const auto& disk = m_disks[static_cast<size_t>(idx)];
    const QString tailMode = m_tailFsCombo->currentData().toString();
    const int requestedFat32 = m_virtualCount->value();

    PartitionPlan plan;
    if (disk.sizeBytes < 1ULL * GiB) {
        plan.summary = "Nośnik jest za mały na sensowny podział.";
        return plan;
    }

    const quint64 maxFat32 = maxFat32CountForDisk(disk.sizeBytes);
    quint64 fat32Count = 0;

    if (requestedFat32 <= 0) {
        if (tailMode == "exfat") {
            fat32Count = disk.sizeBytes / FAT32_MAX;
            if (fat32Count < 1) {
                fat32Count = 1;
            }
            const quint64 covered = fat32Count * FAT32_MAX;
            if (covered >= disk.sizeBytes && fat32Count > 1) {
                --fat32Count;
            }
        } else if (tailMode == "none") {
            fat32Count = disk.sizeBytes / FAT32_MAX;
            if (fat32Count < 1) {
                fat32Count = 1;
            }
        } else {
            fat32Count = maxFat32;
        }
    } else {
        fat32Count = static_cast<quint64>(requestedFat32);
    }

    if (fat32Count > maxFat32) {
        fat32Count = maxFat32;
    }

    quint64 used = 0;
    for (quint64 i = 0; i < fat32Count && used < disk.sizeBytes; ++i) {
        const quint64 remaining = disk.sizeBytes - used;
        quint64 partSize = (remaining > FAT32_MAX) ? FAT32_MAX : remaining;

        // Dla trybu exFAT staramy się nie zostawiać końcówki < 256 MiB.
        if (tailMode == "exfat" && i == fat32Count - 1) {
            const quint64 leftAfter = (remaining > partSize) ? (remaining - partSize) : 0;
            if (leftAfter > 0 && leftAfter < MIN_EXFAT && partSize + leftAfter <= FAT32_MAX) {
                partSize += leftAfter;
            }
        }

        PartitionSpec part;
        part.fs = "FAT32";
        part.label = QString("SKANY_%1").arg(i + 1);
        part.sizeBytes = partSize;
        plan.partitions.push_back(part);
        used += partSize;
    }

    if (used < disk.sizeBytes) {
        if (tailMode == "exfat") {
            PartitionSpec tail;
            tail.fs = "exFAT";
            tail.label = "PLIKI";
            tail.sizeBytes = disk.sizeBytes - used;
            plan.partitions.push_back(tail);
            used += tail.sizeBytes;
        } else if (tailMode == "fat32") {
            while (used < disk.sizeBytes) {
                const quint64 remaining = disk.sizeBytes - used;
                PartitionSpec tail;
                tail.fs = "FAT32";
                tail.label = QString("SKANY_%1").arg(plan.partitions.size() + 1);
                tail.sizeBytes = (remaining > FAT32_MAX) ? FAT32_MAX : remaining;
                plan.partitions.push_back(tail);
                used += tail.sizeBytes;
            }
        }
    }

    if (plan.partitions.size() == 1 && plan.partitions[0].fs == "FAT32") {
        plan.partitions[0].label = "SKANY";
    }

    int exfatCount = 0;
    int fatCount = 0;
    for (const auto& p : plan.partitions) {
        if (p.fs == "FAT32") {
            ++fatCount;
        } else if (p.fs == "exFAT") {
            ++exfatCount;
        }
    }
    const quint64 unallocated = (disk.sizeBytes > used) ? (disk.sizeBytes - used) : 0;
    plan.summary = QString("FAT32 partycji: %1, exFAT: %2, nieprzydzielone: %3")
        .arg(fatCount)
        .arg(exfatCount)
        .arg(CommandBuilder::prettySize(unallocated));

    return plan;
}

QStringList MainWindow::validatePlan(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    QStringList errors;

    if (plan.partitions.empty()) {
        errors << "Plan nie zawiera żadnych partycji.";
        return errors;
    }

    quint64 totalPlanned = 0;
    int exfatCount = 0;
    bool exfatIsLast = true;

    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        const int partNo = static_cast<int>(i + 1);

        if (part.sizeBytes == 0) {
            errors << QString("Partycja %1 ma rozmiar 0 B.").arg(partNo);
        }

        if (part.fs != "FAT32" && part.fs != "exFAT") {
            errors << QString("Partycja %1 ma nieobsługiwany system plików: %2.")
                .arg(partNo)
                .arg(part.fs);
        }

        if (part.fs == "FAT32" && part.sizeBytes > FAT32_MAX) {
            errors << QString("Partycja FAT32 #%1 przekracza 32 GiB.").arg(partNo);
        }

        if (part.fs == "exFAT") {
            ++exfatCount;
            if (i != plan.partitions.size() - 1) {
                exfatIsLast = false;
            }
            if (part.sizeBytes < MIN_EXFAT) {
                errors << QString("Partycja exFAT #%1 jest mniejsza niż 256 MiB.").arg(partNo);
            }
        }

        const quint64 newTotal = totalPlanned + part.sizeBytes;
        if (newTotal < totalPlanned) {
            errors << "Wykryto przepełnienie podczas sumowania rozmiarów partycji.";
            break;
        }
        totalPlanned = newTotal;
    }

    if (exfatCount > 1) {
        errors << "Plan zawiera więcej niż jedną partycję exFAT.";
    }
    if (exfatCount == 1 && !exfatIsLast) {
        errors << "Partycja exFAT musi być ostatnia w planie.";
    }

    const bool allowUnallocated = (m_tailFsCombo->currentData().toString() == "none");
    if (!allowUnallocated && totalPlanned != disk.sizeBytes) {
        errors << QString("Suma partycji (%1) nie zgadza się z rozmiarem nośnika (%2).")
            .arg(CommandBuilder::prettySize(totalPlanned))
            .arg(CommandBuilder::prettySize(disk.sizeBytes));
    }

    return errors;
}

void MainWindow::printPlanToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    m_output->appendPlainText("\n=== PLAN PARTYCJONOWANIA ===");
    m_output->appendPlainText("Urządzenie: " + disk.devicePath);
    m_output->appendPlainText("Podsumowanie: " + plan.summary);

    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        m_output->appendPlainText(QString("%1) %2 | %3 | %4")
            .arg(i + 1)
            .arg(part.fs)
            .arg(part.label)
            .arg(CommandBuilder::prettySize(part.sizeBytes)));
    }
}

void MainWindow::printSimulationToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    m_output->appendPlainText("\n=== TEST SYMULACJI (bez zmian na dysku) ===");

    quint64 offset = 0;
    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        const quint64 start = offset;
        const quint64 end = offset + part.sizeBytes;

        m_output->appendPlainText(QString("P%1 | %2 | %3 | %4 | zakres: %5 -> %6")
            .arg(i + 1)
            .arg(part.fs)
            .arg(part.label)
            .arg(CommandBuilder::prettySize(part.sizeBytes))
            .arg(CommandBuilder::prettySize(start))
            .arg(CommandBuilder::prettySize(end)));

        offset = end;
    }

    m_output->appendPlainText(QString("Suma partycji: %1 / Rozmiar nośnika: %2")
        .arg(CommandBuilder::prettySize(offset))
        .arg(CommandBuilder::prettySize(disk.sizeBytes)));
}

void MainWindow::buildPlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    const PartitionPlan p = buildCurrentPartitionPlan();
    const QStringList errors = validatePlan(disk, p);
    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] Walidacja planu nie powiodła się:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::critical(this, "Błędny plan", "Walidacja planu nie powiodła się. Sprawdź log.");
        m_lastPartitionPlan = {};
        m_lastCommandPlan = {};
        return;
    }

    m_lastPartitionPlan = p;
    m_lastCommandPlan = CommandBuilder::build(disk, p);

    printPlanToOutput(disk, p);

    m_output->appendPlainText("\n=== PLAN POLECEŃ ===");
    for (const auto& c : m_lastCommandPlan.commands) {
        m_output->appendPlainText(c);
    }
}

void MainWindow::testPlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    const PartitionPlan p = buildCurrentPartitionPlan();
    const QStringList errors = validatePlan(disk, p);

    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] TEST: plan nie przeszedł walidacji:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::warning(this, "Test nieudany", "Plan nie przeszedł walidacji. Sprawdź log.");
        return;
    }

    m_output->appendPlainText("\n[OK] TEST: plan przeszedł walidację.");
    printPlanToOutput(disk, p);
    printSimulationToOutput(disk, p);
}

void MainWindow::executePlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    // ── 1. Blokada dysku systemowego ─────────────────────────────────────────
    if (disk.isSystem) {
        QMessageBox::critical(this, "Blokada – dysk systemowy",
            QString("Wybrany dysk (%1) jest dyskiem systemowym!\n\n"
                    "Formatowanie dysku z systemem operacyjnym grozi "
                    "całkowitą utratą systemu.\n\n"
                    "Operacja jest zablokowana.")
            .arg(disk.devicePath));
        return;
    }

    if (m_lastPartitionPlan.partitions.empty() || m_lastCommandPlan.commands.empty()) {
        QMessageBox::information(this, "Brak planu", "Najpierw kliknij 'Zbuduj plan'.");
        return;
    }

    const QStringList errors = validatePlan(disk, m_lastPartitionPlan);
    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] Walidacja przed wykonaniem nie powiodła się:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::critical(this, "Błędny plan", "Plan jest niepoprawny. Zbuduj plan ponownie.");
        return;
    }

    // ── 2. Ostrzeżenie z listą plików ────────────────────────────────────────
    {
        m_output->appendPlainText("[INFO] Sprawdzam zawartość dysku...");
        const QStringList files = m_diskManager.sampleDiskFiles(disk.devicePath, 25);
        QString fileListMsg;
        if (!files.isEmpty()) {
            fileListMsg = "\n\nZnaleziono dane na dysku:\n";
            for (const QString& f : files) {
                fileListMsg += "  " + f + "\n";
            }
            if (files.size() >= 25) fileListMsg += "  ...(i więcej)\n";
        }

        QMessageBox confirmBox(this);
        confirmBox.setIcon(QMessageBox::Warning);
        confirmBox.setWindowTitle("Potwierdzenie – utrata danych");
        confirmBox.setText(QString("UWAGA: zostanie sformatowane urządzenie %1.\n"
            "Wszystkie dane na tym nośniku zostaną bezpowrotnie usunięte.\n"
            "Ta operacja jest nieodwracalna.%2\n\n"
            "Czy na pewno kontynuować?")
            .arg(disk.devicePath)
            .arg(fileListMsg));
        auto* yesBtn = confirmBox.addButton("Tak", QMessageBox::AcceptRole);
        confirmBox.addButton("Nie", QMessageBox::RejectRole);
        confirmBox.exec();
        if (confirmBox.clickedButton() != yesBtn) {
            return;
        }
    }

    // ── 3. Zapis skryptu i uruchomienie asynchroniczne ───────────────────────
#if defined(Q_OS_WIN)
    const QString scriptPath = QDir::tempPath() + "/diskpart_script.txt";
    {
        QFile f(scriptPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_output->appendPlainText("[ERR] Nie można utworzyć pliku tymczasowego: " + scriptPath);
            return;
        }
        QTextStream s(&f);
        for (const QString& cmd : m_lastCommandPlan.commands) s << cmd << "\n";
    }
    m_output->appendPlainText("[INFO] Skrypt diskpart: " + scriptPath);
    m_output->appendPlainText("[RUN] diskpart /s " + scriptPath);

    auto* proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString txt = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!txt.isEmpty()) m_output->appendPlainText(txt);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, scriptPath, disk](int exitCode, QProcess::ExitStatus) {
            const QString stdErr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            if (!stdErr.isEmpty()) m_output->appendPlainText("[ERR] " + stdErr);
            m_output->appendPlainText(exitCode == 0
                ? "[OK] Formatowanie zakończone pomyślnie."
                : "[ERR] Diskpart zakończył się z kodem: " + QString::number(exitCode));
            if (exitCode == 0) {
                verifyAfterExecution(disk);
            }
            QFile::remove(scriptPath);
            setUiBusy(false);
            proc->deleteLater();
        });

    setUiBusy(true);
    m_progress->setRange(0, 0); // indeterminate
    proc->start("diskpart", {"/s", scriptPath});
    if (!proc->waitForStarted(5000)) {
        m_output->appendPlainText("[ERR] Nie można uruchomić diskpart.");
        QFile::remove(scriptPath);
        setUiBusy(false);
        proc->deleteLater();
    }
#else
    // Zapisz wszystkie komendy jako skrypt powłoki
    const QString scriptPath = QDir::tempPath() + "/usb_fat32_script.sh";
    {
        QFile f(scriptPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_output->appendPlainText("[ERR] Nie można utworzyć skryptu: " + scriptPath);
            return;
        }
        QTextStream s(&f);
        s << "#!/bin/sh\nset -e\n";
        for (const QString& cmd : m_lastCommandPlan.commands) s << cmd << "\n";
    }
    m_output->appendPlainText("[RUN] /bin/sh " + scriptPath);

    auto* proc = new QProcess(this);
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString txt = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!txt.isEmpty()) m_output->appendPlainText(txt);
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        const QString txt = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!txt.isEmpty()) m_output->appendPlainText("[ERR] " + txt);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, scriptPath, disk](int exitCode, QProcess::ExitStatus) {
            m_output->appendPlainText(exitCode == 0
                ? "[OK] Formatowanie zakończone pomyślnie."
                : "[ERR] Skrypt zakończył się z kodem: " + QString::number(exitCode));
            if (exitCode == 0) {
                verifyAfterExecution(disk);
            }
            QFile::remove(scriptPath);
            setUiBusy(false);
            proc->deleteLater();
        });

    setUiBusy(true);
    m_progress->setRange(0, 0); // indeterminate
    proc->start("/bin/sh", {scriptPath});
    if (!proc->waitForStarted(5000)) {
        m_output->appendPlainText("[ERR] Nie można uruchomić skryptu.");
        QFile::remove(scriptPath);
        setUiBusy(false);
        proc->deleteLater();
    }
#endif
}
