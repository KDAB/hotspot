/*
  resultsdisassemblypage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Darya Knysh <d.knysh@nips.ru>

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

#include "data.h"
#include "models/costdelegate.h"
#include <QWidget>

class QMenu;

namespace Ui {
class ResultsDisassemblyPage;
}

namespace Data {
struct Symbol;
struct DisassemblyResult;
}

class QTreeView;

class QStandardItemModel;
class QTemporaryFile;
class CostDelegate;
class DisassemblyOutput;
class DisassemblyModel;

class ResultsDisassemblyPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsDisassemblyPage(QWidget* parent = nullptr);
    ~ResultsDisassemblyPage();

    void clear();
    void setupAsmViewModel();
    void showDisassembly();
    void setAppPath(const QString& path);
    void setSymbol(const Data::Symbol& data);
    void setCostsMap(const Data::CallerCalleeResults& callerCalleeResults);
    void setObjdump(const QString& objdump);
    void setArch(const QString& arch);
signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);

private:
    void showDisassembly(const DisassemblyOutput& disassemblyOutput);

    QScopedPointer<Ui::ResultsDisassemblyPage> ui;
    // Model
    DisassemblyModel* m_model;
    // Current chosen function symbol
    Data::Symbol m_curSymbol;
    // Architecture
    QString m_arch;
    // Objdump binary name
    QString m_objdump;
    // Map of symbols and its locations with costs
    Data::CallerCalleeResults m_callerCalleeResults;
    // Cost delegate
    CostDelegate* m_costDelegate;
};
