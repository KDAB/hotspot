/*
    SPDX-FileCopyrightText: Petr Lyapidevskiy <p.lyapidevskiy@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "settingsdialog.h"

#include "ui_callgraphsettingspage.h"
#include "ui_debuginfodpage.h"
#include "ui_disassemblysettingspage.h"
#include "ui_flamegraphsettingspage.h"
#include "ui_perfsettingspage.h"
#include "ui_sourcepathsettings.h"
#include "ui_sshsettingspage.h"
#include "ui_unwindsettingspage.h"

#include "multiconfigwidget.h"
#include "settings.h"

#include <KComboBox>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KUrlRequester>

#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QProcess>

#include <hotspot-config.h>

namespace {
QPushButton* setupMultiPath(KEditListWidget* listWidget, QLabel* buddy, QWidget* previous)
{
    auto editor = new KUrlRequester(listWidget);
    editor->setPlaceholderText(QObject::tr("auto-detect"));
    editor->setMode(KFile::LocalOnly | KFile::Directory | KFile::ExistingOnly);
    buddy->setBuddy(editor);
    listWidget->setCustomEditor(editor->customEditor());
    QWidget::setTabOrder(previous, editor);
    QWidget::setTabOrder(editor, listWidget->listView());
    QWidget::setTabOrder(listWidget->listView(), listWidget->addButton());
    QWidget::setTabOrder(listWidget->addButton(), listWidget->removeButton());
    QWidget::setTabOrder(listWidget->removeButton(), listWidget->upButton());
    QWidget::setTabOrder(listWidget->upButton(), listWidget->downButton());
    return listWidget->downButton();
}

QIcon icon()
{
    static const auto icon = QIcon::fromTheme(QStringLiteral("preferences-system-windows-behavior"));
    return icon;
}
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : KPageDialog(parent)
    , perfPage(new Ui::PerfSettingsPage)
    , unwindPage(new Ui::UnwindSettingsPage)
    , flamegraphPage(new Ui::FlamegraphSettingsPage)
    , debuginfodPage(new Ui::DebuginfodPage)
    , disassemblyPage(new Ui::DisassemblySettingsPage)
#if KGraphViewerPart_FOUND
    , callgraphPage(new Ui::CallgraphSettingsPage)
#endif
    , sshSettingsPage(new Ui::SSHSettingsPage)
{
    addPerfSettingsPage();
    addPathSettingsPage();
    addFlamegraphPage();
    addDebuginfodPage();
#if KGraphViewerPart_FOUND
    addCallgraphPage();
#endif
    addSourcePathPage();
    addSSHPage();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::initSettings()
{
    const auto configName = Settings::instance()->lastUsedEnvironment();
    if (!configName.isEmpty()) {
        unwindPage->multiConfig->loadConfig(configName);
    }
}

QString SettingsDialog::sysroot() const
{
    return unwindPage->sysroot->text();
}

QString SettingsDialog::appPath() const
{
    return unwindPage->appPath->text();
}

QString SettingsDialog::extraLibPaths() const
{
    return unwindPage->extraLibraryPaths->items().join(QLatin1Char(':'));
}

QString SettingsDialog::debugPaths() const
{
    return unwindPage->debugPaths->items().join(QLatin1Char(':'));
}

QString SettingsDialog::kallsyms() const
{
    return unwindPage->kallsyms->text();
}

QString SettingsDialog::arch() const
{
    const auto sArch = unwindPage->arch->currentText();
    return (sArch == QLatin1String("auto-detect")) ? QString() : sArch;
}

QString SettingsDialog::objdump() const
{
    return unwindPage->objdump->text();
}

QString SettingsDialog::perfMapPath() const
{
    return unwindPage->lineEditPerfMapPath->text();
}

void SettingsDialog::addPerfSettingsPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Perf"));
    item->setIcon(icon());

    perfPage->setupUi(page);

    connect(this, &KPageDialog::accepted, this, [this]() {
        auto settings = Settings::instance();
        settings->setPerfPath(perfPage->perfPathEdit->url().toLocalFile());
    });

    perfPage->perfPathEdit->setUrl(QUrl::fromLocalFile(Settings::instance()->perfPath()));
}

void SettingsDialog::addPathSettingsPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Unwinding"));
    item->setHeader(tr("Unwind Options"));
    item->setIcon(icon());
    unwindPage->setupUi(page);

    unwindPage->multiConfig->setConfigGroup(KSharedConfig::openConfig()->group("PerfPaths"));
    unwindPage->multiConfig->setChildWidget(unwindPage->widget,
                                            {unwindPage->sysroot, unwindPage->appPath, unwindPage->extraLibraryPaths,
                                             unwindPage->debugPaths, unwindPage->kallsyms, unwindPage->arch,
                                             unwindPage->objdump});

    auto lastExtraLibsWidget =
        setupMultiPath(unwindPage->extraLibraryPaths, unwindPage->extraLibraryPathsLabel, unwindPage->appPath);
    setupMultiPath(unwindPage->debugPaths, unwindPage->debugPathsLabel, lastExtraLibsWidget);

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this] {
        unwindPage->multiConfig->saveCurrentConfig();
        Settings::instance()->setLastUsedEnvironment(unwindPage->multiConfig->currentConfig());
    });
}

void SettingsDialog::keyPressEvent(QKeyEvent* event)
{
    // disable the return -> accept policy since it prevents the user from confirming name changes in the combobox
    // you can still press CTRL + Enter to close the dialog
    if (event->modifiers() != Qt::Key_Control && (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
        return;
    }
    QDialog::keyPressEvent(event);
}

void SettingsDialog::addFlamegraphPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Flamegraph"));
    item->setHeader(tr("Flamegraph Options"));
    item->setIcon(icon());

    flamegraphPage->setupUi(page);

    auto lastUserPath = setupMultiPath(flamegraphPage->userPaths, flamegraphPage->userPathsLabel, nullptr);
    setupMultiPath(flamegraphPage->systemPaths, flamegraphPage->systemPathsLabel, lastUserPath);

    flamegraphPage->userPaths->insertStringList(Settings::instance()->userPaths());
    flamegraphPage->systemPaths->insertStringList(Settings::instance()->systemPaths());

    connect(Settings::instance(), &Settings::pathsChanged, this, [this] {
        flamegraphPage->userPaths->clear();
        flamegraphPage->systemPaths->clear();
        flamegraphPage->userPaths->insertStringList(Settings::instance()->userPaths());
        flamegraphPage->systemPaths->insertStringList(Settings::instance()->systemPaths());
    });

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this] {
        Settings::instance()->setPaths(flamegraphPage->userPaths->items(), flamegraphPage->systemPaths->items());
    });
}

void SettingsDialog::addDebuginfodPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("debuginfod"));
    item->setHeader(tr("debuginfod Urls"));
    item->setIcon(icon());

    debuginfodPage->setupUi(page);

    debuginfodPage->urls->insertStringList(Settings::instance()->debuginfodUrls());

    connect(Settings::instance(), &Settings::debuginfodUrlsChanged, this, [this] {
        debuginfodPage->urls->clear();
        debuginfodPage->urls->insertStringList(Settings::instance()->debuginfodUrls());
    });

    connect(buttonBox(), &QDialogButtonBox::accepted, this,
            [this] { Settings::instance()->setDebuginfodUrls(debuginfodPage->urls->items()); });
}

void SettingsDialog::addCallgraphPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Callgraph"));
    item->setHeader(tr("Callgraph Settings"));
    item->setIcon(icon());

    callgraphPage->setupUi(page);

    connect(Settings::instance(), &Settings::callgraphChanged, this, [this] {
        auto settings = Settings::instance();
        callgraphPage->parentSpinBox->setValue(settings->callgraphParentDepth());
        callgraphPage->childSpinBox->setValue(settings->callgraphChildDepth());
        callgraphPage->currentFunctionColor->setColor(settings->callgraphActiveColor());
        callgraphPage->functionColor->setColor(settings->callgraphColor());
    });

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this] {
        auto settings = Settings::instance();
        settings->setCallgraphParentDepth(callgraphPage->parentSpinBox->value());
        settings->setCallgraphChildDepth(callgraphPage->childSpinBox->value());
        settings->setCallgraphColors(callgraphPage->currentFunctionColor->color().name(),
                                     callgraphPage->functionColor->color().name());
    });
}

void SettingsDialog::addSourcePathPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Disassembly"));
    item->setHeader(tr("Disassembly Settings"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-behavior")));

    disassemblyPage->setupUi(page);

    auto settings = Settings::instance();

    const auto colon = QLatin1Char(':');
    connect(settings, &Settings::sourceCodePathsChanged, this,
            [this, colon](const QString& paths) { disassemblyPage->sourcePaths->setItems(paths.split(colon)); });

    setupMultiPath(disassemblyPage->sourcePaths, disassemblyPage->label, buttonBox()->button(QDialogButtonBox::Ok));

    disassemblyPage->showBranches->setChecked(settings->showBranches());

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this, colon, settings] {
        settings->setSourceCodePaths(disassemblyPage->sourcePaths->items().join(colon));
        settings->setShowBranches(disassemblyPage->showBranches->isChecked());
    });
}

void SettingsDialog::addSSHPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("SSH Settings"));
    item->setHeader(tr("SSH Settings Page"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-behavior")));
    sshSettingsPage->setupUi(page);
    sshSettingsPage->messageWidget->hide();
    sshSettingsPage->errorWidget->hide();

    bool ksshaskpassFound = !QStandardPaths()::findExe(QStringLiteral("ksshaskpass")).isEmpty();
    if (!qEnvironmentVariableIsSet("SSH_ASKPASS") && !ksshaskpassFound) {
        sshSettingsPage->errorWidget->setText(
            tr("<tt>SSH_ASKPASS<tt> is not set please install <tt>ksshaskpass<tt> or set <tt>SSH_ASKPASS<tt>"));
        sshSettingsPage->errorWidget->show();
    }

    auto configGroup = KSharedConfig::openConfig()->group("SSH");
    sshSettingsPage->deviceConfig->setChildWidget(
        sshSettingsPage->deviceSettings,
        {sshSettingsPage->username, sshSettingsPage->hostname, sshSettingsPage->options});
    sshSettingsPage->deviceConfig->setConfigGroup(configGroup);

    connect(sshSettingsPage->copySshKeyButton, &QPushButton::pressed, this, [this] {
        auto* copyKey = new QProcess(this);

        auto path = sshSettingsPage->sshCopyIdPath->text();
        if (path.isEmpty()) {
            path = QStandardPaths::findExecutable(QStringLiteral("ssh-copy-id"));
        }
        if (path.isEmpty()) {
            sshSettingsPage->messageWidget->setText(tr("Could not find ssh-copy-id"));
            sshSettingsPage->messageWidget->show();
            return;
        }

        copyKey->setProgram(path);

        QStringList arguments = {};
        auto options = sshSettingsPage->options->text();
        if (!options.isEmpty()) {
            arguments.append(options.split(QLatin1Char(' ')));
        }
        arguments.append(
            QStringLiteral("%1@%2").arg(sshSettingsPage->username->text(), sshSettingsPage->hostname->text()));
        copyKey->setArguments(arguments);

        connect(copyKey, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this, copyKey](int code, QProcess::ExitStatus) {
                    if (code == 0) {
                        sshSettingsPage->messageWidget->setText(QStringLiteral("Copy key successfully"));
                        sshSettingsPage->messageWidget->show();
                    } else {
                        sshSettingsPage->errorWidget->setText(QStringLiteral("Failed to copy key"));
                        sshSettingsPage->errorWidget->show();
                    }
                    copyKey->deleteLater();
                });
        copyKey->start();
    });

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this, configGroup] {
        sshSettingsPage->deviceConfig->saveCurrentConfig();

        auto settings = Settings::instance();
        settings->setDevices(configGroup.groupList());
        settings->setSSHPath(sshSettingsPage->sshBinary->text());
        settings->setSSHCopyKeyPath(sshSettingsPage->sshCopyIdPath->text());
    });
}
