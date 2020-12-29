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
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto setupMultiPath = [](KEditListWidget* listWidget, QLabel* buddy, QWidget* previous)
    {
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
    auto lastExtraLibsWidget = setupMultiPath(ui->extraLibraryPaths, ui->extraLibraryPathsLabel, ui->lineEditApplicationPath);
    setupMultiPath(ui->debugPaths, ui->debugPathsLabel, lastExtraLibsWidget);
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
    fromPathString(ui->extraLibraryPaths, extraLibPaths);
    fromPathString(ui->debugPaths, debugPaths);

    ui->lineEditSysroot->setText(sysroot);
    ui->lineEditApplicationPath->setText(appPath);
    ui->lineEditKallsyms->setText(kallsyms);
    ui->lineEditObjdump->setText(objdump);

    int itemIndex = 0;
    if (!arch.isEmpty()) {
        itemIndex = ui->comboBoxArchitecture->findText(arch);
        if (itemIndex == -1) {
            itemIndex = ui->comboBoxArchitecture->count();
            ui->comboBoxArchitecture->addItem(arch);
        }
    }
    ui->comboBoxArchitecture->setCurrentIndex(itemIndex);
}

QString SettingsDialog::sysroot() const
{
    return ui->lineEditSysroot->text();
}

QString SettingsDialog::appPath() const
{
    return ui->lineEditApplicationPath->text();
}

QString SettingsDialog::extraLibPaths() const
{
    return ui->extraLibraryPaths->items().join(QLatin1Char(':'));
}

QString SettingsDialog::debugPaths() const
{
    return ui->debugPaths->items().join(QLatin1Char(':'));
}

QString SettingsDialog::kallsyms() const
{
    return ui->lineEditKallsyms->text();
}

QString SettingsDialog::arch() const
{
    QString sArch = ui->comboBoxArchitecture->currentText();
    return (sArch == QLatin1String("auto-detect")) ? QString() : sArch;
}

QString SettingsDialog::objdump() const
{
    return ui->lineEditObjdump->text();
}
