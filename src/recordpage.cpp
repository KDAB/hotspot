/*
  recordpage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "recordpage.h"
#include "ui_recordpage.h"

#include "processfiltermodel.h"
#include "processmodel.h"
#include "util.h"

#include <QDebug>
#include <QKeyEvent>
#include <QListView>
#include <QShortcut>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <KColumnResizer>
#include <KComboBox>
#include <KConfigGroup>
#include <KFilterProxySearchLine>
#include <KRecursiveFilterProxyModel>
#include <KSharedConfig>
#include <KShell>
#include <KUrlCompletion>
#include <KUrlRequester>
#include <Solid/Device>
#include <Solid/Processor>
#include <kio_version.h>

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
    return config().group(QLatin1String("Application ") + application);
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
    ui->setupUi(this);

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
    ui->applicationRecordErrorMessage->setCloseButtonVisible(false);
    ui->applicationRecordErrorMessage->setWordWrap(true);
    ui->applicationRecordErrorMessage->setMessageType(KMessageWidget::Error);
    ui->outputFile->setText(QDir::currentPath() + QDir::separator() + QStringLiteral("perf.data"));
    ui->outputFile->setMode(KFile::File | KFile::LocalOnly);
    ui->eventTypeBox->lineEdit()->setPlaceholderText(tr("perf defaults (usually cycles:Pu)"));

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
    updateRecordType();

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

    connect(m_perfRecord, &PerfRecord::recordingStarted, this,
            [this](const QString& perfBinary, const QStringList& arguments) {
                m_recordTimer.start();
                m_updateRuntimeTimer->start();
                appendOutput(QLatin1String("$ ") + perfBinary + QLatin1Char(' ') + arguments.join(QLatin1Char(' '))
                             + QLatin1Char('\n'));
                ui->perfInputEdit->setEnabled(true);
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

    connect(m_perfRecord, &PerfRecord::recordingOutput, this, &RecordPage::appendOutput);

    connect(ui->perfInputEdit, &QLineEdit::returnPressed, this, [this]() {
        m_perfRecord->sendInput(ui->perfInputEdit->text().toUtf8());
        m_perfRecord->sendInput(QByteArrayLiteral("\n"));
        ui->perfInputEdit->clear();
    });

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

    ui->processesFilterBox->setProxy(m_processProxyModel);

    connect(m_watcher, &QFutureWatcher<ProcDataList>::finished, this, &RecordPage::updateProcessesFinished);

    if (m_perfRecord->currentUsername() == QLatin1String("root")) {
        ui->elevatePrivilegesCheckBox->setChecked(true);
        ui->elevatePrivilegesCheckBox->setEnabled(false);
    } else if (m_perfRecord->sudoUtil().isEmpty()) {
        ui->elevatePrivilegesCheckBox->setChecked(false);
        ui->elevatePrivilegesCheckBox->setEnabled(false);
        ui->elevatePrivilegesCheckBox->setText(
            tr("(Note: Install kdesudo or kdesu to temporarily elevate perf privileges.)"));
    }

    connect(ui->elevatePrivilegesCheckBox, &QCheckBox::toggled, this, &RecordPage::updateOffCpuCheckboxState);

    restoreCombobox(config(), QStringLiteral("applications"), ui->applicationName->comboBox());
    restoreCombobox(config(), QStringLiteral("eventType"), ui->eventTypeBox, {ui->eventTypeBox->currentText()});
    restoreCombobox(config(), QStringLiteral("customOptions"), ui->perfParams);
    ui->elevatePrivilegesCheckBox->setChecked(config().readEntry(QStringLiteral("elevatePrivileges"), false));
    ui->offCpuCheckBox->setChecked(config().readEntry(QStringLiteral("offCpuProfiling"), false));
    ui->sampleCpuCheckBox->setChecked(config().readEntry(QStringLiteral("sampleCpu"), true));
    ui->mmapPagesSpinBox->setValue(config().readEntry(QStringLiteral("mmapPages"), 0));
    ui->mmapPagesUnitComboBox->setCurrentIndex(config().readEntry(QStringLiteral("mmapPagesUnit"), 2));
    ui->useAioCheckBox->setChecked(config().readEntry(QStringLiteral("useAio"), PerfRecord::canUseAio()));

    const auto callGraph = config().readEntry("callGraph", ui->callGraphComboBox->currentData());
    const auto callGraphIdx = ui->callGraphComboBox->findData(callGraph);
    if (callGraphIdx != -1) {
        ui->callGraphComboBox->setCurrentIndex(callGraphIdx);
    }

    updateOffCpuCheckboxState();

    showRecordPage();

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

    auto* startRecordingShortcut = new QShortcut(Qt::Key_Return, this);
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
        ui->perfResultsTextEdit->clear();

        QStringList perfOptions;

        const auto callGraphOption = ui->callGraphComboBox->currentData().toString();
        config().writeEntry("callGraph", callGraphOption);
        if (!callGraphOption.isEmpty()) {
            perfOptions << QStringLiteral("--call-graph") << callGraphOption;
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

            for (const auto& item : selectionModel->selectedIndexes()) {
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
    ui->perfInputEdit->setEnabled(false);
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
    QTextCursor cursor(ui->perfResultsTextEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
}

void RecordPage::setError(const QString& message)
{
    ui->applicationRecordErrorMessage->setText(message);
    ui->applicationRecordErrorMessage->setVisible(!message.isEmpty());
}

void RecordPage::updateRecordType()
{
    const auto recordType = selectedRecordType(ui);
    ui->launchAppBox->setVisible(recordType == LaunchApplication);
    ui->attachAppBox->setVisible(recordType == AttachToProcess);
    ui->perfInputEdit->setVisible(recordType == LaunchApplication);
    ui->perfInputEdit->clear();
    ui->perfResultsTextEdit->clear();
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
