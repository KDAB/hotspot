/*
  settingsdialog.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Petr Lyapidevskiy <p.lyapidevskiy@nips.ru>

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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <KComboBox>
#include <KUrlRequester>
#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <QLineEdit>
#include <QListView>

namespace {
KConfigGroup config()
{
    return KSharedConfig::openConfig()->group("PerfPaths");
}
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : KPageDialog(parent)
    , unwindPage(new Ui::SettingsDialog)
{
    addPathSettingsPage();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::initSettings(const QString& configName)
{
    const int index = unwindPage->configComboBox->findText(configName);
    if (index > -1) {
        unwindPage->configComboBox->setCurrentIndex(index);
    }
    applyCurrentConfig();
}

void SettingsDialog::initSettings(const QString &sysroot, const QString &appPath, const QString &extraLibPaths,
                                  const QString &debugPaths, const QString &kallsyms, const QString &arch,
                                  const QString &objdump)
{
    auto fromPathString = [](KEditListWidget* listWidget, const QString &string)
    {
        listWidget->setItems(string.split(QLatin1Char(':'), Qt::SkipEmptyParts));
    };
    fromPathString(unwindPage->extraLibraryPaths, extraLibPaths);
    fromPathString(unwindPage->debugPaths, debugPaths);

    unwindPage->lineEditSysroot->setText(sysroot);
    unwindPage->lineEditApplicationPath->setText(appPath);
    unwindPage->lineEditKallsyms->setText(kallsyms);
    unwindPage->lineEditObjdump->setText(objdump);

    int itemIndex = 0;
    if (!arch.isEmpty()) {
        itemIndex = unwindPage->comboBoxArchitecture->findText(arch);
        if (itemIndex == -1) {
            itemIndex = unwindPage->comboBoxArchitecture->count();
            unwindPage->comboBoxArchitecture->addItem(arch);
        }
    }
    unwindPage->comboBoxArchitecture->setCurrentIndex(itemIndex);
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
    QString sArch = unwindPage->comboBoxArchitecture->currentText();
    return (sArch == QLatin1String("auto-detect")) ? QString() : sArch;
}

QString SettingsDialog::objdump() const
{
    return unwindPage->lineEditObjdump->text();
}

void SettingsDialog::addPathSettingsPage()
{
    auto page = new QWidget(this);
    auto item = addPage(page, tr("Unwinding"));
    item->setHeader(tr("Unwind Options"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-behavior")));

    unwindPage->setupUi(page);

    auto setupMultiPath = [](KEditListWidget* listWidget, QLabel* buddy, QWidget* previous) {
        auto editor = new KUrlRequester(listWidget);
        editor->setPlaceholderText(tr("auto-detect"));
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
    };
    auto lastExtraLibsWidget = setupMultiPath(unwindPage->extraLibraryPaths, unwindPage->extraLibraryPathsLabel,
                                              unwindPage->lineEditApplicationPath);
    setupMultiPath(unwindPage->debugPaths, unwindPage->debugPathsLabel, lastExtraLibsWidget);
    
    const auto configGroups = config().groupList();
    auto configfile = config();
    for (const auto& configName : configGroups) {
        if (configfile.hasGroup(configName)) {
            // itemdata is used to save the old name so the old config can be removed
            unwindPage->configComboBox->addItem(configName, configName);
        }
    }

    unwindPage->configComboBox->setDisabled(unwindPage->configComboBox->count() == 0);
    unwindPage->configComboBox->setInsertPolicy(QComboBox::InsertAtCurrent);

    connect(unwindPage->copyConfigButton, &QPushButton::pressed, this, [this] {
        const auto name = QStringLiteral("Config %1").arg(unwindPage->configComboBox->count() + 1);
        unwindPage->configComboBox->addItem(name, name);
        unwindPage->configComboBox->setDisabled(false);
        unwindPage->configComboBox->setCurrentIndex(unwindPage->configComboBox->findText(name));
        saveCurrentConfig();
    });
    connect(unwindPage->removeConfigButton, &QPushButton::pressed, this, &SettingsDialog::removeCurrentConfig);
    connect(unwindPage->configComboBox->lineEdit(), &QLineEdit::editingFinished, this, &SettingsDialog::renameCurrentConfig);
    connect(unwindPage->configComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::applyCurrentConfig);
    connect(this, &KPageDialog::accepted, this, [this] { saveCurrentConfig(); });

    for (auto field : {unwindPage->lineEditSysroot, unwindPage->lineEditApplicationPath, unwindPage->lineEditKallsyms, unwindPage->lineEditObjdump}) {
        connect(field, &KUrlRequester::textEdited, this, &SettingsDialog::saveCurrentConfig);
        connect(field, &KUrlRequester::urlSelected, this, &SettingsDialog::saveCurrentConfig);
    }

    connect(unwindPage->comboBoxArchitecture, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SettingsDialog::saveCurrentConfig);

    connect(unwindPage->debugPaths, &KEditListWidget::changed, this, &SettingsDialog::saveCurrentConfig);
    connect(unwindPage->extraLibraryPaths, &KEditListWidget::changed, this, &SettingsDialog::saveCurrentConfig);
}

void SettingsDialog::saveCurrentConfig()
{
    auto config = ::config();
    KConfigGroup group(&config, unwindPage->configComboBox->currentText());
    group.writeEntry("sysroot", sysroot());
    group.writeEntry("appPath", appPath());
    group.writeEntry("extraLibPaths", extraLibPaths());
    group.writeEntry("debugPaths", debugPaths());
    group.writeEntry("kallsyms", kallsyms());
    group.writeEntry("arch", arch());
    group.writeEntry("objdump", objdump());

    config.sync();
}

void SettingsDialog::renameCurrentConfig()
{
    // itemdata is used to save the old name so the old config can be removed
    const auto oldName = unwindPage->configComboBox->currentData().toString();
    config().deleteGroup(oldName);

    unwindPage->configComboBox->setItemData(unwindPage->configComboBox->currentIndex(), unwindPage->configComboBox->currentText());
    saveCurrentConfig();
    config().sync();
}

void SettingsDialog::removeCurrentConfig()
{
    config().deleteGroup(unwindPage->configComboBox->currentText());
    unwindPage->configComboBox->removeItem(unwindPage->configComboBox->currentIndex());

    if (unwindPage->configComboBox->count() == 0) {
        unwindPage->configComboBox->setDisabled(true);
    }
}

void SettingsDialog::applyCurrentConfig()
{
    auto config = ::config().group(unwindPage->configComboBox->currentText());
    const auto sysroot = config.readEntry("sysroot");
    const auto appPath = config.readEntry("appPath");
    const auto extraLibPaths = config.readEntry("extraLibPaths");
    const auto debugPaths = config.readEntry("debugPaths");
    const auto kallsyms = config.readEntry("kallsyms");
    const auto arch = config.readEntry("arch");
    const auto objdump = config.readEntry("objdump");
    initSettings(sysroot, appPath, extraLibPaths, debugPaths, kallsyms, arch, objdump);
    ::config().writeEntry("lastUsed", unwindPage->configComboBox->currentText());
}
