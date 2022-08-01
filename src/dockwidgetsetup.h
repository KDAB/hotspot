/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace KDDockWidgets {
class MainWindow;
class DockWidget;
}

class QWidget;
class QString;

void setupDockWidgets();
KDDockWidgets::MainWindow* createDockingArea(const QString& id, QWidget* parent);
KDDockWidgets::DockWidget* dockify(QWidget* widget, const QString& id, const QString& title, const QString& shortcut);
