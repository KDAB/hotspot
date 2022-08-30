/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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
class CodeDelegate;
class DisassemblyDelegate;
struct DisassemblyOutput;
class DisassemblyModel;
class SourceCodeModel;

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
    DisassemblyModel* m_disassemblyModel;
    SourceCodeModel* m_sourceCodeModel;
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
    CodeDelegate* m_disassemblyDelegate;
    CodeDelegate* m_sourceCodeDelegate;
};
