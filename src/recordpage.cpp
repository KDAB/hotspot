/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "recordpage.h"
#include "ui_recordpage.h"

#include "processfiltermodel.h"
#include "processmodel.h"
#include "resultsutil.h"
#include "util.h"

#include <QDebug>
#include <QKeyEvent>
#include <QListView>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <KColumnResizer>
#include <KComboBox>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KShell>
#include <KUrlCompletion>
#include <KUrlRequester>
#include <kio_version.h>

#include <Solid/Device>
#include <Solid/Processor>

#include "hotspot-config.h"

#include "multiconfigwidget.h"
#include "perfoutputwidgetkonsole.h"
#include "perfoutputwidgettext.h"
#include "perfrecord.h"

namespace {
bool isIntel()
{
    using namespace Solid;
    const auto list = Device::listFromType(DeviceInterface::Processor, QString());
    if (list.isEmpty()) {
        return false;
    }

    const auto& device = list[0];
    if (!device.is<Processor>()) {
        return false;
    }

    const auto* processor = device.as<Processor>();
    const auto instructionSets = processor->instructionSets();

    return instructionSets.testFlag(Processor::IntelMmx) || instructionSets.testFlag(Processor::IntelSse)
        || instructionSets.testFlag(Processor::IntelSse2) || instructionSets.testFlag(Processor::IntelSse3)
        || instructionSets.testFlag(Processor::IntelSsse3) || instructionSets.testFlag(Processor::IntelSse4)
        || instructionSets.testFlag(Processor::IntelSse41) || instructionSets.testFlag(Processor::IntelSse42);
}

RecordType selectedRecordType(const QScopedPointer<Ui::RecordPage>& ui)
{
    return ui->recordTypeComboBox->currentData().value<RecordType>();
}

void updateStartRecordingButtonState(const QScopedPointer<Ui::RecordPage>& ui)
{
    if (!PerfRecord::isPerfInstalled()) {
        ui->startRecordingButton->setEnabled(false);
        ui->applicationRecordErrorMessage->setText(QObject::tr("Please install perf before trying to record."));
        ui->applicationRecordErrorMessage->setVisible(true);
        return;
    }

    bool enabled = false;
    switch (selectedRecordType(ui)) {
    case LaunchApplication:
        enabled = ui->applicationName->url().isValid();
        break;
    case AttachToProcess:
        enabled = ui->processesTableView->selectionModel()->hasSelection();
        break;
    case ProfileSystem:
        enabled = true;
        break;
    case NUM_RECORD_TYPES:
        break;
    }
    enabled &= ui->applicationRecordErrorMessage->text().isEmpty();

    ui->startRecordingButton->setEnabled(enabled);
}

KConfigGroup config()
{
    return KSharedConfig::openConfig()->group("RecordPage");
}

KConfigGroup applicationConfig(const QString& application)
{
    return config().group(QLatin1String("Application ") + KShell::tildeExpand(application));
}

constexpr const int MAX_COMBO_ENTRIES = 10;

void rememberCombobox(KConfigGroup config, const QString& entryName, const QString& value, QComboBox* combo)
{
    // remove the value if it exists already
    auto idx = combo->findText(value);
    if (idx != -1) {
        combo->removeItem(idx);
    }

    // insert value on front
    combo->insertItem(0, value);
    combo->setCurrentIndex(0);

    // store the values in the config
    QStringList values;
    values.reserve(combo->count());
    for (int i = 0, c = std::min(MAX_COMBO_ENTRIES, combo->count()); i < c; ++i) {
        values << combo->itemText(i);
    }
    config.writeEntry(entryName, values);
}

void restoreCombobox(const KConfigGroup& config, const QString& entryName, QComboBox* combo,
                     const QStringList& defaults = {})
{
    combo->clear();
    const auto& values = config.readEntry(entryName, defaults);
    for (const auto& application : values) {
        combo->addItem(application);
    }
}

void rememberApplication(const QString& application, const QString& appParameters, const QString& workingDir,
                         KComboBox* combo)
{
    // set the app config early, so when we change the combo box below we can
    // restore the options as expected
    auto options = applicationConfig(application);
    options.writeEntry("params", appParameters);
    options.writeEntry("workingDir", workingDir);

    rememberCombobox(config(), QStringLiteral("applications"), application, combo);
}
}

RecordPage::RecordPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::RecordPage)
    , m_perfRecord(new PerfRecord(this))
    , m_updateRuntimeTimer(new QTimer(this))
    , m_watcher(new QFutureWatcher<ProcDataList>(this))
{
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setFrameStyle(QFrame::NoFrame);
        layout->addWidget(scrollArea);
        auto* contents = new QWidget(this);
        scrollArea->setWidget(contents);
        scrollArea->setWidgetResizable(true);

        ui->setupUi(contents);
    }

    auto completion = ui->applicationName->completionObject();
    ui->applicationName->comboBox()->setEditable(true);
    // NOTE: workaround until https://phabricator.kde.org/D7966 has landed and we bump the required version
    ui->applicationName->comboBox()->setCompletionObject(completion);
    ui->applicationName->setMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
#if KIO_VERSION >= QT_VERSION_CHECK(5, 31, 0)
    // we are only interested in executable files, so set the mime type filter accordingly
    // note that exe's build with PIE are actually "shared libs"...
    ui->applicationName->setMimeTypeFilters(
        {QStringLiteral("application/x-executable"), QStringLiteral("application/x-sharedlib")});
#endif
    ui->workingDirectory->setMode(KFile::Directory | KFile::LocalOnly);
    ui->outputFile->setText(QDir::currentPath() + QDir::separator() + QStringLiteral("perf.data"));
    ui->outputFile->setMode(KFile::File | KFile::LocalOnly);
    ui->eventTypeBox->lineEdit()->setPlaceholderText(tr("perf defaults (usually cycles:Pu)"));

    m_perfOutput = PerfOutputWidgetKonsole::create(this);
    if (!m_perfOutput) {
        m_perfOutput = new PerfOutputWidgetText(this);
    }
    ui->recordOutputBoxLayout->insertWidget(1, m_perfOutput);

    connect(m_perfOutput, &PerfOutputWidget::sendInput, this,
            [this](const QByteArray& input) { m_perfRecord->sendInput(input); });

    auto saveFunction = [this](KConfigGroup group) {
        group.writeEntry("params", ui->applicationParametersBox->text());
        group.writeEntry("workingDir", ui->workingDirectory->text());
    };

    auto restoreFunction = [this](const KConfigGroup& group) {
        ui->applicationParametersBox->setText(group.readEntry("params", ""));
        ui->workingDirectory->setText(group.readEntry("workingDir", ""));
        setError({});
    };

    m_multiConfig = new MultiConfigWidget(this);

    connect(m_multiConfig, &MultiConfigWidget::saveConfig, this, saveFunction);
    connect(m_multiConfig, &MultiConfigWidget::restoreConfig, this, restoreFunction);

    m_multiConfig->setConfig(applicationConfig(ui->applicationName->text()));

    ui->launchAppBox->layout()->addWidget(m_multiConfig);

    connect(ui->applicationParametersBox, &QLineEdit::editingFinished, m_multiConfig,
            &MultiConfigWidget::updateCurrentConfig);
    connect(ui->workingDirectory, &KUrlRequester::textChanged, m_multiConfig, &MultiConfigWidget::updateCurrentConfig);

    auto columnResizer = new KColumnResizer(this);
    columnResizer->addWidgetsFromLayout(ui->formLayout);
    columnResizer->addWidgetsFromLayout(ui->formLayout_1);
    columnResizer->addWidgetsFromLayout(ui->formLayout_2);
    columnResizer->addWidgetsFromLayout(ui->formLayout_3);

    connect(ui->homeButton, &QPushButton::clicked, this, &RecordPage::homeButtonClicked);
    connect(ui->applicationName, &KUrlRequester::textChanged, this, &RecordPage::onApplicationNameChanged);
    // NOTE: workaround until https://phabricator.kde.org/D7968 has landed and we bump the required version
    connect(ui->applicationName->comboBox()->lineEdit(), &QLineEdit::textChanged, this,
            &RecordPage::onApplicationNameChanged);
    connect(ui->startRecordingButton, &QPushButton::toggled, this, &RecordPage::onStartRecordingButtonClicked);
    connect(ui->workingDirectory, &KUrlRequester::textChanged, this, &RecordPage::onWorkingDirectoryNameChanged);
    connect(ui->viewPerfRecordResultsButton, &QPushButton::clicked, this,
            &RecordPage::onViewPerfRecordResultsButtonClicked);
    connect(ui->outputFile, &KUrlRequester::textChanged, this, &RecordPage::onOutputFileNameChanged);
    connect(ui->outputFile, static_cast<void (KUrlRequester::*)(const QString&)>(&KUrlRequester::returnPressed), this,
            &RecordPage::onOutputFileNameSelected);
    connect(ui->outputFile, &KUrlRequester::urlSelected, this, &RecordPage::onOutputFileUrlChanged);

    ui->recordTypeComboBox->addItem(QIcon::fromTheme(QStringLiteral("run-build")), tr("Launch Application"),
                                    QVariant::fromValue(LaunchApplication));
    ui->recordTypeComboBox->addItem(QIcon::fromTheme(QStringLiteral("run-install")), tr("Attach To Process(es)"),
                                    QVariant::fromValue(AttachToProcess));
    ui->recordTypeComboBox->addItem(QIcon::fromTheme(QStringLiteral("run-build-install-root")), tr("Profile System"),
                                    QVariant::fromValue(ProfileSystem));
    connect(ui->recordTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &RecordPage::updateRecordType);

    {
        ui->callGraphComboBox->addItem(tr("None"), QVariant::fromValue(QString()));
        ui->callGraphComboBox->setItemData(
            ui->callGraphComboBox->count() - 1,
            tr("<qt>Do not unwind the call stack. This results in tiny data files. "
               " But the data can be hard to make use of, when hotspots lie "
               " in third party or system libraries not under your direct control.</qt>"),
            Qt::ToolTipRole);

        const auto dwarfIdx = ui->callGraphComboBox->count();
        ui->callGraphComboBox->addItem(tr("DWARF"), QVariant::fromValue(QStringLiteral("dwarf")));
        ui->callGraphComboBox->setItemData(
            dwarfIdx,
            tr("<qt>Use the DWARF unwinder, which requires debug information to be available."
               " This can result in large data files, but is usually the most portable option to use.</qt>"),
            Qt::ToolTipRole);

        ui->callGraphComboBox->addItem(tr("Frame Pointer"), QVariant::fromValue(QStringLiteral("fp")));
        ui->callGraphComboBox->setItemData(
            ui->callGraphComboBox->count() - 1,
            tr("<qt>Use the frame pointer for stack unwinding. This only works when your code was compiled"
               " with <tt>-fno-omit-framepointer</tt>, which is usually not the case nowadays."
               " As such, only use this option when you know that you have frame pointers available."
               " If frame pointers are available, this option is the recommended unwinding option,"
               " as it results in smaller data files and has less overhead while recording.</qt>"),
            Qt::ToolTipRole);

        if (isIntel()) {
            ui->callGraphComboBox->addItem(tr("Last Branch Record"), QVariant::fromValue(QStringLiteral("lbr")));
            ui->callGraphComboBox->setItemData(
                ui->callGraphComboBox->count() - 1,
                tr("<qt>Use the Last Branch Record (LBR) for stack unwinding. This only works on newer Intel CPUs"
                   " but does not require any special compile options. The depth of the LBR is relatively limited,"
                   " which makes this option not too useful for many real-world applications.</qt>"),
                Qt::ToolTipRole);
        }

        ui->callGraphComboBox->setCurrentIndex(dwarfIdx);
    }

    {
        for (const auto size : {1024, 2048, 4096, 8192, 16384}) {
            ui->stackDumpComboBox->addItem(QString::number(size));
        }

        // select 8196 (perf default)
        ui->stackDumpComboBox->setCurrentIndex(3);
    }

    connect(ui->callGraphComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // these elements only need to hide if the user changed the callgraph method
        // the default is DWARF
        const bool isDwarf = ui->callGraphComboBox->itemData(index) == QLatin1String("dwarf");
        ui->stackDumpComboBox->setVisible(isDwarf);
        ui->stackSizeLabel->setVisible(isDwarf);
    });

    connect(m_perfRecord, &PerfRecord::recordingStarted, this,
            [this](const QString& perfBinary, const QStringList& arguments) {
                m_recordTimer.start();
                m_updateRuntimeTimer->start();
                appendOutput(QLatin1String("$ ") + perfBinary + QLatin1Char(' ') + arguments.join(QLatin1Char(' '))
                             + QLatin1Char('\n'));
                m_perfOutput->enableInput(true);
            });

    connect(m_perfRecord, &PerfRecord::recordingFinished, this, [this](const QString& fileLocation) {
        appendOutput(tr("\nrecording finished after %1").arg(Util::formatTimeString(m_recordTimer.nsecsElapsed())));
        m_resultsFile = fileLocation;
        setError({});
        recordingStopped();
        ui->viewPerfRecordResultsButton->setEnabled(true);
    });

    connect(m_perfRecord, &PerfRecord::recordingFailed, this, [this](const QString& errorMessage) {
        if (m_recordTimer.isValid()) {
            appendOutput(tr("\nrecording failed after %1: %2")
                             .arg(Util::formatTimeString(m_recordTimer.nsecsElapsed()), errorMessage));
        } else {
            appendOutput(tr("\nrecording failed: %1").arg(errorMessage));
        }
        setError(errorMessage);
        recordingStopped();
        ui->viewPerfRecordResultsButton->setEnabled(false);
    });

    connect(m_perfRecord, &PerfRecord::debuggeeCrashed, this, [this] {
        ui->applicationRecordWarningMessage->setText(tr("Debugge crashed. Results may be unusable."));
        ui->applicationRecordWarningMessage->show();
    });

    connect(m_perfRecord, &PerfRecord::recordingOutput, this, &RecordPage::appendOutput);

    m_processModel = new ProcessModel(this);
    m_processProxyModel = new ProcessFilterModel(this);
    m_processProxyModel->setSourceModel(m_processModel);
    m_processProxyModel->setDynamicSortFilter(true);

    ui->processesTableView->setModel(m_processProxyModel);
    // hide state
    ui->processesTableView->hideColumn(ProcessModel::StateColumn);
    ui->processesTableView->sortByColumn(ProcessModel::NameColumn, Qt::AscendingOrder);
    ui->processesTableView->setSortingEnabled(true);
    ui->processesTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->processesTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processesTableView->setSelectionMode(QAbstractItemView::MultiSelection);
    connect(ui->processesTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { updateStartRecordingButtonState(ui); });

    ResultsUtil::connectFilter(ui->processesFilterBox, m_processProxyModel);

    connect(m_watcher, &QFutureWatcher<ProcDataList>::finished, this, &RecordPage::updateProcessesFinished);

    if (m_perfRecord->currentUsername() == QLatin1String("root")) {
        ui->elevatePrivilegesCheckBox->setChecked(true);
        ui->elevatePrivilegesCheckBox->setEnabled(false);
    } else if (m_perfRecord->sudoUtil().isEmpty() && !KF5Auth_FOUND) {
        ui->elevatePrivilegesCheckBox->setChecked(false);
        ui->elevatePrivilegesCheckBox->setEnabled(false);
        ui->elevatePrivilegesCheckBox->setText(
            tr("(Note: Install pkexec, kdesudo, kdesu or KAuth to temporarily elevate perf privileges.)"));
    }

    connect(ui->elevatePrivilegesCheckBox, &QCheckBox::toggled, this, &RecordPage::updateOffCpuCheckboxState);

    restoreCombobox(config(), QStringLiteral("applications"), ui->applicationName->comboBox());
    restoreCombobox(config(), QStringLiteral("eventType"), ui->eventTypeBox, {ui->eventTypeBox->currentText()});
    restoreCombobox(config(), QStringLiteral("customOptions"), ui->perfParams);
    ui->elevatePrivilegesCheckBox->setChecked(config().readEntry(QStringLiteral("elevatePrivileges"), false));
    ui->offCpuCheckBox->setChecked(config().readEntry(QStringLiteral("offCpuProfiling"), false));
    ui->sampleCpuCheckBox->setChecked(config().readEntry(QStringLiteral("sampleCpu"), true));
    ui->mmapPagesSpinBox->setValue(config().readEntry(QStringLiteral("mmapPages"), 16));
    ui->mmapPagesUnitComboBox->setCurrentIndex(config().readEntry(QStringLiteral("mmapPagesUnit"), 2));
    ui->useAioCheckBox->setChecked(config().readEntry(QStringLiteral("useAio"), PerfRecord::canUseAio()));

    const auto callGraph = config().readEntry("callGraph", ui->callGraphComboBox->currentData());
    const auto callGraphIdx = ui->callGraphComboBox->findData(callGraph);
    if (callGraphIdx != -1) {
        ui->callGraphComboBox->setCurrentIndex(callGraphIdx);
    }

    updateOffCpuCheckboxState();

    m_updateRuntimeTimer->setInterval(1000);
    connect(m_updateRuntimeTimer, &QTimer::timeout, this, [this] {
        // round to the nearest second
        const auto roundedElapsed = std::round(double(m_recordTimer.nsecsElapsed()) / 1E9) * 1E9;
        ui->startRecordingButton->setText(tr("Stop Recording (%1)").arg(Util::formatTimeString(roundedElapsed, true)));
    });

    auto* stopRecordingShortcut = new QShortcut(Qt::Key_Escape, this);
    stopRecordingShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(stopRecordingShortcut, &QShortcut::activated, this,
            [this] { ui->startRecordingButton->setChecked(false); });

    auto* startRecordingShortcut = new QShortcut(Qt::CTRL + Qt::Key_Return, this);
    startRecordingShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(startRecordingShortcut, &QShortcut::activated, this, [this] {
        if (ui->viewPerfRecordResultsButton->isEnabled()) {
            ui->viewPerfRecordResultsButton->click();
        } else if (ui->startRecordingButton->isEnabled()) {
            ui->startRecordingButton->setChecked(true);
        }
    });

    if (!PerfRecord::canSampleCpu()) {
        ui->sampleCpuCheckBox->hide();
        ui->sampleCpuLabel->hide();
    }
    if (!PerfRecord::canSwitchEvents()) {
        ui->offCpuCheckBox->hide();
        ui->offCpuLabel->hide();
    }
    if (!PerfRecord::canUseAio()) {
        ui->useAioCheckBox->hide();
        ui->useAioLabel->hide();
    }
    if (!PerfRecord::canCompress()) {
        ui->compressionComboBox->hide();
        ui->compressionLabel->hide();
    } else {
        ui->compressionComboBox->addItem(tr("Disabled"), -1);
        ui->compressionComboBox->addItem(tr("Enabled (Default Level)"), 0);
        ui->compressionComboBox->addItem(tr("Level 1 (Fastest)"), 1);
        for (int i = 2; i <= 21; ++i)
            ui->compressionComboBox->addItem(tr("Level %1").arg(i), 0);
        ui->compressionComboBox->addItem(tr("Level 22 (Slowest)"), 22);

        ui->compressionComboBox->setCurrentIndex(1);
        const auto defaultLevel = ui->compressionComboBox->currentData().toInt();
        const auto level = config().readEntry(QStringLiteral("compressionLevel"), defaultLevel);
        const auto index = ui->compressionComboBox->findData(level);
        if (index != -1)
            ui->compressionComboBox->setCurrentIndex(index);
    }

    showRecordPage();

    ui->applicationRecordWarningMessage->setVisible(false);
}

RecordPage::~RecordPage() = default;

void RecordPage::showRecordPage()
{
    m_resultsFile.clear();
    setError({});
    updateRecordType();
    ui->viewPerfRecordResultsButton->setEnabled(false);
}

void RecordPage::onStartRecordingButtonClicked(bool checked)
{
    const auto recordType = selectedRecordType(ui);
    if (checked) {
        showRecordPage();
        m_watcher->cancel();
        ui->recordTypeComboBox->setEnabled(false);
        ui->launchAppBox->setEnabled(false);
        ui->attachAppBox->setEnabled(false);
        ui->perfOptionsBox->setEnabled(false);
        ui->startRecordingButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
        ui->startRecordingButton->setText(tr("Stop Recording"));
        m_perfOutput->clear();
        ui->applicationRecordWarningMessage->hide();

        QStringList perfOptions;

        const auto callGraphOption = ui->callGraphComboBox->currentData().toString();
        config().writeEntry("callGraph", callGraphOption);
        if (!callGraphOption.isEmpty()) {
            perfOptions << QStringLiteral("--call-graph");
            if (callGraphOption == QLatin1String("dwarf")) {
                perfOptions << callGraphOption + QStringLiteral(",") + ui->stackDumpComboBox->currentText();
            } else {
                perfOptions << callGraphOption;
            }
        }

        const auto eventType = ui->eventTypeBox->currentText();
        rememberCombobox(config(), QStringLiteral("eventType"), eventType, ui->eventTypeBox);
        if (!eventType.isEmpty()) {
            perfOptions << QStringLiteral("--event") << eventType;
        }

        const auto customOptions = ui->perfParams->currentText();
        rememberCombobox(config(), QStringLiteral("customOptions"), customOptions, ui->perfParams);
        perfOptions += KShell::splitArgs(customOptions);

        const bool offCpuProfilingEnabled = ui->offCpuCheckBox->isChecked();
        if (offCpuProfilingEnabled && PerfRecord::canSwitchEvents()) {
            if (eventType.isEmpty()) {
                // TODO: use clock event in VM context
                perfOptions += QStringLiteral("--event");
                perfOptions += QStringLiteral("cycles");
            }
            perfOptions += PerfRecord::offCpuProfilingOptions();
        }
        config().writeEntry(QStringLiteral("offCpuProfiling"), offCpuProfilingEnabled);

        const bool useAioEnabled = ui->useAioCheckBox->isChecked();
        if (useAioEnabled && PerfRecord::canUseAio()) {
            perfOptions += QStringLiteral("--aio");
        }
        config().writeEntry(QStringLiteral("useAio"), useAioEnabled);

        const auto compressionLevel = ui->compressionComboBox->currentData().toInt();
        if (PerfRecord::canCompress() && compressionLevel >= 0) {
            if (compressionLevel == 0)
                perfOptions += QStringLiteral("-z");
            else
                perfOptions += QStringLiteral("--compression-level=") + QString::number(compressionLevel);
        }
        config().writeEntry(QStringLiteral("compressionLevel"), compressionLevel);

        const bool elevatePrivileges = ui->elevatePrivilegesCheckBox->isChecked();

        const bool sampleCpuEnabled = ui->sampleCpuCheckBox->isChecked();
        if (sampleCpuEnabled && PerfRecord::canSampleCpu()) {
            perfOptions += QStringLiteral("--sample-cpu");
        }

        if (recordType != ProfileSystem) { // always true when recording full system
            config().writeEntry(QStringLiteral("elevatePrivileges"), elevatePrivileges);
            config().writeEntry(QStringLiteral("sampleCpu"), sampleCpuEnabled);
        }

        const int mmapPages = ui->mmapPagesSpinBox->value();
        const int mmapPagesUnit = ui->mmapPagesUnitComboBox->currentIndex();
        if (mmapPages > 0) {
            auto mmapPagesArg = QString::number(mmapPages);
            switch (mmapPagesUnit) {
            case 0:
                mmapPagesArg.append(QLatin1Char('B'));
                break;
            case 1:
                mmapPagesArg.append(QLatin1Char('K'));
                break;
            case 2:
                mmapPagesArg.append(QLatin1Char('M'));
                break;
            case 3:
                mmapPagesArg.append(QLatin1Char('G'));
                break;
            case 4:
                // pages, no unit
                break;
            default:
                qWarning() << "Unhandled mmap pages unit";
                break;
            }
            perfOptions += QStringLiteral("--mmap-pages");
            perfOptions += mmapPagesArg;
        }
        config().writeEntry(QStringLiteral("mmapPages"), mmapPages);
        config().writeEntry(QStringLiteral("mmapPagesUnit"), mmapPagesUnit);

        const auto outputFile = ui->outputFile->url().toLocalFile();

        switch (recordType) {
        case LaunchApplication: {
            const auto applicationName = KShell::tildeExpand(ui->applicationName->text());
            const auto appParameters = ui->applicationParametersBox->text();
            auto workingDir = ui->workingDirectory->text();
            if (workingDir.isEmpty()) {
                workingDir = ui->workingDirectory->placeholderText();
            }
            rememberApplication(applicationName, appParameters, workingDir, ui->applicationName->comboBox());
            m_perfRecord->record(perfOptions, outputFile, elevatePrivileges, applicationName,
                                 KShell::splitArgs(appParameters), workingDir);
            break;
        }
        case AttachToProcess: {
            QItemSelectionModel* selectionModel = ui->processesTableView->selectionModel();
            QStringList pids;

            const auto selection = selectionModel->selectedIndexes();
            for (const auto& item : selection) {
                if (item.column() == 0) {
                    pids.append(item.data(ProcessModel::PIDRole).toString());
                }
            }

            m_perfRecord->record(perfOptions, outputFile, elevatePrivileges, pids);
            break;
        }
        case ProfileSystem: {
            m_perfRecord->recordSystem(perfOptions, outputFile);
            break;
        }
        case NUM_RECORD_TYPES:
            break;
        }
    } else {
        stopRecording();
    }
}

void RecordPage::recordingStopped()
{
    m_updateRuntimeTimer->stop();
    m_recordTimer.invalidate();
    ui->startRecordingButton->setChecked(false);
    ui->startRecordingButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    ui->startRecordingButton->setText(tr("Start Recording"));

    ui->recordTypeComboBox->setEnabled(true);
    ui->launchAppBox->setEnabled(true);
    ui->attachAppBox->setEnabled(true);
    ui->perfOptionsBox->setEnabled(true);
    m_perfOutput->enableInput(false);
}

void RecordPage::stopRecording()
{
    m_perfRecord->stopRecording();
}

void RecordPage::onApplicationNameChanged(const QString& filePath)
{
    QFileInfo application(KShell::tildeExpand(filePath));
    if (!application.exists()) {
        application.setFile(QStandardPaths::findExecutable(filePath));
    }

    if (!application.exists()) {
        setError(tr("Application file cannot be found: %1").arg(filePath));
    } else if (!application.isFile()) {
        setError(tr("Application file is not valid: %1").arg(filePath));
    } else if (!application.isExecutable()) {
        setError(tr("Application file is not executable: %1").arg(filePath));
    } else {
        const auto config = applicationConfig(filePath);
        ui->workingDirectory->setText(config.readEntry("workingDir", QString()));
        ui->applicationParametersBox->setText(config.readEntry("params", QString()));
        ui->workingDirectory->setPlaceholderText(application.path());
        setError({});

        m_multiConfig->setConfig(applicationConfig(ui->applicationName->text()));
    }
    updateStartRecordingButtonState(ui);
}

void RecordPage::onWorkingDirectoryNameChanged(const QString& folderPath)
{
    QFileInfo folder(ui->workingDirectory->url().toLocalFile());

    if (!folder.exists()) {
        setError(tr("Working directory folder cannot be found: %1").arg(folderPath));
    } else if (!folder.isDir()) {
        setError(tr("Working directory folder is not valid: %1").arg(folderPath));
    } else if (!folder.isWritable()) {
        setError(tr("Working directory folder is not writable: %1").arg(folderPath));
    } else {
        setError({});
    }
    updateStartRecordingButtonState(ui);
}

void RecordPage::onViewPerfRecordResultsButtonClicked()
{
    emit openFile(m_resultsFile);
}

void RecordPage::onOutputFileNameChanged(const QString& /*filePath*/)
{
    const auto perfDataExtension = QStringLiteral(".data");

    QFileInfo file(ui->outputFile->url().toLocalFile());
    QFileInfo folder(file.absolutePath());

    if (!folder.exists()) {
        setError(tr("Output file directory folder cannot be found: %1").arg(folder.path()));
    } else if (!folder.isDir()) {
        setError(tr("Output file directory folder is not valid: %1").arg(folder.path()));
    } else if (!folder.isWritable()) {
        setError(tr("Output file directory folder is not writable: %1").arg(folder.path()));
    } else if (!file.absoluteFilePath().endsWith(perfDataExtension)) {
        setError(tr("Output file must end with %1").arg(perfDataExtension));
    } else {
        setError({});
    }
    updateStartRecordingButtonState(ui);
}

void RecordPage::onOutputFileNameSelected(const QString& filePath)
{
    const auto perfDataExtension = QStringLiteral(".data");

    if (!filePath.endsWith(perfDataExtension)) {
        ui->outputFile->setText(filePath + perfDataExtension);
    }
}

void RecordPage::onOutputFileUrlChanged(const QUrl& fileUrl)
{
    onOutputFileNameSelected(fileUrl.toLocalFile());
}

void RecordPage::updateProcesses()
{
    m_watcher->setFuture(QtConcurrent::run(processList));
}

void RecordPage::updateProcessesFinished()
{
    if (ui->startRecordingButton->isChecked()) {
        return;
    }

    m_processModel->mergeProcesses(m_watcher->result());

    if (selectedRecordType(ui) == AttachToProcess) {
        // only update the state when we show the attach app page
        updateStartRecordingButtonState(ui);
        QTimer::singleShot(1000, this, &RecordPage::updateProcesses);
    }
}

void RecordPage::appendOutput(const QString& text)
{
    m_perfOutput->addOutput(text);
}

void RecordPage::setError(const QString& message)
{
    ui->applicationRecordErrorMessage->setText(message);
    ui->applicationRecordErrorMessage->setVisible(!message.isEmpty());
}

void RecordPage::updateRecordType()
{
    setError({});

    const auto recordType = selectedRecordType(ui);
    ui->launchAppBox->setVisible(recordType == LaunchApplication);
    ui->attachAppBox->setVisible(recordType == AttachToProcess);

    m_perfOutput->setInputVisible(recordType == LaunchApplication);
    m_perfOutput->clear();
    ui->elevatePrivilegesCheckBox->setEnabled(recordType != ProfileSystem);
    ui->sampleCpuCheckBox->setEnabled(recordType != ProfileSystem && PerfRecord::canSampleCpu());
    if (recordType == ProfileSystem) {
        ui->elevatePrivilegesCheckBox->setChecked(true);
        ui->sampleCpuCheckBox->setChecked(true && PerfRecord::canSampleCpu());
    }

    if (recordType == AttachToProcess) {
        updateProcesses();
    }

    updateStartRecordingButtonState(ui);
}

void RecordPage::updateOffCpuCheckboxState()
{
    const bool enableOffCpuProfiling =
        (ui->elevatePrivilegesCheckBox->isChecked() || PerfRecord::canProfileOffCpu()) && PerfRecord::canSwitchEvents();

    if (enableOffCpuProfiling == ui->offCpuCheckBox->isEnabled()) {
        return;
    }

    ui->offCpuCheckBox->setEnabled(enableOffCpuProfiling);

    // prevent user confusion: don't show the value as checked when the checkbox is disabled
    if (!enableOffCpuProfiling) {
        // remember the current value
        config().writeEntry(QStringLiteral("offCpuProfiling"), ui->offCpuCheckBox->isChecked());
        ui->offCpuCheckBox->setChecked(false);
    } else {
        ui->offCpuCheckBox->setChecked(config().readEntry(QStringLiteral("offCpuProfiling"), false));
    }
}
