/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsdisassemblypage.h"
#include "ui_resultsdisassemblypage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>
#include <QTextStream>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "data.h"
#include "models/codedelegate.h"
#include "models/costdelegate.h"
#include "models/disassemblymodel.h"
#include "models/hashmodel.h"
#include "models/sourcecodemodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"
#include "settings.h"

ResultsDisassemblyPage::ResultsDisassemblyPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsDisassemblyPage)
    , m_disassemblyModel(new DisassemblyModel(this))
    , m_sourceCodeModel(new SourceCodeModel(this))
    , m_disassemblyCostDelegate(new CostDelegate(DisassemblyModel::CostRole, DisassemblyModel::TotalCostRole, this))
    , m_sourceCodeCostDelegate(new CostDelegate(SourceCodeModel::CostRole, SourceCodeModel::TotalCostRole, this))
    , m_disassemblyDelegate(new CodeDelegate(DisassemblyModel::RainbowLineNumberRole, DisassemblyModel::HighlightRole,
                                             DisassemblyModel::SyntaxHighlightRole, this))
    , m_sourceCodeDelegate(new CodeDelegate(SourceCodeModel::RainbowLineNumberRole, SourceCodeModel::HighlightRole,
                                            SourceCodeModel::SyntaxHighlightRole, this))
{
    ui->setupUi(this);
    ui->assemblyView->setModel(m_disassemblyModel);
    ui->assemblyView->setMouseTracking(true);
    ui->sourceCodeView->setModel(m_sourceCodeModel);
    ui->sourceCodeView->setMouseTracking(true);

    auto settings = Settings::instance();
    m_sourceCodeModel->setSysroot(settings->sysroot());

    connect(settings, &Settings::sysrootChanged, m_sourceCodeModel, &SourceCodeModel::setSysroot);

    auto updateFromDisassembly = [this](const QModelIndex& index) {
        const auto fileLine = m_disassemblyModel->fileLineForIndex(index);
        m_disassemblyModel->updateHighlighting(fileLine.line);
        m_sourceCodeModel->updateHighlighting(fileLine.line);
    };

    auto updateFromSource = [this](const QModelIndex& index) {
        const auto fileLine = m_sourceCodeModel->fileLineForIndex(index);
        m_disassemblyModel->updateHighlighting(fileLine.line);
        m_sourceCodeModel->updateHighlighting(fileLine.line);
    };

    connect(ui->assemblyView, &QTreeView::entered, this, updateFromDisassembly);

    connect(ui->sourceCodeView, &QTreeView::entered, this, updateFromSource);

    ui->sourceCodeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceCodeView, &QTreeView::customContextMenuRequested, this, [this](const QPoint& point) {
        const auto index = ui->sourceCodeView->indexAt(point);
        const auto fileLine = index.data(SourceCodeModel::FileLineRole).value<Data::FileLine>();
        if (!fileLine.isValid())
            return;

        QMenu contextMenu;
        auto* openEditorAction = contextMenu.addAction(QCoreApplication::translate("Util", "Open in Editor"));
        QObject::connect(openEditorAction, &QAction::triggered, &contextMenu,
                         [this, fileLine]() { emit navigateToCode(fileLine.file, fileLine.line, -1); });
        contextMenu.exec(QCursor::pos());
    });
    connect(ui->sourceCodeView, &QTreeView::activated, this, [this](const QModelIndex& index) {
        const auto fileLine = m_sourceCodeModel->fileLineForIndex(index);
        if (fileLine.isValid()) {
            ui->assemblyView->scrollTo(m_disassemblyModel->indexForFileLine(fileLine));
        }
    });

    connect(ui->assemblyView, &QTreeView::activated, this, [this](const QModelIndex& index) {
        const auto fileLine = m_disassemblyModel->fileLineForIndex(index);
        if (fileLine.isValid()) {
            ui->sourceCodeView->scrollTo(m_sourceCodeModel->indexForFileLine(fileLine));
        }

        const QString functionName = index.data(DisassemblyModel::LinkedFunctionNameRole).toString();
        if (functionName.isEmpty())
            return;

        int functionOffset = index.data(DisassemblyModel::LinkedFunctionOffsetRole).toInt();

        if (m_curSymbol.symbol == functionName) {
            ui->assemblyView->scrollTo(m_disassemblyModel->findIndexWithOffset(functionOffset),
                                       QAbstractItemView::ScrollHint::PositionAtTop);
        } else {
            const auto symbol =
                std::find_if(m_callerCalleeResults.entries.keyBegin(), m_callerCalleeResults.entries.keyEnd(),
                             [functionName](const Data::Symbol& symbol) { return symbol.symbol == functionName; });

            if (symbol != m_callerCalleeResults.entries.keyEnd()) {
                setSymbol(*symbol);
                showDisassembly();
            }
        }
    });
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear()
{
    m_disassemblyModel->clear();
    m_sourceCodeModel->clear();
}

void ResultsDisassemblyPage::setupAsmViewModel()
{
    ui->sourceCodeView->header()->setStretchLastSection(false);
    ui->sourceCodeView->header()->setSectionResizeMode(SourceCodeModel::SourceCodeLineNumber,
                                                       QHeaderView::ResizeToContents);
    ui->sourceCodeView->header()->setSectionResizeMode(SourceCodeModel::SourceCodeColumn, QHeaderView::Stretch);
    ui->sourceCodeView->setItemDelegateForColumn(SourceCodeModel::SourceCodeColumn, m_sourceCodeDelegate);

    ui->assemblyView->header()->setStretchLastSection(false);
    ui->assemblyView->header()->setSectionResizeMode(DisassemblyModel::DisassemblyColumn, QHeaderView::Stretch);
    ui->assemblyView->setItemDelegateForColumn(DisassemblyModel::DisassemblyColumn, m_disassemblyDelegate);

    for (int col = DisassemblyModel::COLUMN_COUNT; col < m_disassemblyModel->columnCount(); col++) {
        ui->assemblyView->setColumnWidth(col, 100);
        ui->assemblyView->header()->setSectionResizeMode(col, QHeaderView::Interactive);
        ui->assemblyView->setItemDelegateForColumn(col, m_disassemblyCostDelegate);
    }

    for (int col = SourceCodeModel::COLUMN_COUNT; col < m_sourceCodeModel->columnCount(); col++) {
        ui->sourceCodeView->setColumnWidth(col, 100);
        ui->sourceCodeView->header()->setSectionResizeMode(col, QHeaderView::Interactive);
        ui->sourceCodeView->setItemDelegateForColumn(col, m_sourceCodeCostDelegate);
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
    m_disassemblyModel->clear();
    m_sourceCodeModel->clear();

    const auto& entry = m_callerCalleeResults.entry(m_curSymbol);

    ui->filenameLabel->setText(disassemblyOutput.mainSourceFileName);
    // don't set tooltip on symbolLabel, as that will be called internally and then get overwritten
    setToolTip(Util::formatTooltip(entry.id, m_curSymbol, m_callerCalleeResults.selfCosts,
                                   m_callerCalleeResults.inclusiveCosts));

    if (!disassemblyOutput) {
        ui->errorMessage->setText(disassemblyOutput.errorMessage);
        ui->errorMessage->show();
        return;
    }

    ui->errorMessage->hide();

    m_disassemblyModel->setDisassembly(disassemblyOutput, m_callerCalleeResults);
    m_sourceCodeModel->setDisassembly(disassemblyOutput, m_callerCalleeResults);

    setupAsmViewModel();
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
