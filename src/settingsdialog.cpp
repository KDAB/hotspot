/*
    SPDX-FileCopyrightText: Petr Lyapidevskiy <p.lyapidevskiy@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "settingsdialog.h"

#include "ui_callgraphsettingspage.h"
#include "ui_debuginfodpage.h"
#include "ui_disassemblysettingspage.h"
#include "ui_flamegraphsettingspage.h"
#include "ui_perfsettingspage.h"
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

#include <hotspot-config.h>

namespace {
KConfigGroup config()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("PerfPaths"));
}

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
    , m_configs(std::make_unique<MultiConfigWidget>(this))
{
    addPerfSettingsPage();
    addPathSettingsPage();
    addFlamegraphPage();
    addDebuginfodPage();
#if KGraphViewerPart_FOUND
    addCallgraphPage();
#endif
    addSourcePathPage();

    setAttribute(Qt::WA_DeleteOnClose);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::initSettings()
{
    const auto configName = Settings::instance()->lastUsedEnvironment();
    if (!configName.isEmpty()) {
        m_configs->selectConfig(configName);
    }
}

QString SettingsDialog::sysroot() const
{
    return unwindPage->lineEditSysroot->text();
}

QString SettingsDialog::appPath() const
{
    return unwindPage->lineEditApplicationPath->text();
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
    return unwindPage->lineEditKallsyms->text();
}

QString SettingsDialog::arch() const
{
    const auto sArch = unwindPage->comboBoxArchitecture->currentText();
    return (sArch == QLatin1String("auto-detect")) ? QString() : sArch;
}

QString SettingsDialog::objdump() const
{
    return unwindPage->lineEditObjdump->text();
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

    auto lastExtraLibsWidget = setupMultiPath(unwindPage->extraLibraryPaths, unwindPage->extraLibraryPathsLabel,
                                              unwindPage->lineEditApplicationPath);
    setupMultiPath(unwindPage->debugPaths, unwindPage->debugPathsLabel, lastExtraLibsWidget);

    auto* label = new QLabel(this);
    label->setText(tr("Config:"));

    auto loadFromSettings = [this]() {
        auto settings = Settings::instance();
        auto fromPathString = [](KEditListWidget* listWidget, const QString& string) {
            listWidget->setItems(string.split(QLatin1Char(':'), Qt::SkipEmptyParts));
        };
        fromPathString(unwindPage->extraLibraryPaths, settings->extraLibPaths());
        fromPathString(unwindPage->debugPaths, settings->debugPaths());

        unwindPage->lineEditSysroot->setText(settings->sysroot());
        unwindPage->lineEditApplicationPath->setText(settings->appPath());
        unwindPage->lineEditKallsyms->setText(settings->kallsyms());
        unwindPage->lineEditObjdump->setText(settings->objdump());

        const auto arch = settings->arch();
        int itemIndex = 0;
        if (!arch.isEmpty()) {
            itemIndex = unwindPage->comboBoxArchitecture->findText(arch);
            if (itemIndex == -1) {
                itemIndex = unwindPage->comboBoxArchitecture->count();
                unwindPage->comboBoxArchitecture->addItem(arch);
            }
        }
        unwindPage->comboBoxArchitecture->setCurrentIndex(itemIndex);
    };

    loadFromSettings();

    auto saveFunction = [this](KConfigGroup group) {
        group.writeEntry("sysroot", sysroot());
        group.writeEntry("appPath", appPath());
        group.writeEntry("extraLibPaths", extraLibPaths());
        group.writeEntry("debugPaths", debugPaths());
        group.writeEntry("kallsyms", kallsyms());
        group.writeEntry("arch", arch());
        group.writeEntry("objdump", objdump());
    };

    auto restoreFunction = [this, loadFromSettings](const KConfigGroup& group) {
        ::config().writeEntry("lastUsed", m_configs->currentConfig());
        auto settings = Settings::instance();

        const auto sysroot = group.readEntry("sysroot");
        settings->setSysroot(sysroot);
        const auto appPath = group.readEntry("appPath");
        settings->setAppPath(appPath);
        const auto extraLibPaths = group.readEntry("extraLibPaths");
        settings->setExtraLibPaths(extraLibPaths);
        const auto debugPaths = group.readEntry("debugPaths");
        settings->setDebugPaths(debugPaths);
        const auto kallsyms = group.readEntry("kallsyms");
        settings->setKallsyms(kallsyms);
        const auto arch = group.readEntry("arch");
        settings->setArch(arch);
        const auto objdump = group.readEntry("objdump");
        settings->setObjdump(objdump);

        loadFromSettings();
    };

    m_configs->setConfig(config());
    m_configs->restoreCurrent();

    connect(m_configs.get(), &MultiConfigWidget::saveConfig, this, saveFunction);
    connect(m_configs.get(), &MultiConfigWidget::restoreConfig, this, restoreFunction);

    unwindPage->formLayout->insertRow(0, label, m_configs.get());

    connect(this, &KPageDialog::accepted, this, [this] { m_configs->updateCurrentConfig(); });

    for (auto field : {unwindPage->lineEditSysroot, unwindPage->lineEditApplicationPath, unwindPage->lineEditKallsyms,
                       unwindPage->lineEditObjdump}) {
        connect(field, &KUrlRequester::textEdited, m_configs.get(), &MultiConfigWidget::updateCurrentConfig);
        connect(field, &KUrlRequester::urlSelected, m_configs.get(), &MultiConfigWidget::updateCurrentConfig);
    }

    connect(unwindPage->comboBoxArchitecture, QOverload<int>::of(&QComboBox::currentIndexChanged), m_configs.get(),
            &MultiConfigWidget::updateCurrentConfig);

    connect(unwindPage->debugPaths, &KEditListWidget::changed, m_configs.get(),
            &MultiConfigWidget::updateCurrentConfig);
    connect(unwindPage->extraLibraryPaths, &KEditListWidget::changed, m_configs.get(),
            &MultiConfigWidget::updateCurrentConfig);
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
    disassemblyPage->showHexdump->setChecked(settings->showHexdump());
    disassemblyPage->tabWidth->setValue(settings->tabWidth());

    connect(buttonBox(), &QDialogButtonBox::accepted, this, [this, colon, settings] {
        settings->setSourceCodePaths(disassemblyPage->sourcePaths->items().join(colon));
        settings->setShowBranches(disassemblyPage->showBranches->isChecked());
        settings->setShowHexdump(disassemblyPage->showHexdump->isChecked());
        settings->setTabWidth(disassemblyPage->tabWidth->value());
    });
}
