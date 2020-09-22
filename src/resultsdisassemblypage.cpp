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

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

#include <QStandardItemModel>

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, PerfParser *parser, QWidget *parent)
        : QWidget(parent), ui(new Ui::ResultsDisassemblyPage) {
    ui->setupUi(this);

    connect(ui->asmView, &QAbstractItemView::doubleClicked, this,
            &ResultsDisassemblyPage::jumpToAsmCallee);
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

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
    if (m_disasmApproach.isEmpty() || m_disasmApproach.startsWith(QLatin1String("address"))) {
        showDisassemblyByAddressRange();
    } else {
        showDisassemblyBySymbol();
    }
}

/**
 *  Produce disassembler with 'objdump' by symbol name and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassemblyBySymbol() {
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
        return;
    }

    // Call objdump with arguments: mangled name of function and binary file
    QString processName =
            m_objdump + QLatin1String(" --disassemble=") + m_curSymbol.mangled + QLatin1String(" ") + m_curSymbol.path;

    showDisassembly(processName);
}

/**
 *  Produce disassembler with 'objdump' by addresses range and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassemblyByAddressRange() {
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
        return;
    }

    // Call objdump with arguments: addresses range and binary file
    QString processName =
            m_objdump + QLatin1String(" -d --start-address=0x") + QString::number(m_curSymbol.relAddr, 16) +
            QLatin1String(" --stop-address=0x") +
            QString::number(m_curSymbol.relAddr + m_curSymbol.size, 16) +
            QLatin1String(" ") + m_curSymbol.path;

    // Workaround for the case when symbol size is equal to zero
    if (m_curSymbol.size == 0) {
        processName =
                m_objdump + QLatin1String(" --disassemble=") + m_curSymbol.mangled + QLatin1String(" ") +
                m_curSymbol.path;
    }
    showDisassembly(processName);
}

/**
 *  Produce disassembler with 'objdump' and output to Disassembly tab
 */
void ResultsDisassemblyPage::showDisassembly(QString processName) {
    QTemporaryFile m_tmpFile;
    QProcess asmProcess;

    if (m_tmpFile.open()) {
        asmProcess.start(processName);

        if (!asmProcess.waitForStarted() || !asmProcess.waitForFinished()) {
            return;
        }
        QTextStream stream(&m_tmpFile);
        stream << asmProcess.readAllStandardOutput();
        m_tmpFile.close();
    }

    if (m_tmpFile.open()) {
        int row = 0;
        model = new QStandardItemModel();

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
                    costItem->setForeground(Qt::red);
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
    // If binary is not found at the specified path, use current binary file located at the application path
    if (!QFile::exists(m_curSymbol.path)) {
        m_curSymbol.path = m_appPath + QDir::separator() + m_curSymbol.binary;
    }
    // If binary is still not found, trying to find it in extraLibPaths
    if (!QFile::exists(m_curSymbol.path)) {
        QStringList dirs = m_extraLibPaths.split(QLatin1String(":"));
        foreach (QString dir, dirs) {
            QDirIterator it(dir, QDir::Dirs, QDirIterator::Subdirectories);

            while (it.hasNext()) {
                QString dirName = it.next();
                QString fileName = dirName + QDir::separator() + m_curSymbol.binary;
                if (QFile::exists(fileName)) {
                    m_curSymbol.path = fileName;
                    break;
                }
            }
        }
    }
}

/**
 * Set path to perf.data
 */
void ResultsDisassemblyPage::setData(const Data::DisassemblyResult &data) {
    m_perfDataPath = data.perfDataPath;
    m_appPath = data.appPath;
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
    showDisassembly();
}
