#pragma once

#include <QWidget>
#include <QTemporaryFile>
#include "data.h"

class QMenu;

namespace Ui {
class ResultsDisassemblyPage;
}

namespace Data {
struct Symbol;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;

class ResultsDisassemblyPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsDisassemblyPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                 QWidget* parent = nullptr);
    ~ResultsDisassemblyPage();

    void clear();
    void showDisassembly();
    void showDisassemblyBySymbol();
    void showDisassemblyByAddressRange();
    // Output Disassembly that is the result of call process running 'processName' command on tab Disassembly
    void showDisassembly(QString processName);
    void setAppPath(const QString& path);
    void setData(const Data::Symbol& data);
    void setData(const Data::DisassemblyResult& data);

private:
    QScopedPointer<Ui::ResultsDisassemblyPage> ui;
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
    // Disassembly approach code: 'symbol' - by function symbol, 'address' or default - by addresses range
    QString m_disasmApproach;
    // Objdump binary name
    QString m_objdump;
};
