/*
  recordpage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QDebug>
#include <QStandardPaths>

#include <KUrlRequester>
#include <Solid/Device>
#include <Solid/Processor>
#include <KShell>

#include "recordpage.h"
#include "ui_recordpage.h"
#include "perfrecord.h"

namespace {
bool isIntel()
{
    const auto list = Solid::Device::listFromType(Solid::DeviceInterface::Processor, QString());
    Solid::Device device = list[0];
    if(list.empty() || !device.is<Solid::Processor>()) {
        return false;
    }
    const auto *processor= device.as<Solid::Processor>();
    const auto instructionSets = processor->instructionSets();

    return instructionSets.testFlag(Solid::Processor::IntelMmx) ||
           instructionSets.testFlag(Solid::Processor::IntelSse) ||
           instructionSets.testFlag(Solid::Processor::IntelSse2) ||
           instructionSets.testFlag(Solid::Processor::IntelSse3) ||
           instructionSets.testFlag(Solid::Processor::IntelSsse3) ||
           instructionSets.testFlag(Solid::Processor::IntelSse4) ||
           instructionSets.testFlag(Solid::Processor::IntelSse41) ||
           instructionSets.testFlag(Solid::Processor::IntelSse42);
}
}

RecordPage::RecordPage(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::RecordPage),
      m_perfRecord(new PerfRecord(this))
{
    ui->setupUi(this);

    ui->applicationName->setMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
    // we are only interested in executable files, so set the mime type filter accordingly
    // note that exe's build with PIE are actually "shared libs"...
    ui->applicationName->setMimeTypeFilters({QStringLiteral("application/x-executable"),
                                             QStringLiteral("application/x-sharedlib")});
    ui->workingDirectory->setMode(KFile::Directory | KFile::LocalOnly);
    ui->workingDirectory->setText(QDir::currentPath());
    ui->applicationRecordErrorMessage->setCloseButtonVisible(false);
    ui->applicationRecordErrorMessage->setWordWrap(true);
    ui->applicationRecordErrorMessage->setMessageType(KMessageWidget::Error);
    ui->outputFile->setText(QDir::currentPath() + QDir::separator() + QStringLiteral("perf.data"));
    ui->outputFile->setMode(KFile::File | KFile::LocalOnly);

    connect(ui->homeButton, &QPushButton::clicked, this, &RecordPage::homeButtonClicked);
    connect(ui->applicationName, &KUrlRequester::textChanged, this, &RecordPage::onApplicationNameChanged);
    connect(ui->startRecordingButton, &QPushButton::toggled, this, &RecordPage::onStartRecordingButtonClicked);
    connect(ui->workingDirectory, &KUrlRequester::textChanged, this, &RecordPage::onWorkingDirectoryNameChanged);
    connect(ui->viewPerfRecordResultsButton, &QPushButton::clicked, this, &RecordPage::onViewPerfRecordResultsButtonClicked);
    connect(ui->outputFile, &KUrlRequester::textChanged, this, &RecordPage::onOutputFileNameChanged);
    connect(ui->outputFile, static_cast<void (KUrlRequester::*)(const QString&)>(&KUrlRequester::returnPressed),
            this, &RecordPage::onOutputFileNameSelected);
    connect(ui->outputFile, &KUrlRequester::urlSelected, this, &RecordPage::onOutputFileUrlChanged);

    ui->recordTypeComboBox->addItem(tr("Launch Application"), QVariant::fromValue(LaunchApplication));
    ui->callGraphComboBox->addItem(tr("None"), QVariant::fromValue(QString()));
    ui->callGraphComboBox->addItem(tr("DWARF"), QVariant::fromValue(QStringLiteral("dwarf")));
    ui->callGraphComboBox->addItem(tr("Frame Pointer"), QVariant::fromValue(QStringLiteral("fp")));
    if (isIntel()) {
        ui->callGraphComboBox->addItem(tr("Last Branch Record"), QVariant::fromValue(QStringLiteral("lbr")));
    }
    ui->callGraphComboBox->setCurrentIndex(1);

    connect(m_perfRecord, &PerfRecord::recordingFinished,
            this, [this] (const QString& fileLocation) {
                ui->startRecordingButton->setChecked(false);
                ui->applicationRecordErrorMessage->hide();
                m_resultsFile = fileLocation;
                ui->viewPerfRecordResultsButton->setEnabled(true);
    });

    connect(m_perfRecord, &PerfRecord::recordingFailed,
            this, [this] (const QString& errorMessage) {
                ui->startRecordingButton->setChecked(false);
                ui->applicationRecordErrorMessage->setText(errorMessage);
                ui->applicationRecordErrorMessage->show();
                ui->viewPerfRecordResultsButton->setEnabled(false);

    });

    connect(m_perfRecord, &PerfRecord::recordingOutput,
            this, [this] (const QString& outputMessage) {
                ui->perfResultsTextEdit->insertPlainText(outputMessage);
                ui->perfResultsTextEdit->show();
                ui->perfResultsLabel->show();
                ui->perfResultsTextEdit->moveCursor(QTextCursor::End);
    });

    showRecordPage();
}

RecordPage::~RecordPage() = default;

void RecordPage::showRecordPage()
{
    setWindowTitle(tr("Hotspot - Record"));
    m_resultsFile.clear();
    ui->applicationRecordErrorMessage->hide();
    ui->perfResultsTextEdit->clear();
    ui->perfResultsTextEdit->hide();
    ui->perfResultsLabel->hide();
}

void RecordPage::onStartRecordingButtonClicked(bool checked)
{
    if (checked) {
        showRecordPage();
        ui->startRecordingButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
        ui->startRecordingButton->setText(tr("Stop Recording"));

        QStringList perfOptions;
        const auto callGraphOption = ui->callGraphComboBox->currentData().toString();

        if (!callGraphOption.isEmpty()) {
            perfOptions << QStringLiteral("--call-graph") << callGraphOption;
        }

        if (!ui->eventTypeBox->text().isEmpty()) {
            perfOptions << QStringLiteral("--event") << ui->eventTypeBox->text();
        }

        m_perfRecord->record(perfOptions, ui->outputFile->url().toLocalFile(),
                             ui->applicationName->url().toLocalFile(),
                             KShell::splitArgs(ui->applicationParametersBox->text()),
                             ui->workingDirectory->url().toLocalFile());
    } else {
        ui->startRecordingButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
        ui->startRecordingButton->setText(tr("Start Recording"));
        m_perfRecord->stopRecording();
    }
}

void RecordPage::onApplicationNameChanged(const QString& filePath)
{
    QFileInfo application(ui->applicationName->url().toLocalFile());

    if (!application.exists()) {
        application.setFile(QStandardPaths::findExecutable(filePath));
    }

    if (!application.exists()) {
        ui->applicationRecordErrorMessage->setText(tr("Application file cannot be found: %1").arg(filePath));
    } else if (!application.isFile()) {
        ui->applicationRecordErrorMessage->setText(tr("Application file is not valid: %1").arg(filePath));
    } else if (!application.isExecutable()) {
        ui->applicationRecordErrorMessage->setText(tr("Application file is not executable: %1").arg(filePath));
    } else {
        if (ui->workingDirectory->text().isEmpty()) {
            ui->workingDirectory->setPlaceholderText(application.path());
        }
        ui->applicationRecordErrorMessage->hide();
        return;
    }
    ui->applicationRecordErrorMessage->show();
}

void RecordPage::onWorkingDirectoryNameChanged(const QString& folderPath)
{
    QFileInfo folder(ui->workingDirectory->url().toLocalFile());

    if (!folder.exists()) {
        ui->applicationRecordErrorMessage->setText(tr("Working directory folder cannot be found: %1").arg(folderPath));
    } else if (!folder.isDir()) {
        ui->applicationRecordErrorMessage->setText(tr("Working directory folder is not valid: %1").arg(folderPath));
    } else if (!folder.isWritable()) {
        ui->applicationRecordErrorMessage->setText(tr("Working directory folder is not writable: %1").arg(folderPath));
    } else {
        ui->applicationRecordErrorMessage->hide();
        return;
    }
    ui->applicationRecordErrorMessage->show();
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
        ui->applicationRecordErrorMessage->setText(tr("Output file directory folder cannot be found: %1").arg(folder.path()));
    } else if (!folder.isDir()) {
        ui->applicationRecordErrorMessage->setText(tr("Output file directory folder is not valid: %1").arg(folder.path()));
    } else if (!folder.isWritable()) {
        ui->applicationRecordErrorMessage->setText(tr("Output file directory folder is not writable: %1").arg(folder.path()));
    } else if (!file.absoluteFilePath().endsWith(perfDataExtension)) {
        ui->applicationRecordErrorMessage->setText(tr("Output file must end with %1").arg(perfDataExtension));
    } else {
        ui->applicationRecordErrorMessage->hide();
        return;
    }
    ui->applicationRecordErrorMessage->show();
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
