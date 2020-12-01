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

#include <QWidget>
#include "data.h"
#include "models/costdelegate.h"

class QMenu;

namespace Ui {
class ResultsDisassemblyPage;
}

namespace Data {
struct Symbol;
struct DisassemblyResult;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;
class QStandardItemModel;
class QTemporaryFile;
class CostDelegate;

struct DisassemblyOutput
{
    QString errorMessage;
    explicit operator bool() const { return errorMessage.isEmpty(); }
    static DisassemblyOutput fromProcess(const QString &processName, const QStringList &arguments);

    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
    };
    QVector<DisassemblyLine> disassemblyLines;
};
Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);

static QVector<DisassemblyOutput::DisassemblyLine> objdumpParse(QByteArray output, int numTypes);

class ResultsDisassemblyPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsDisassemblyPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                 QWidget* parent = nullptr);
    ~ResultsDisassemblyPage();

    void clear();
    void setupAsmViewModel(int numTypes);
    void showDisassembly();
    // Output Disassembly that is the result of call process running 'processName' command on tab Disassembly
    void showDisassembly(const QString& processName, const QStringList& arguments);
    void setAppPath(const QString& path);
    void setSymbol(const Data::Symbol& data);
    void setData(const Data::DisassemblyResult& data);
    void setCostsMap(const Data::CallerCalleeResults& callerCalleeResults);
    void setObjdump(const QString& objdump);
signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
private:
    QScopedPointer<Ui::ResultsDisassemblyPage> ui;
    // Model
    QStandardItemModel *m_model;
    // Perf.data path
    QString m_perfDataPath;
    // Current chosen function symbol
    Data::Symbol m_curSymbol;
    // Application path
    QString m_appPath;
    // Extra libs path
    QString m_extraLibPaths;
    // Architecture
    QString m_arch;
    // Objdump binary name
    QString m_objdump;
    // Objdump path from settings
    QString m_objdumpPath;
    // Map of symbols
    Data::DisassemblyResult m_disasmResult;
    // Map of symbols and its locations with costs
    Data::CallerCalleeResults m_callerCalleeResults;
    // Cost delegate
    CostDelegate *m_costDelegate;
};
