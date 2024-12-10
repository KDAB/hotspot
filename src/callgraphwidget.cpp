/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "callgraphwidget.h"

#include <utility>

#include <QLabel>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QTextStream>
#include <QVBoxLayout>

#include <KColorScheme>
#include <KParts/ReadOnlyPart>

#include "callgraphgenerator.h"
#include "settings.h"
#include "ui_callgraphwidget.h"
#include "util.h"

#include <kgraphviewer/kgraphviewer_interface.h>

CallgraphWidget::CallgraphWidget(Data::CallerCalleeResults results, KParts::ReadOnlyPart* view,
                                 KGraphViewer::KGraphViewerInterface* interface, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::CallgraphWidget)
    , m_graphFile(new QTemporaryFile(this))
    , m_graphview(view)
    , m_interface(interface)
    , m_callerCalleeResults(std::move(results))
{
    ui->setupUi(this);

    connect(ui->costThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double threshold) {
        m_thresholdPercent = threshold / 100.f; // convert to percent
        generateCallgraph(m_currentSymbol);
    });

    auto oldWidget = layout()->replaceWidget(ui->graphPlaceholder, view->widget());
    delete oldWidget;

    updateColors();
    m_interface->setLayoutMethod(KGraphViewer::KGraphViewerInterface::LayoutMethod::InternalLibrary);

    connect(m_graphview, SIGNAL(hoverEnter(QString)), // clazy:exclude=old-style-connect
            this, SLOT(hoverEnter(QString)));
    connect(m_graphview, SIGNAL(hoverLeave(QString)), // clazy:exclude=old-style-connect
            this, SLOT(hoverLeave(QString)));

    // used to find mouseclicks in the kgraphviewer part
    qApp->installEventFilter(this);

    m_graphFile->open();

    connect(Settings::instance(), &Settings::callgraphChanged, this, [this] { generateCallgraph(m_currentSymbol); });
}

CallgraphWidget::~CallgraphWidget() = default;

CallgraphWidget* CallgraphWidget::createCallgraphWidget(const Data::CallerCalleeResults& results, QWidget* parent)
{
    auto part = Util::createPart(QStringLiteral("kgraphviewerpart"));
    if (!part) {
        return nullptr;
    }

    auto interface = qobject_cast<KGraphViewer::KGraphViewerInterface*>(part);
    if (!interface) {
        return nullptr;
    }

    return new CallgraphWidget(results, part, interface, parent);
}

void CallgraphWidget::selectSymbol(const Data::Symbol& symbol)
{
    if (symbol == m_currentSymbol) {
        return;
    }

    generateCallgraph(symbol);
}

void CallgraphWidget::setResults(const Data::CallerCalleeResults& results)
{
    m_callerCalleeResults = results;
    selectSymbol(m_currentSymbol);
}

void CallgraphWidget::hoverEnter(const QString& node)
{
    // cut of the node prefix
    m_currentNode = node.mid(4);
}

void CallgraphWidget::hoverLeave(const QString& node)
{
    Q_UNUSED(node);
    m_currentNode = QString();
}

bool CallgraphWidget::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (!isVisible())
        return false;

    if (event->type() == QEvent::MouseButtonPress) {
        const auto e = static_cast<QMouseEvent*>(event);

        if (m_graphview->widget()->geometry().contains(e->pos())) {
            if (e->button() == Qt::MouseButton::LeftButton && !m_currentNode.isEmpty()) {
                const auto symbol = m_symbolToId.key(m_currentNode);
                if (symbol.isValid()) {
                    emit clickedOn(symbol);
                    m_currentNode.clear();
                    return true;
                }
            }
        }
    }
    return false;
}

void CallgraphWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::PaletteChange) {
        updateColors();
    }
}

void CallgraphWidget::showEvent(QShowEvent* event)
{
    Q_UNUSED(event);
    if (!m_graphview) {
        return;
    }

    m_graphview->openUrl(QUrl::fromLocalFile(m_graphFile->fileName()));
}

void CallgraphWidget::generateCallgraph(const Data::Symbol& symbol)
{
    if (!m_graphview) {
        return;
    }

    m_graphview->closeUrl();
    m_graphFile->resize(0);
    m_currentSymbol = symbol;

    QTextStream stream(m_graphFile);
    m_symbolToId = writeGraph(stream, symbol, m_callerCalleeResults, m_thresholdPercent, m_fontColor);
    stream.flush();

    // if openUrl is called before the window is open it will freeze the application
    if (isVisible()) {
        m_graphview->openUrl(QUrl::fromLocalFile(m_graphFile->fileName()));
    }
}

void CallgraphWidget::updateColors()
{
    const auto scheme = KColorScheme(palette().currentColorGroup());
    m_interface->setBackgroundColor(scheme.background(KColorScheme::NormalBackground).color());
    m_fontColor = scheme.foreground().color().name();
}
