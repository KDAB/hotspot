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

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, PerfParser *parser, QWidget *parent)
        : QWidget(parent), ui(new Ui::ResultsDisassemblyPage), m_noShowRawInsn(true), m_noShowAddress(false), m_intelSyntaxDisassembly(false) {
    ui->setupUi(this);

    ui->searchTextEdit->setPlaceholderText(QLatin1String("Search"));
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    ResultsUtil::setupDisassemblyContextMenu(ui->asmView);
    connect(ui->asmView, &QAbstractItemView::doubleClicked, this, &ResultsDisassemblyPage::jumpToAsmCallee);
    m_origFontSize = this->font().pointSize();
    m_filterAndZoomStack = filterStack;

    m_searchDelegate = new SearchDelegate(ui->asmView);
    ui->asmView->setItemDelegate(m_searchDelegate);

    connect(ui->searchTextEdit, &QTextEdit::textChanged, this, &ResultsDisassemblyPage::searchTextAndHighlight);

    connect(ui->asmView, &QAbstractItemView::clicked, this, &ResultsDisassemblyPage::onItemClicked);

    auto shortcut = new QShortcut(QKeySequence(QLatin1String("Ctrl+A")), ui->asmView);
    QObject::connect(shortcut, &QShortcut::activated, [this]() {
        this->selectAll();
    });
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

/**
 *  Select all handler
 */
void ResultsDisassemblyPage::selectAll()
{
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    ui->asmView->selectAll();
    m_searchDelegate->setSelectedIndexes(ui->asmView->selectionModel()->selectedIndexes());
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

/**
 *  Select one item handler
 * @param index
 */
void ResultsDisassemblyPage::onItemClicked(const QModelIndex &index)
{
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    m_searchDelegate->setSelectedIndexes(ui->asmView->selectionModel()->selectedIndexes());
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

/**
 *  Search text (taken from text editor Search) in Disassembly output and highlight found.
 */
void ResultsDisassemblyPage::searchTextAndHighlight() {
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
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
    QFont curFont = this->font();
    curFont.setPointSize(curFont.pointSize() + event->delta() / 100);

    int fontSize = (curFont.pointSize() / (double) m_origFontSize) * 100;
    this->setFont(curFont);
    this->setToolTip(QLatin1String("Zoom: ") + QString::number(fontSize) + QLatin1String("%"));

    QFont textEditFont = curFont;
    textEditFont.setPointSize(m_origFontSize);
    ui->searchTextEdit->setFont(textEditFont);
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
    setNoShowRawInsn(filtered);
    resetDisassembly();
}

/**
 *  Hide instruction address when an argument is true
 * @param filtered
 */
void ResultsDisassemblyPage::filterDisassemblyAddress(bool filtered) {
    setNoShowAddress(filtered);
    resetDisassembly();
}

/**
 *
 * @param intelSyntax
 */
void ResultsDisassemblyPage::switchOnIntelSyntax(bool intelSyntax) {
    setIntelSyntaxDisassembly(intelSyntax);
    resetDisassembly();
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
 *  Set model to asmView and resize it's columns
 * @param model
 * @param numTypes
 */
void ResultsDisassemblyPage::setAsmViewModel(QStandardItemModel *model, int numTypes) {
    ui->asmView->setModel(model);
    ui->asmView->header()->setStretchLastSection(false);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
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
void ResultsDisassemblyPage::getObjdumpVersion(QByteArray &processOutput) {
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
            processOutput = QByteArray("Version of objdump should be >= 2.32. You use objdump with version ") +
                            m_objdumpVersion.toUtf8();
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

            if (m_objdumpVersion.isEmpty()) {
                getObjdumpVersion(processOutput);
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
        model = new DisassemblyModel();

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
}

/**
 * Clear call stack when another symbol was selected
 */
void ResultsDisassemblyPage::resetCallStack() {
    m_callStack.clear();
}

/**
 *  Auxilary method to extract relative address of selected 'call' instruction and it's name
 */
void calcFunction(QString asmLineCall, QString *offset, QString *symName) {
    QStringList sym = asmLineCall.trimmed().split(QLatin1Char('<'));
    *offset = sym[0].trimmed();
    *symName = sym[1].replace(QLatin1Char('>'), QLatin1String(""));
}

/**
 *  Slot method for double click on 'call' or 'return' instructions
 *  We show it's Disassembly by double click on 'call' instruction
 *  And we go back by double click on 'return'
 */
void ResultsDisassemblyPage::jumpToAsmCallee(QModelIndex index) {
    QStandardItem *asmItem = model->takeItem(index.row(), 0);
    QString opCodeCall = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("bl") : QLatin1String("callq");
    QString opCodeReturn = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("ret") : QLatin1String("retq");
    QString asmLine = asmItem->text();
    if (asmLine.contains(opCodeCall)) {
        QString offset, symName;
        QString asmLineSimple = asmLine.simplified();

        calcFunction(asmLineSimple.section(opCodeCall, 1), &offset, &symName);

        QHash<Data::Symbol, Data::DisassemblyEntry>::iterator i = m_disasmResult.entries.begin();
        while (i != m_disasmResult.entries.end()) {
            QString relAddr = QString::number(i.key().relAddr, 16);
            if (!i.key().mangled.isEmpty() &&
                (symName.contains(i.key().mangled) || symName.contains(i.key().symbol) ||
                 i.key().mangled.contains(symName) || i.key().symbol.contains(symName)) &&
                ((relAddr == offset) ||
                 (i.key().size == 0 && i.key().relAddr == 0)))
            {
                m_callStack.push(m_curSymbol);
                setData(i.key());
                break;
            }
            i++;
        }
    }
    if (asmLine.contains(opCodeReturn)) {
        if (!m_callStack.isEmpty()) {
            setData(m_callStack.pop());
        }
    }
    resetDisassembly();
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
