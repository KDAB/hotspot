/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include "data.h"

namespace KParts {
class ReadOnlyPart;
}

namespace KGraphViewer {
class KGraphViewerInterface;
}

class QTemporaryFile;
class QTextStream;

class CallerModel;
class CalleeModel;

namespace Ui {
class CallgraphWidget;
}

class CallgraphWidget : public QWidget
{
    Q_OBJECT
public:
    ~CallgraphWidget();

    static CallgraphWidget* createCallgraphWidget(const Data::CallerCalleeResults& results, QWidget* parent = nullptr);

    void selectSymbol(const Data::Symbol& symbol);

signals:
    void clickedOn(const Data::Symbol& symbol);

public slots:
    void setResults(const Data::CallerCalleeResults& results);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void hoverEnter(const QString& node);
    void hoverLeave(const QString& node);

private:
    CallgraphWidget(const Data::CallerCalleeResults& results, KParts::ReadOnlyPart* view,
                    KGraphViewer::KGraphViewerInterface* interface, QWidget* parent = nullptr);

    void generateCallgraph(const Data::Symbol& symbol);
    void updateColors();

    std::unique_ptr<Ui::CallgraphWidget> ui;
    double m_thresholdPercent = 0.1 / 100.f;
    QTemporaryFile* m_graphFile = nullptr;
    KParts::ReadOnlyPart* m_graphview = nullptr;
    KGraphViewer::KGraphViewerInterface* m_interface = nullptr;
    Data::CallerCalleeResults m_callerCalleeResults;
    QHash<Data::Symbol, QString> m_symbolToId;
    Data::Symbol m_currentSymbol;
    QString m_currentNode;
    QString m_fontColor;
    bool m_callgraphShown = false;
};
