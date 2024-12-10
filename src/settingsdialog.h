/*
    SPDX-FileCopyrightText: Petr Lyapidevskiy <p.lyapidevskiy@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KPageDialog>

#include <memory>

namespace Ui {
class UnwindSettingsPage;
class FlamegraphSettingsPage;
class DebuginfodPage;
class CallgraphSettingsPage;
class DisassemblySettingsPage;
class PerfSettingsPage;
}

class MultiConfigWidget;

class SettingsDialog : public KPageDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();
    void initSettings();
    QString sysroot() const;
    QString appPath() const;
    QString extraLibPaths() const;
    QString debugPaths() const;
    QString kallsyms() const;
    QString arch() const;
    QString objdump() const;
    QString perfMapPath() const;

    void keyPressEvent(QKeyEvent* event) override;

private:
    void addPerfSettingsPage();
    void addPathSettingsPage();
    void addFlamegraphPage();
    void addDebuginfodPage();
    void addCallgraphPage();
    void addSourcePathPage();

    std::unique_ptr<Ui::PerfSettingsPage> perfPage;
    std::unique_ptr<Ui::UnwindSettingsPage> unwindPage;
    std::unique_ptr<Ui::FlamegraphSettingsPage> flamegraphPage;
    std::unique_ptr<Ui::DebuginfodPage> debuginfodPage;
    std::unique_ptr<Ui::DisassemblySettingsPage> disassemblyPage;
    std::unique_ptr<Ui::CallgraphSettingsPage> callgraphPage;
    std::unique_ptr<MultiConfigWidget> m_configs;
};
