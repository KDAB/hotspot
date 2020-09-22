#include "resultsdisassemblypage.h"
#include "ui_resultsdisassemblypage.h"

#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QString>
#include <QListWidgetItem>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QDirIterator>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/filterandzoomstack.h"
#include "models/costdelegate.h"
#include "models/searchdelegate.h"
#include "models/disassemblymodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

#include <QStandardItemModel>
#include <QWheelEvent>
#include <QToolTip>
#include <QTextEdit>
#include <QShortcut>

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, QWidget *parent)
        : QWidget(parent), ui(new Ui::ResultsDisassemblyPage), m_noShowRawInsn(true), m_noShowAddress(false), m_intelSyntaxDisassembly(false) {
    ui->setupUi(this);

    ui->searchTextEdit->setPlaceholderText(QLatin1String("Search"));
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    m_origFontSize = this->font().pointSize();
    setupDisassemblyContextMenu(ui->asmView, m_origFontSize);
    connect(ui->asmView, &QAbstractItemView::doubleClicked, this, &ResultsDisassemblyPage::jumpToAsmCallee);
    m_filterAndZoomStack = filterStack;

    m_searchDelegate = new SearchDelegate(ui->asmView);
    ui->asmView->setItemDelegate(m_searchDelegate);

    connect(ui->searchTextEdit, &QTextEdit::textChanged, this, &ResultsDisassemblyPage::searchTextAndHighlight);
    connect(ui->backButton, &QToolButton::clicked, this, &ResultsDisassemblyPage::backButtonClicked);
    model = new DisassemblyModel();
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

/**
 *  Search text (taken from text editor Search) in Disassembly output and highlight found.
 */
void ResultsDisassemblyPage::searchTextAndHighlight() {
    ui->asmView->setAllColumnsShowFocus(true);

    QString text = ui->searchTextEdit->toPlainText();
    m_searchDelegate->setSearchText(text);
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

/**
 *  Override QWidget::wheelEvent
 * @param event
 */
void ResultsDisassemblyPage::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        zoomFont(event);
    }
}

/**
 *  Change font size depending on mouse wheel movement
 * @param event
 */
void ResultsDisassemblyPage::zoomFont(QWheelEvent *event) {
    QFont curFont = ui->asmView->font();

    int newFontSize = curFont.pointSize() + event->delta() / 100;
    if (newFontSize <= 0)
        return;

    curFont.setPointSize(newFontSize);

    int fontSize = (newFontSize / (double) m_origFontSize) * 100;
    ui->asmView->setFont(curFont);
    ui->asmView->setToolTip(QLatin1String("Zoom: ") + QString::number(fontSize) + QLatin1String("%"));
}

/**
 *  Remove created during Hotspot's work tmp files
 */
void ResultsDisassemblyPage::clearTmpFiles() {
    for (int i = 0; i < m_tmpAppList.size(); i++) {
        QString tmpFileName = m_tmpAppList.at(i);
        if (QFile::exists(tmpFileName))
            QFile(tmpFileName).remove();
    }
}

/**
 *  Hide instruction bytes when an argument is true
 * @param filtered
 */
void ResultsDisassemblyPage::filterDisassemblyBytes(bool filtered) {
    int row = selectedRow();
    setNoShowRawInsn(filtered);
    resetDisassembly();
    selectRow(row);
}

/**
 *  Hide instruction address when an argument is true
 * @param filtered
 */
void ResultsDisassemblyPage::filterDisassemblyAddress(bool filtered) {
    int row = selectedRow();
    setNoShowAddress(filtered);
    resetDisassembly();
    selectRow(row);
}

/**
 *  Set opcodes for call and return instructions considering selected syntax
 * @param intelSyntax
 */
void ResultsDisassemblyPage::setOpcodes(bool intelSyntax) {
    opCodeCall = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("bl") :
                 (intelSyntax ? QLatin1String("call") : QLatin1String("callq"));
    opCodeReturn = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("ret") :
                   (intelSyntax ? QLatin1String("ret") : QLatin1String("retq"));
}

/**
 *  Return selected row
 * @return
 */
int ResultsDisassemblyPage::selectedRow() {
    return (model->rowCount() > 0) ? ui->asmView->currentIndex().row() : 0;
}

/**
 *  Select row
 * @param row
 */
void ResultsDisassemblyPage::selectRow(int row) {
    ui->asmView->setCurrentIndex(model->index(row, 0));
}

/**
 *  Block Back button
 * @return
 */
void ResultsDisassemblyPage::blockBackButton() {
    if (m_callStack.isEmpty() && m_addressStack.isEmpty()) {
        ui->backButton->setEnabled(false);
    }
}

/**
 *  Switch to Intel syntax and back
 * @param intelSyntax
 */
void ResultsDisassemblyPage::switchOnIntelSyntax(bool intelSyntax) {
    int row = selectedRow();
    setIntelSyntaxDisassembly(intelSyntax);
    setOpcodes(intelSyntax);
    resetDisassembly();
    selectRow(row);
}

/**
 *  Clear
 */
void ResultsDisassemblyPage::clear() {
    if (ui->asmView->model() == nullptr) return;

    int rowCount = ui->asmView->model()->rowCount();
    if (rowCount > 0) {
        ui->asmView->model()->removeRows(0, rowCount, QModelIndex());
    }
}

/**
 *  Resize event
 * @param event
 */
void ResultsDisassemblyPage::resizeEvent(QResizeEvent *event) {
    event->accept();
    int columnsCount = ui->asmView->header()->count();
    if (columnsCount > 0) {
        ui->asmView->setColumnWidth(0, this->width() - 100 * (columnsCount - 1));
        for (int i = 0; i < columnsCount - 1; i++) {
            ui->asmView->setColumnWidth(i + 1, 100);
        }
    }
}

/**
 *  Set model to asmView and resize it's columns
 * @param model
 * @param numTypes
 */
void ResultsDisassemblyPage::setAsmViewModel(QStandardItemModel *model, int numTypes) {
    ui->asmView->setModel(model);
    ui->asmView->header()->setStretchLastSection(true);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->asmView->setColumnWidth(0, this->width() - 100 * numTypes);
    for (int event = 0; event < numTypes; event++) {
        ui->asmView->setColumnWidth(event + 1, 100);
        ui->asmView->header()->setSectionResizeMode(event + 1, QHeaderView::Interactive);
    }
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

/**
 *  Produce and show disassembly with 'objdump' depending on passed value through option --disasm-approach=<value>
 */
void ResultsDisassemblyPage::showDisassembly() {
    if (m_disasmApproach.isEmpty() || m_disasmApproach.startsWith(QLatin1String("symbol"))) {
        showDisassemblyBySymbol();
    } else {
        showDisassemblyByAddressRange();
    }
    if (!m_calleesProcessed) {
        m_searchDelegate->setCallees(m_callees);
        m_calleesProcessed = true;
    }
}

/**
 *  Produce disassembler with 'objdump' by symbol name and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassemblyBySymbol() {
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
    }

    // Call objdump with arguments: mangled name of function and binary file
    QString processName =
            m_objdump + QLatin1String(" --disassemble=") + m_curSymbol.mangled + QLatin1String(" ") + m_curAppPath;

    showDisassembly(processName);
}

/**
 *  Produce disassembler with 'objdump' by addresses range and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassemblyByAddressRange() {
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
    }

    // Call objdump with arguments: addresses range and binary file
    QString processName =
            m_objdump + QLatin1String(" -d --start-address=0x") + QString::number(m_curSymbol.relAddr, 16) +
            QLatin1String(" --stop-address=0x") +
            QString::number(m_curSymbol.relAddr + m_curSymbol.size, 16) +
            QLatin1String(" ") + m_curAppPath;

    // Workaround for the case when symbol size is equal to zero
    if (m_curSymbol.size == 0) {
        processName =
                m_objdump + QLatin1String(" --disassemble=") + m_curSymbol.mangled + QLatin1String(" ") +
                m_curAppPath;
    }
    showDisassembly(processName);
}

/**
 *  Compute installed objdump version. If it is less than required 2.32 then Disassembly item is disabled.
 * @return
 */
void ResultsDisassemblyPage::getObjdumpVersion() {
    QProcess versionProcess;
    QString processVersionName = m_objdump + QLatin1String(" -v");
    versionProcess.start(processVersionName);
    if (versionProcess.waitForStarted() && versionProcess.waitForFinished()) {
        QByteArray versionLine = versionProcess.readLine();
        QString version = QString::fromStdString(versionLine.toStdString());

        QRegExp rx(QLatin1String("\\d+\\.\\d+"));
        int pos = rx.lastIndexIn(version);
        m_objdumpVersion = rx.capturedTexts().at(0);
        if (m_objdumpVersion.toFloat() < 2.32) {
            m_filterAndZoomStack->actions().disassembly->setEnabled(false);
        }
    }    
}

/**
 * Run processName command in QProcess and diagnoses some cases
 * @param processName
 * @return
 */
QByteArray ResultsDisassemblyPage::processDisassemblyGenRun(QString processName) {
    QByteArray processOutput = QByteArray();
    if (m_curSymbol.symbol.isEmpty()) {
        processOutput = "Empty symbol ?? is selected";
    } else {
        QProcess asmProcess;
        asmProcess.start(processName);

        bool started = asmProcess.waitForStarted();
        bool finished = asmProcess.waitForFinished();
        if (!started || !finished) {
            if (!started) {
                if (!m_arch.startsWith(QLatin1String("arm"))) {
                    processOutput = QByteArray(
                            "Process was not started. Probably command 'objdump' not found, but can be installed with 'apt install binutils'");
                } else {
                    processOutput = QByteArray(
                            "Process was not started. Probably command 'arm-linux-gnueabi-objdump' not found, but can be installed with 'apt install binutils-arm-linux-gnueabi'");
                }
                m_searchDelegate->setDiagnosticStyle(true);
            } else {
                return processOutput;
            }
        } else {
            processOutput = asmProcess.readAllStandardOutput();
        }

        if (processOutput.isEmpty()) {
            processOutput = QByteArray("Empty output of command ");
            processOutput += processName.toUtf8();

            if (m_objdumpVersion.isEmpty())
                getObjdumpVersion();

            if (!m_objdumpVersion.isEmpty() && m_objdumpVersion.toFloat() < 2.32) {
                processOutput = QByteArray("Version of objdump should be >= 2.32. You use objdump with version ") +
                                m_objdumpVersion.toUtf8();
            }
            m_searchDelegate->setDiagnosticStyle(true);
        }
    }
    return processOutput;
}

/**
 *  Produce disassembler with 'objdump' and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassembly(QString processName) {
    if (m_noShowRawInsn)
        processName += QLatin1String(" --no-show-raw-insn ");

    if (m_intelSyntaxDisassembly)
        processName += QLatin1String(" -M intel ");

    QTemporaryFile m_tmpFile;

    if (m_tmpFile.open()) {
        QTextStream stream(&m_tmpFile);
        stream << processDisassemblyGenRun(processName);
        m_tmpFile.close();
    }

    if (m_tmpFile.open()) {
        int row = 0;
        model->clear();

        QStringList headerList;
        headerList.append(QLatin1String("Assembly"));
        for (int i = 0; i < m_disasmResult.selfCosts.numTypes(); i++) {
            headerList.append(m_disasmResult.selfCosts.typeName(i));
        }
        model->setHorizontalHeaderLabels(headerList);

        QTextStream stream(&m_tmpFile);
        while (!stream.atEnd()) {
            QString asmLine = stream.readLine();
            if (asmLine.isEmpty() || asmLine.startsWith(QLatin1String("Disassembly"))) continue;

            QStringList asmTokens = asmLine.split(QLatin1Char(':'));
            QString addrLine = asmTokens.value(0);
            QString costLine = QString();

            if (m_noShowAddress) {
                QRegExp hexMatcher(QLatin1String("[0-9A-F]+$"), Qt::CaseInsensitive);
                if (hexMatcher.exactMatch(addrLine.trimmed())) {
                    asmTokens.removeFirst();
                    asmLine = asmTokens.join(QLatin1Char(':')).trimmed();
                }
            }
            QStandardItem *asmItem = new QStandardItem();
            asmItem->setText(asmLine);
            model->setItem(row, 0, asmItem);

            if (!m_calleesProcessed) {
                Data::Symbol calleeSymbol = getCalleeSymbol(asmLine);
                if (calleeSymbol.isValid())
                    m_callees.insert(row, calleeSymbol);
            }

            // Calculate event times and add them in red to corresponding columns of the current disassembly row
            if (!m_disasmResult.branchTraverse ||
                !m_disasmResult.unwindMethod.startsWith(QLatin1String("lbr"))) {
                for (int event = 0; event < m_disasmResult.selfCosts.numTypes(); event++) {
                    float totalCost = 0;
                    auto &entry = m_disasmResult.entry(m_curSymbol);
                    QHash<Data::Location, Data::LocationCost>::iterator i = entry.relSourceMap.begin();
                    while (i != entry.relSourceMap.end()) {
                        Data::Location location = i.key();
                        Data::LocationCost locationCost = i.value();
                        float cost = locationCost.selfCost[event];
                        if (QString::number(location.relAddr, 16) == addrLine.trimmed()) {
                            costLine = QString::number(cost);
                        }
                        totalCost += cost;
                        i++;
                    }

                    float costInstruction = costLine.toFloat();
                    costLine = costInstruction ? QString::number(costInstruction * 100 / totalCost, 'f', 2) +
                                                 QLatin1String("%") : QString();

                    QStandardItem *costItem = new QStandardItem(costLine);
                    model->setItem(row, event + 1, costItem);
                }
            }
            row++;
        }
        setAsmViewModel(model, m_disasmResult.selfCosts.numTypes());
    }
}

/**
 * Set current chosen function symbol for Disassembler
 */
void ResultsDisassemblyPage::setData(const Data::Symbol &symbol) {
    m_curSymbol = symbol;

    if (m_curSymbol.symbol.isEmpty()) {
        return;
    }

    m_curAppPath = m_curSymbol.path;
    // If binary is not found at the specified path, use current binary file located at the application path
    if (!QFile::exists(m_curAppPath) || m_arch.startsWith(QLatin1String("arm"))) {
        m_curAppPath = m_appPath + QDir::separator() + m_curSymbol.binary;
    }
    // If binary is still not found, trying to find it in extraLibPaths
    if (!QFile::exists(m_curAppPath) || m_arch.startsWith(QLatin1String("arm"))) {
        QStringList dirs = m_extraLibPaths.split(QLatin1String(":"));
        foreach (QString dir, dirs) {
            QDirIterator it(dir, QDir::Dirs, QDirIterator::Subdirectories);

            while (it.hasNext()) {
                QString dirName = it.next();
                QString fileName = dirName + QDir::separator() + m_curSymbol.binary;
                if (QFile::exists(fileName)) {
                    m_curAppPath = fileName;
                    break;
                }
            }
        }
    }
    if (!m_curSymbol.path.isEmpty() && !QFile::exists(m_curSymbol.path)) {
        if (m_targetRoot.isEmpty()) m_targetRoot = QLatin1String("/tmp");

        QString linkPath = m_targetRoot + m_curSymbol.path;
        if (!QFile::exists(linkPath)) {
            QDir dir(QDir::root());
            QFileInfo linkPathInfo = QFileInfo(linkPath);
            dir.mkpath(linkPathInfo.absolutePath());
            QFile::copy(m_curAppPath, linkPath);

            m_tmpAppList.push_back(linkPath);
        }
    }
    m_searchDelegate->setDiagnosticStyle(false);
    m_callees.clear();
    m_calleesProcessed = false;
}

/**
 * Set path to perf.data
 */
void ResultsDisassemblyPage::setData(const Data::DisassemblyResult &data) {
    m_perfDataPath = data.perfDataPath;
    m_appPath = data.appPath;
    m_targetRoot = data.targetRoot;
    m_extraLibPaths = data.extraLibPaths;
    m_arch = data.arch.trimmed().toLower();
    m_disasmApproach = data.disasmApproach;
    m_disasmResult = data;

    m_objdump = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("arm-linux-gnueabi-objdump") : QLatin1String(
            "objdump");
    if (m_arch.startsWith(QLatin1String("armv8")) || m_arch.startsWith(QLatin1String("aarch64"))) {
        m_arch = QLatin1String("armv8");
        m_objdump = QLatin1String("aarch64-linux-gnu-objdump");
    }
    m_searchDelegate->setArch(m_arch);
    setOpcodes(m_intelSyntaxDisassembly);
}

/**
 *  Clear call stack, address stack and scroll to top when another symbol was selected
 */
void ResultsDisassemblyPage::resetCallStack() {
    m_callStack.clear();
    m_addressStack.clear();
    ui->backButton->setEnabled(false);
    if (model->rowCount() > 0) {
        ui->asmView->setCurrentIndex(model->index(0, 0));
    }
}

/**
 *  Auxilary method to extract relative address of selected 'call' instruction and it's name
 */
void calcFunction(QStringList sym, QString *offset, QString *symName) {
    *offset = sym[0].trimmed();
    *symName = sym[1].replace(QLatin1Char('>'), QLatin1String(""));
}

/**
 *  Find Data::Symbol corresponded to symbol from Assembly call instruction
 * @param asmLine
 * @return
 */
Data::Symbol ResultsDisassemblyPage::getCalleeSymbol(QString asmLine) {
    if (asmLine.contains(opCodeCall)) {
        QString offset, symName;
        QString asmLineSimple = asmLine.simplified();

        QString asmLineCall = asmLineSimple.section(opCodeCall, 1);
        QStringList sym = asmLineCall.trimmed().split(QLatin1Char('<'));
        if (sym.size() >= 2) {
            calcFunction(sym, &offset, &symName);

            QHash<Data::Symbol, Data::DisassemblyEntry>::iterator i = m_disasmResult.entries.begin();
            while (i != m_disasmResult.entries.end()) {
                QString relAddr = QString::number(i.key().relAddr, 16);
                if (!i.key().mangled.isEmpty() &&
                    (symName.contains(i.key().mangled) || symName.contains(i.key().symbol) ||
                     i.key().mangled.contains(symName) || i.key().symbol.contains(symName)) &&
                    ((relAddr == offset) ||
                     (i.key().size == 0 && i.key().relAddr == 0))) {
                    return i.key();
                }
                i++;
            }
        }
    }
    return {};
}

/**
 *  Navigate to instruction by selected address inside Disassembly output
 * @param index
 * @param asmLine
 */
void ResultsDisassemblyPage::navigateToAddressInstruction(QModelIndex index, QString asmLine) {
    QModelIndex selectedIndex = index;
    bool isScrollTo = false;
    QString addrRegExp = QLatin1String("[a-z0-9]+\\s*<");
    QRegularExpression addrMatcher = QRegularExpression(addrRegExp);
    QString address = QString();
    QRegularExpressionMatchIterator matchIterator = addrMatcher.globalMatch(asmLine);
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        address = asmLine.mid(match.capturedStart(), match.capturedLength() - 1);
        for (int i = 0; i < model->rowCount(); i++) {
            QStandardItem *asmItem = model->item(i, 0);
            if (asmItem->text().trimmed().startsWith(address.trimmed())) {
                selectedIndex = model->index(i, 0);
                isScrollTo = true;
                m_addressStack.push(index.row());
                ui->backButton->setEnabled(true);
                break;
            }
        }
    }    
    ui->asmView->setCurrentIndex(selectedIndex);
    if (isScrollTo)
        ui->asmView->scrollTo(selectedIndex);
}

/**
 *  Slot method for double click on 'call' or 'return' instructions
 *  We show it's Disassembly by double click on 'call' instruction
 *  And we go back by double click on 'return'
 */
void ResultsDisassemblyPage::jumpToAsmCallee(QModelIndex index) {
    QStandardItem *asmItem = model->item(index.row(), 0);
    QString asmLine = asmItem->text();
    if (m_callees.contains(index.row())) {
        QMap<Data::Symbol, int> callee;
        callee.insert(m_curSymbol, index.row());
        m_callStack.push(callee);
        setData(m_callees.value(index.row()));
        resetDisassembly();

        m_addressStack.clear();
        ui->asmView->scrollTo(model->index(0, 0));
        ui->backButton->setEnabled(true);
    } else if (asmLine.contains(opCodeReturn) && !m_callStack.isEmpty()) {
        returnToCaller();
    } else {
        QList<QStandardItem *> rowItems = model->takeRow(index.row());
        model->insertRow(index.row(), rowItems);

        navigateToAddressInstruction(index, asmLine);
    }
}

/**
 *  Return to Jump from
 */
void ResultsDisassemblyPage::returnToJump() {
    if (!m_addressStack.isEmpty()) {
        int jumpRow = m_addressStack.pop();
        QModelIndex jumpIndex = model->index(jumpRow, 0);
        ui->asmView->setCurrentIndex(jumpIndex);
        ui->asmView->scrollTo(jumpIndex);
    }
    blockBackButton();
}

/**
 *  Return to Caller from Callee Disassembly
 */
void ResultsDisassemblyPage::returnToCaller() {
    if (!m_callStack.isEmpty()) {
        QMap<Data::Symbol, int> caller = m_callStack.pop();
        Data::Symbol callerSymbol = caller.keys().at(0);
        int callerIndex = caller.value(callerSymbol);
        setData(callerSymbol);
        resetDisassembly();
        ui->asmView->setCurrentIndex(model->index(callerIndex, 0));
        ui->asmView->scrollTo(model->index(callerIndex, 0));    

        m_addressStack.clear();
    }
    blockBackButton();
}

/**
 *  Back button click slot to Return to Jump or Return to Caller
 */
void ResultsDisassemblyPage::backButtonClicked() {
    if (!m_addressStack.isEmpty()) {
        returnToJump();
    } else if (!m_callStack.isEmpty()) {
        returnToCaller();
    }
}

/**
 *  Reset Disassembly depending on selected approach
 */
void ResultsDisassemblyPage::resetDisassembly() {
    showDisassembly();
}

/**
 *  Setter for m_noShowRawInsn
 * @param noShowRawInsn
 */
void ResultsDisassemblyPage::setNoShowRawInsn(bool noShowRawInsn) {
    m_noShowRawInsn = noShowRawInsn;
}

/**
 *  Setter for m_noShowAddress
 * @param noShowAddress
 */
void ResultsDisassemblyPage::setNoShowAddress(bool noShowAddress) {
    m_noShowAddress = noShowAddress;
}

/**
 *  Setter for m_intelSyntaxDisassembly
 * @param intelSyntax
 */
void ResultsDisassemblyPage::setIntelSyntaxDisassembly(bool intelSyntax) {
    m_intelSyntaxDisassembly = intelSyntax;
}

/**
 *  Setup context menu and connect signals with slots for selected part of Disassembly view copying
 * @param view
 */
void ResultsDisassemblyPage::setupDisassemblyContextMenu(QTreeView *view, int origFontSize) {
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    auto shortcut = new QShortcut(QKeySequence(QLatin1String("Ctrl+C")), view);
    QObject::connect(shortcut, &QShortcut::activated, [view]() {
        ResultsUtil::copySelectedDisassembly(view);
    });

    QObject::connect(view, &QTreeView::customContextMenuRequested, view, [view, origFontSize, this]() {
        QMenu contextMenu;
        auto *copyAction = contextMenu.addAction(QLatin1String("Copy"));
        auto *exportToCSVAction = contextMenu.addAction(QLatin1String("Export to CSV..."));
        auto *zoomInAction = contextMenu.addAction(QLatin1String("Zoom In"));
        auto *zoomOutAction = contextMenu.addAction(QLatin1String("Zoom Out"));

        QObject::connect(copyAction, &QAction::triggered, &contextMenu, [view]() {
            ResultsUtil::copySelectedDisassembly(view);
        });
        QObject::connect(exportToCSVAction, &QAction::triggered, &contextMenu, [view]() {
            ResultsUtil::exportToCSVDisassembly(view);
        });
        QObject::connect(zoomInAction, &QAction::triggered, &contextMenu, [view, origFontSize]() {
            ResultsUtil::zoomFont(view, origFontSize, 4);
        });
        QObject::connect(zoomOutAction, &QAction::triggered, &contextMenu, [view, origFontSize]() {
            ResultsUtil::zoomFont(view, origFontSize, -4);
        });

        if (!m_callStack.isEmpty()) {
            auto *returnToCallerAction = contextMenu.addAction(QLatin1String("Return to Caller"));
            QObject::connect(returnToCallerAction, &QAction::triggered, &contextMenu, [this]() {
                this->returnToCaller();
            });
        }
        contextMenu.exec(QCursor::pos());
    });
}
