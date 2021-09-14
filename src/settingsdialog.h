/*
    SPDX-FileCopyrightText: Petr Lyapidevskiy <p.lyapidevskiy@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KPageDialog>
#include "multiconfigwidget.h"
#include <memory>

namespace Ui {
class SettingsDialog;
class FlamegraphSettings;
class DebuginfodDialog;
class CallgraphSettings;
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

    void keyPressEvent(QKeyEvent* event) override;

private:
    void addPathSettingsPage();
    void addFlamegraphPage();
    void addDebuginfodPage();
    void addCallgraphPage();

    std::unique_ptr<Ui::SettingsDialog> unwindPage;
    std::unique_ptr<Ui::FlamegraphSettings> flamegraphPage;
    std::unique_ptr<Ui::DebuginfodDialog> debuginfodPage;
    std::unique_ptr<Ui::CallgraphSettings> callgraphSettings;
    MultiConfigWidget* m_configs;
};
