/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dockwidgets.h"

class QWidget;
class QString;

void setupDockWidgets();
DockMainWindow* createDockingArea(const QString& id, QWidget* parent);
