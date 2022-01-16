/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsdisassemblypage.h"
#include "ui_resultsdisassemblypage.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>
#include <QTextStream>
#include <QStandardItemModel>
#include <QStandardPaths>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "data.h"
#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"
#include "models/disassemblymodel.h"

ResultsDisassemblyPage::ResultsDisassemblyPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsDisassemblyPage)
    , m_model(new DisassemblyModel(this))
    , m_costDelegate(new CostDelegate(DisassemblyModel::CostRole, DisassemblyModel::TotalCostRole, this))
{
    ui->setupUi(this);
    ui->asmView->setModel(m_model);
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear()
{
    m_model->clear();
}

void ResultsDisassemblyPage::setupAsmViewModel()
{
    ui->asmView->header()->setStretchLastSection(false);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int col = 1; col < m_model->columnCount(); col++) {
        ui->asmView->setColumnWidth(col, 100);
        ui->asmView->header()->setSectionResizeMode(col, QHeaderView::Interactive);
        ui->asmView->setItemDelegateForColumn(col, m_costDelegate);
    }
}

void ResultsDisassemblyPage::showDisassembly()
{
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
    }

    // TODO: add the ability to configure the arch <-> objdump mapping somehow in the settings
    const auto objdump = [this]() {
        if (!m_objdump.isEmpty())
            return m_objdump;

        if (m_arch.startsWith(QLatin1String("armv8")) || m_arch.startsWith(QLatin1String("aarch64"))) {
            return QStringLiteral("aarch64-linux-gnu-objdump");
        }
        const auto isArm = m_arch.startsWith(QLatin1String("arm"));
        return isArm ? QStringLiteral("arm-linux-gnueabi-objdump") : QStringLiteral("objdump");
    };

    showDisassembly(DisassemblyOutput::disassemble(objdump(), m_arch, m_curSymbol));
}

void ResultsDisassemblyPage::showDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    m_model->clear();

    const auto& entry = m_callerCalleeResults.entry(m_curSymbol);

    ui->symbolLabel->setText(tr("Disassembly for symbol:  %1").arg(Util::formatSymbol(m_curSymbol)));
    // don't set tooltip on symbolLabel, as that will be called internally and then get overwritten
    setToolTip(Util::formatTooltip(entry.id, m_curSymbol, m_callerCalleeResults.selfCosts,
                                   m_callerCalleeResults.inclusiveCosts));

    if (!disassemblyOutput) {
        ui->errorMessage->setText(disassemblyOutput.errorMessage);
        ui->errorMessage->show();
        return;
    }

    ui->errorMessage->hide();

    m_model->setDisassembly(disassemblyOutput);

    setupAsmViewModel();
}

void ResultsDisassemblyPage::setSymbol(const Data::Symbol& symbol)
{
    m_curSymbol = symbol;
}

void ResultsDisassemblyPage::setCostsMap(const Data::CallerCalleeResults& callerCalleeResults)
{
    m_callerCalleeResults = callerCalleeResults;
    m_model->setResults(m_callerCalleeResults);    
}

void ResultsDisassemblyPage::setObjdump(const QString& objdump)
{
    m_objdump = objdump;
}

void ResultsDisassemblyPage::setArch(const QString& arch)
{
    m_arch = arch.trimmed().toLower();
}
