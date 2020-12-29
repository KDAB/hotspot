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

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::initSettings(const QString &sysroot, const QString &appPath, const QString &extraLibPaths,
                                  const QString &debugPaths, const QString &kallsyms, const QString &arch,
                                  const QString &objdump)
{
    ui->lineEditSysroot->setText(sysroot);
    ui->lineEditApplicationPath->setText(appPath);
    ui->lineEditExtraLibraryPaths->setText(extraLibPaths);
    ui->lineEditDebugPaths->setText(debugPaths);
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
    return ui->lineEditExtraLibraryPaths->text();
}

QString SettingsDialog::debugPaths() const
{
    return ui->lineEditDebugPaths->text();
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
