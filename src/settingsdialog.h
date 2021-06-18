/*
  settingsdialog.h

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

#pragma once

#include <KPageDialog>
#include <memory>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public KPageDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();
    void initSettings(const QString& configName);
    void initSettings(const QString& sysroot, const QString& appPath, const QString& extraLibPaths,
                      const QString& debugPaths, const QString& kallsyms, const QString& arch, const QString& objdump);    
    QString sysroot() const;
    QString appPath() const;
    QString extraLibPaths() const;
    QString debugPaths() const;
    QString kallsyms() const;
    QString arch() const;
    QString objdump() const;

private slots:
    void saveCurrentConfig();
    void renameCurrentConfig();
    void removeCurrentConfig();
    void applyCurrentConfig();

private:
    void addPathSettingsPage();

    std::unique_ptr<Ui::SettingsDialog> unwindPage;
};
