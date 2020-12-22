/*
  resultsdisassemblypage.cpp

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
#include "models/disassemblyoutput.h"

namespace {
enum CustomRoles
{
    CostRole = Qt::UserRole,
    TotalCostRole = Qt::UserRole + 1,
};
}

ResultsDisassemblyPage::ResultsDisassemblyPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsDisassemblyPage)
    , m_model(new QStandardItemModel(this))
    , m_costDelegate(new CostDelegate(CostRole, TotalCostRole, this))
{
    ui->setupUi(this);
    ui->asmView->setModel(m_model);
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear()
{
    int rowCount = m_model->rowCount();
    if (rowCount > 0) {
        m_model->removeRows(0, rowCount, {});
    }
}

void ResultsDisassemblyPage::setupAsmViewModel(int numTypes)
{
    ui->asmView->header()->setStretchLastSection(false);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int event = 0; event < numTypes; event++) {
        ui->asmView->setColumnWidth(event + 1, 100);
        ui->asmView->header()->setSectionResizeMode(event + 1, QHeaderView::Interactive);
        ui->asmView->setItemDelegateForColumn(event + 1, m_costDelegate);
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

    const auto numTypes = m_callerCalleeResults.selfCosts.numTypes();

    QStringList headerList;
    headerList.reserve(numTypes + 1);
    headerList.append({tr("Assembly")});
    for (int i = 0; i < numTypes; i++) {
        headerList.append(m_callerCalleeResults.selfCosts.typeName(i));
    }
    m_model->setHorizontalHeaderLabels(headerList);

    for (int row = 0; row < disassemblyOutput.disassemblyLines.size(); row++) {
        const auto& disassemblyLine = disassemblyOutput.disassemblyLines.at(row);

        auto* asmItem = new QStandardItem(disassemblyLine.disassembly);
        asmItem->setFlags(asmItem->flags().setFlag(Qt::ItemIsEditable, false));
        m_model->setItem(row, 0, asmItem);

        // Calculate event times and add them in red to corresponding columns of the current disassembly row
        auto it = entry.offsetMap.find(disassemblyLine.addr);
        if (it != entry.offsetMap.end()) {
            const auto& locationCost = it.value();
            const auto tooltip = Util::formatTooltip(asmItem->text(), locationCost, m_callerCalleeResults.selfCosts);
            asmItem->setToolTip(tooltip);
            for (int event = 0; event < numTypes; event++) {
                const auto &costLine = locationCost.selfCost[event];
                const auto totalCost = m_callerCalleeResults.selfCosts.totalCost(event);
                const auto cost = Util::formatCostRelative(costLine, totalCost, true);

                // FIXME QStandardItem stuff should be reimplemented properly
                auto* costItem = new QStandardItem(cost);
                costItem->setFlags(asmItem->flags().setFlag(Qt::ItemIsEditable, false));
                costItem->setData(costLine, CostRole);
                costItem->setData(totalCost, TotalCostRole);
                costItem->setToolTip(tooltip);
                m_model->setItem(row, event + 1, costItem);
            }
        } else {
            asmItem->setToolTip(
                tr("<qt><tt>%1</tt><hr/>No samples at this location.</qt>").arg(asmItem->text().toHtmlEscaped()));
        }
    }
    setupAsmViewModel(numTypes);
}

void ResultsDisassemblyPage::setSymbol(const Data::Symbol& symbol)
{
    m_curSymbol = symbol;
}

void ResultsDisassemblyPage::setCostsMap(const Data::CallerCalleeResults& callerCalleeResults)
{
    m_callerCalleeResults = callerCalleeResults;
}

void ResultsDisassemblyPage::setObjdump(const QString& objdump)
{
    m_objdump = objdump;
}

void ResultsDisassemblyPage::setArch(const QString& arch)
{
    m_arch = arch.trimmed().toLower();
}
