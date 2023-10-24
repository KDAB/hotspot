/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsdisassemblypage.h"
#include "ui_resultsdisassemblypage.h"

#include <QActionGroup>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>
#include <QTextStream>
#include <QVarLengthArray>

#include <KColorScheme>
#include <KStandardAction>

#include "resultsutil.h"

#if KFSyntaxHighlighting_FOUND
#include "highlighter.hpp"
#include <KSyntaxHighlighting/definition.h>
#include <KSyntaxHighlighting/repository.h>
#include <QCompleter>
#include <QStringListModel>
#endif

#include "costheaderview.h"
#include "data.h"
#include "models/codedelegate.h"
#include "models/costdelegate.h"
#include "models/disassemblymodel.h"
#include "models/search.h"
#include "models/sourcecodemodel.h"
#include "settings.h"

namespace {
template<typename ModelType, typename ResultFound, typename EndReached>
void connectModel(ModelType* model, ResultFound resultFound, EndReached endReached)
{
    QObject::connect(model, &ModelType::resultFound, model, resultFound);
    QObject::connect(model, &ModelType::searchEndReached, model, endReached);
}

class ColumnSpanDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ColumnSpanDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }
    ~ColumnSpanDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto opt = option;
        opt.index = index.siblingAtColumn(DisassemblyModel::DisassemblyColumn);
        QStyledItemDelegate::paint(painter, opt, opt.index);
    }
};

class BranchDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BranchDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }
    ~BranchDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const auto jumps = findJumps(index);
        if (jumps.data.isEmpty())
            return;

        const auto top = option.rect.top();
        const auto right = option.rect.right();
        const auto bottom = option.rect.bottom();
        const auto left = option.rect.left();
        const auto horizontalAdvance = 3;
        const auto horizontalMidAdvance = 1;
        const auto ymid = top + option.rect.height() / 2;

        // we merge horizontal lines into one long line
        int horizontalLineStart = -1;

        QVarLengthArray<QLine, 64> lines;
        auto x = left;
        for (int i = 0, size = jumps.data.size(); i < size; ++i) {
            const auto xend = x + horizontalAdvance;
            const auto xmid = x + horizontalMidAdvance;
            if (xmid > right)
                break;

            auto verticalLine = [&]() { lines.append({xmid, top, xmid, bottom}); };
            auto startHorizontalLine = [&horizontalLineStart](int x) {
                if (horizontalLineStart == -1)
                    horizontalLineStart = x;
            };
            auto topRightEdge = [&]() {
                if (!jumps.fromSibling) {
                    lines.append({xmid, top, xmid, ymid});
                    startHorizontalLine(xend);
                }
            };
            auto bottomLeftEdge = [&]() {
                if (!jumps.fromSibling) {
                    lines.append({xmid, bottom, xmid, ymid});
                    startHorizontalLine(xend);
                } else {
                    verticalLine();
                }
            };

            const auto c = jumps.data[i];
            switch (c.toLatin1()) {
            case ' ':
                break;
            case '|':
                verticalLine();
                break;
            case '-':
                startHorizontalLine(x);
                break;
            case '+':
                startHorizontalLine(x);
                verticalLine();
                break;
            case '\\':
                topRightEdge();
                break;
            case '/':
                bottomLeftEdge();
                break;
            case '>':
                if (!jumps.fromSibling) {
                    if (i == size - 2) {
                        // jump target ends with "> "
                    } else {
                        // branch intersection
                        verticalLine();
                    }
                    startHorizontalLine(xend);
                } else {
                    verticalLine();
                }
                break;
            default:
                qWarning("unexpected jump visualization char: %c", c.toLatin1());
                break;
            }

            x = xend;
        }

        if (!jumps.fromSibling && horizontalLineStart != -1) {
            auto lineEnd = right;

            const auto arrowSize = 4;
            if (jumps.data.endsWith(QLatin1String("> "))) {
                // jump target
                lines.append({right - arrowSize, ymid + arrowSize, right, ymid});
                lines.append({right - arrowSize, ymid - arrowSize, right, ymid});
            } else {
                // jump
                lines.append({right, ymid + arrowSize, right - arrowSize, ymid});
                lines.append({right, ymid - arrowSize, right - arrowSize, ymid});
                lineEnd -= arrowSize;
            }

            lines.append({horizontalLineStart, ymid, lineEnd, ymid});
        }

        auto pen = QPen(option.palette.color(QPalette::Link), 1);
        pen.setCosmetic(true);
        const auto oldPen = painter->pen();
        painter->setPen(pen);

        painter->drawLines(lines.constData(), lines.size());
        painter->setPen(oldPen);
    }

private:
    struct Jumps
    {
        QString data;
        // when we take the jumps from siblings, we only want to draw the vertical lines
        bool fromSibling = false;
    };
    static Jumps findJumps(QModelIndex index)
    {
        bool fromSibling = false;
        // find row that might have jumps (i.e. has valid addr column)
        while (index.row() > 0 && !index.data(DisassemblyModel::AddrRole).toULongLong()) {
            index = index.siblingAtRow(index.row() - 1);
            fromSibling = true;
        }
        return {index.data().toString(), fromSibling};
    }
};
}

ResultsDisassemblyPage::ResultsDisassemblyPage(CostContextMenu* costContextMenu, QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ResultsDisassemblyPage>())
#if KFSyntaxHighlighting_FOUND
    , m_repository(std::make_unique<KSyntaxHighlighting::Repository>())
    , m_disassemblyModel(new DisassemblyModel(m_repository.get(), this))
    , m_sourceCodeModel(new SourceCodeModel(m_repository.get(), this))
#else
    , m_disassemblyModel(new DisassemblyModel(nullptr, this))
    , m_sourceCodeModel(new SourceCodeModel(nullptr, this))
#endif
    , m_disassemblyCostDelegate(new CostDelegate(DisassemblyModel::CostRole, DisassemblyModel::TotalCostRole, this))
    , m_sourceCodeCostDelegate(new CostDelegate(SourceCodeModel::CostRole, SourceCodeModel::TotalCostRole, this))
    , m_disassemblyDelegate(new CodeDelegate(DisassemblyModel::RainbowLineNumberRole, DisassemblyModel::HighlightRole,
                                             DisassemblyModel::SyntaxHighlightRole, this))
    , m_sourceCodeDelegate(new CodeDelegate(SourceCodeModel::RainbowLineNumberRole, SourceCodeModel::HighlightRole,
                                            SourceCodeModel::SyntaxHighlightRole, this))
    , m_branchesDelegate(new BranchDelegate(this))
{
    // TODO: the auto resize behavior is broken with these models that don't have the stretch column on the left
    auto setCostHeader = [this, costContextMenu](QTreeView* view) {
        auto header = new CostHeaderView(costContextMenu, this);
        header->setAutoResize(false);
        view->setHeader(header);
    };

    ui->setupUi(this);
    ui->assemblyView->setModel(m_disassemblyModel);
    ui->assemblyView->setMouseTracking(true);
    setCostHeader(ui->assemblyView);
    ui->assemblyView->setDrawColumnSpanDelegate(new ColumnSpanDelegate(this));

    ui->sourceCodeView->setModel(m_sourceCodeModel);
    ui->sourceCodeView->setMouseTracking(true);
    setCostHeader(ui->sourceCodeView);

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

    connect(settings, &Settings::sourceCodePathsChanged, this, [this](const QString&) { showDisassembly(); });

    connect(ui->assemblyView, &QTreeView::entered, this, updateFromDisassembly);
    connect(ui->sourceCodeView, &QTreeView::entered, this, updateFromSource);

    ui->sourceCodeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceCodeView, &QTreeView::customContextMenuRequested, this, [this](QPoint point) {
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

    auto addScrollTo = [](QTreeView* sourceView, QTreeView* destView, auto sourceModel, auto destModel) {
        connect(sourceView, &QTreeView::clicked, sourceView, [=](const QModelIndex& index) {
            const auto fileLine = sourceModel->fileLineForIndex(index);
            if (fileLine.isValid()) {
                destView->scrollTo(destModel->indexForFileLine(fileLine));
            }
        });
    };

    addScrollTo(ui->sourceCodeView, ui->assemblyView, m_sourceCodeModel, m_disassemblyModel);
    addScrollTo(ui->assemblyView, ui->sourceCodeView, m_disassemblyModel, m_sourceCodeModel);

    connect(ui->assemblyView, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString functionName = index.data(DisassemblyModel::LinkedFunctionNameRole).toString();
        if (functionName.isEmpty())
            return;

        const auto functionOffset = index.data(DisassemblyModel::LinkedFunctionOffsetRole).toInt();

        if (m_symbolStack[m_stackIndex].symbol == functionName) {
            ui->assemblyView->scrollTo(m_disassemblyModel->findIndexWithOffset(functionOffset),
                                       QAbstractItemView::ScrollHint::PositionAtTop);
        } else {
            const auto symbol =
                std::find_if(m_callerCalleeResults.entries.keyBegin(), m_callerCalleeResults.entries.keyEnd(),
                             [functionName](const Data::Symbol& symbol) { return symbol.symbol == functionName; });

            if (symbol != m_callerCalleeResults.entries.keyEnd()) {
                m_symbolStack.push_back(*symbol);
                m_stackIndex++;
                emit stackChanged();
            } else {
                ui->symbolNotFound->setText(tr("unknown symbol %1").arg(functionName));
                ui->symbolNotFound->show();
            }
        }
    });

    connect(ui->stackBackButton, &QPushButton::pressed, this, [this] {
        m_stackIndex--;
        if (m_stackIndex < 0)
            m_stackIndex = m_symbolStack.size() - 1;

        emit stackChanged();
    });

    connect(ui->stackNextButton, &QPushButton::pressed, this, [this] {
        m_stackIndex++;
        if (m_stackIndex >= m_symbolStack.size())
            m_stackIndex = 0;

        emit stackChanged();
    });

    connect(this, &ResultsDisassemblyPage::stackChanged, this, [this] {
        ui->stackBackButton->setEnabled(m_stackIndex > 0);
        ui->stackNextButton->setEnabled(m_stackIndex < m_symbolStack.size() - 1);

        ui->stackEntry->setText(m_symbolStack[m_stackIndex].prettySymbol);

        showDisassembly();
    });

    ui->searchEndWidget->hide();
    ui->disasmEndReachedWidget->hide();

    auto setupSearchShortcuts = [this](QPushButton* search, QPushButton* next, QPushButton* prev, QPushButton* close,
                                       QWidget* searchWidget, QLineEdit* edit, QAbstractItemView* view,
                                       KMessageWidget* endReached, auto* model, QModelIndex* searchResultIndex,
                                       int additionalRows) {
        searchWidget->hide();

        auto actions = new QActionGroup(view);

        auto findAction = KStandardAction::find(
            this,
            [searchWidget, edit] {
                searchWidget->show();
                edit->setFocus();
            },
            actions);
        findAction->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
        view->addAction(findAction);

        auto searchNext = [model, edit, additionalRows, searchResultIndex] {
            const auto offset = searchResultIndex->isValid() ? searchResultIndex->row() - additionalRows : 0;
            model->find(edit->text(), Direction::Forward, offset);
        };

        auto searchPrev = [model, edit, additionalRows, searchResultIndex] {
            const auto offset = searchResultIndex->isValid() ? searchResultIndex->row() - additionalRows : 0;

            model->find(edit->text(), Direction::Backward, offset);
        };

        auto findNextAction = KStandardAction::findNext(this, searchNext, actions);
        findNextAction->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
        searchWidget->addAction(findNextAction);
        auto findPrevAction = KStandardAction::findPrev(this, searchPrev, actions);
        findPrevAction->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
        searchWidget->addAction(findPrevAction);

        connect(edit, &QLineEdit::returnPressed, findNextAction, &QAction::trigger);
        connect(next, &QPushButton::clicked, findNextAction, &QAction::trigger);
        connect(prev, &QPushButton::clicked, findPrevAction, &QAction::trigger);

        connect(search, &QPushButton::clicked, findAction, &QAction::trigger);
        connect(close, &QPushButton::clicked, this, [searchWidget] { searchWidget->hide(); });

        const auto colorScheme = KColorScheme();

        connectModel(
            model,
            [edit, view, colorScheme, searchResultIndex](const QModelIndex& index) {
                auto palette = edit->palette();
                *searchResultIndex = index;
                palette.setBrush(QPalette::Text,
                                 index.isValid() ? colorScheme.foreground()
                                                 : colorScheme.foreground(KColorScheme::NegativeText));
                edit->setPalette(palette);
                view->setCurrentIndex(index);

                if (!index.isValid())
                    view->clearSelection();
            },
            [endReached] { endReached->show(); });
    };

    setupSearchShortcuts(ui->searchButton, ui->nextResult, ui->prevResult, ui->closeButton, ui->searchWidget,
                         ui->searchEdit, ui->sourceCodeView, ui->searchEndWidget, m_sourceCodeModel,
                         &m_currentSourceSearchIndex, 1);
    setupSearchShortcuts(ui->disasmSearchButton, ui->disasmNextButton, ui->disasmPrevButton, ui->disasmCloseButton,
                         ui->disasmSearchWidget, ui->disasmSearchEdit, ui->assemblyView, ui->disasmEndReachedWidget,
                         m_disassemblyModel, &m_currentDisasmSearchIndex, 0);

    ui->assemblyView->setColumnHidden(DisassemblyModel::BranchColumn, !settings->showBranches());
    ui->assemblyView->setColumnHidden(DisassemblyModel::HexdumpColumn, !settings->showHexdump());

    connect(settings, &Settings::showBranchesChanged, this, [this](bool showBranches) {
        ui->assemblyView->setColumnHidden(DisassemblyModel::BranchColumn, !showBranches);
    });

    connect(settings, &Settings::showHexdumpChanged, this, [this](bool showHexdump) {
        ui->assemblyView->setColumnHidden(DisassemblyModel::HexdumpColumn, !showHexdump);
    });

#if KFSyntaxHighlighting_FOUND
    QStringList schemes;

    auto definitions = m_repository->definitions();
    schemes.reserve(definitions.size());

    std::transform(definitions.begin(), definitions.end(), std::back_inserter(schemes),
                   [](const auto& definition) { return definition.name(); });

    auto sourceCodeDefinitionModel = new QStringListModel(this);
    sourceCodeDefinitionModel->setStringList(schemes);

    QStringList assemblySchemes = {QStringLiteral("None")};

    std::transform(definitions.begin(),
                   std::partition(definitions.begin(), definitions.end(),
                                  [](const KSyntaxHighlighting::Definition& definition) {
                                      return definition.section() == QStringLiteral("Assembler");
                                  }),
                   std::back_inserter(assemblySchemes),
                   [](const KSyntaxHighlighting::Definition& definition) { return definition.name(); });

    auto assemblySchemesModel = new QStringListModel(this);
    assemblySchemesModel->setStringList(assemblySchemes);

    auto connectCompletion = [schemes, this](QStringListModel* definitionModel, QComboBox* box, auto* model) {
        auto completer = new QCompleter(this);
        completer->setModel(definitionModel);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        box->setCompleter(completer);
        box->setModel(definitionModel);
        box->setCurrentText(model->highlighter()->definition());

        connect(box, qOverload<int>(&QComboBox::activated), this, [this, model, box]() {
            model->highlighter()->setDefinition(m_repository->definitionForName(box->currentText()));
        });

        connect(model->highlighter(), &Highlighter::definitionChanged,
                [box](const QString& definition) { box->setCurrentText(definition); });
    };

    connectCompletion(sourceCodeDefinitionModel, ui->sourceCodeComboBox, m_sourceCodeModel);
    connectCompletion(assemblySchemesModel, ui->assemblyComboBox, m_disassemblyModel);
#else
    ui->customSourceCodeHighlighting->setVisible(false);
    ui->customAssemblyHighlighting->setVisible(false);
#endif
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear()
{
    m_disassemblyModel->clear();
    m_sourceCodeModel->clear();
}

void ResultsDisassemblyPage::setupAsmViewModel()
{

    ui->sourceCodeView->setItemDelegateForColumn(SourceCodeModel::SourceCodeColumn, m_sourceCodeDelegate);
    ui->sourceCodeView->header()->setStretchLastSection(false);
    ui->sourceCodeView->header()->setSectionResizeMode(SourceCodeModel::SourceCodeLineNumber,
                                                       QHeaderView::ResizeToContents);
    ui->sourceCodeView->header()->setSectionResizeMode(SourceCodeModel::SourceCodeColumn, QHeaderView::Stretch);

    ui->assemblyView->setItemDelegateForColumn(DisassemblyModel::BranchColumn, m_branchesDelegate);
    ui->assemblyView->setItemDelegateForColumn(DisassemblyModel::DisassemblyColumn, m_disassemblyDelegate);
    ui->assemblyView->header()->setStretchLastSection(false);
    ui->assemblyView->header()->setSectionResizeMode(DisassemblyModel::AddrColumn, QHeaderView::ResizeToContents);
    ui->assemblyView->header()->setSectionResizeMode(DisassemblyModel::BranchColumn, QHeaderView::Interactive);
    ui->assemblyView->header()->setSectionResizeMode(DisassemblyModel::HexdumpColumn, QHeaderView::Interactive);
    ui->assemblyView->header()->setSectionResizeMode(DisassemblyModel::DisassemblyColumn, QHeaderView::Stretch);

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
    if (m_symbolStack.isEmpty())
        return;

    const auto& curSymbol = m_symbolStack[m_stackIndex];

    // Show empty tab when selected symbol is not valid
    if (curSymbol.symbol.isEmpty()) {
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

    ui->symbolNotFound->hide();

    auto settings = Settings::instance();
    const auto colon = QLatin1Char(':');

    showDisassembly(DisassemblyOutput::disassemble(
        objdump(), m_arch, settings->debugPaths().split(colon), settings->extraLibPaths().split(colon),
        settings->sourceCodePaths().split(colon), settings->sysroot(), curSymbol));
}

void ResultsDisassemblyPage::showDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    m_disassemblyModel->clear();
    m_sourceCodeModel->clear();

    // this function is only called if m_symbolStack is non empty (see above)
    Q_ASSERT(!m_symbolStack.isEmpty());
    const auto& curSymbol = m_symbolStack[m_stackIndex];

#if KFSyntaxHighlighting_FOUND
    m_sourceCodeModel->highlighter()->setDefinition(
        m_repository->definitionForFileName(disassemblyOutput.mainSourceFileName));
    m_disassemblyModel->highlighter()->setDefinition(m_repository->definitionForName(QStringLiteral("GNU Assembler")));
#endif

    const auto& entry = m_callerCalleeResults.entry(curSymbol);

    ui->filenameLabel->setText(disassemblyOutput.mainSourceFileName);
    // don't set tooltip on symbolLabel, as that will be called internally and then get overwritten
    setToolTip(Util::formatTooltip(entry.id, curSymbol, m_callerCalleeResults.selfCosts,
                                   m_callerCalleeResults.inclusiveCosts));

    if (!disassemblyOutput) {
        ui->errorMessage->setText(disassemblyOutput.errorMessage);
        ui->errorMessage->show();
        return;
    }

    ui->errorMessage->hide();

    m_disassemblyModel->setDisassembly(disassemblyOutput, m_callerCalleeResults);
    m_sourceCodeModel->setDisassembly(disassemblyOutput, m_callerCalleeResults);

    ResultsUtil::hideEmptyColumns(m_callerCalleeResults.selfCosts, ui->assemblyView, DisassemblyModel::COLUMN_COUNT);

    ResultsUtil::hideEmptyColumns(m_callerCalleeResults.selfCosts, ui->sourceCodeView, SourceCodeModel::COLUMN_COUNT);

    ResultsUtil::hideEmptyColumns(m_callerCalleeResults.inclusiveCosts, ui->sourceCodeView,
                                  SourceCodeModel::COLUMN_COUNT + m_callerCalleeResults.selfCosts.numTypes());

    // hide self cost for tracepoints in assembly view, this is basically always zero
    ResultsUtil::hideTracepointColumns(m_callerCalleeResults.selfCosts, ui->assemblyView,
                                       DisassemblyModel::COLUMN_COUNT);

    // hide self cost for tracepoints - only show inclusive times instead here
    ResultsUtil::hideTracepointColumns(m_callerCalleeResults.selfCosts, ui->sourceCodeView,
                                       SourceCodeModel::COLUMN_COUNT);

    setupAsmViewModel();
}

void ResultsDisassemblyPage::setSymbol(const Data::Symbol& symbol)
{
    m_stackIndex = 0;
    m_symbolStack.clear();
    m_symbolStack.push_back(symbol);
    emit stackChanged();
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

#include "resultsdisassemblypage.moc"
