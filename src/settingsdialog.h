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
class UnwindSettingsPage;
class FlamegraphSettingsPage;
class DebuginfodPage;
class CallgraphSettingsPage;
}

class SettingsDialog : public KPageDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();
    void initSettings();
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

    std::unique_ptr<Ui::UnwindSettingsPage> unwindPage;
    std::unique_ptr<Ui::FlamegraphSettingsPage> flamegraphPage;
    std::unique_ptr<Ui::DebuginfodPage> debuginfodPage;
    std::unique_ptr<Ui::CallgraphSettingsPage> callgraphPage;
    MultiConfigWidget* m_configs;
};
