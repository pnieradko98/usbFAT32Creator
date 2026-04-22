#pragma once

#include "DiskManager.h"

#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void autoRefreshDevices();
    void refreshDevices();
    void buildPlan();
    void testPlan();
    void executePlan();
    void showCurrentPartitions();
    void updateFat32Info();
    void exportLog();

private:
    void setupUi();
    void setUiBusy(bool busy);
    void refreshDevicesImpl(bool verboseLog);
    void verifyAfterExecution(const PhysicalDisk& disk);
    bool isSystemDiskSelected() const;
    bool hasSelectedDisk(PhysicalDisk& outDisk) const;
    PartitionPlan buildCurrentPartitionPlan() const;
    QStringList validatePlan(const PhysicalDisk& disk, const PartitionPlan& plan) const;
    void printPlanToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const;
    void printSimulationToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const;

    DiskManager m_diskManager;
    std::vector<PhysicalDisk> m_disks;

    QComboBox*    m_deviceCombo    = nullptr;
    QSpinBox*     m_virtualCount   = nullptr;
    QComboBox*    m_tailFsCombo    = nullptr;
    QLabel*       m_maxFat32Label  = nullptr;
    QPlainTextEdit* m_output       = nullptr;
    QPushButton*  m_refreshBtn     = nullptr;
    QPushButton*  m_partitionsBtn  = nullptr;
    QPushButton*  m_exportLogBtn   = nullptr;
    QPushButton*  m_planBtn        = nullptr;
    QPushButton*  m_testBtn        = nullptr;
    QPushButton*  m_execBtn        = nullptr;
    QProgressBar* m_progress       = nullptr;
    QTimer*       m_autoRefreshTimer = nullptr;
    bool          m_isBusy = false;

    PartitionPlan m_lastPartitionPlan;
    CommandPlan   m_lastCommandPlan;
};
