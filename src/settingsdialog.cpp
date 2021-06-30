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

#include <KUrlRequester>
#include <KComboBox>
#include <QListView>

SettingsDialog::SettingsDialog(QWidget* parent)
    : KPageDialog(parent)
    , unwindPage(new Ui::SettingsDialog)
{
    addPathSettingsPage();
}

SettingsDialog::~SettingsDialog() = default;

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
}
