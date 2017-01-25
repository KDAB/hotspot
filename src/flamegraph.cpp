/*
 * Copyright 2015-2017 Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "flamegraph.h"

#include <cmath>

#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QStyleOption>
#include <QGraphicsView>
#include <QLabel>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include <QEvent>
#include <QToolTip>
#include <QDebug>
#include <QAction>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QCursor>

#include <ThreadWeaver/ThreadWeaver>
#include <KLocalizedString>
#include <KColorScheme>

enum CostType
{
    Samples
};
Q_DECLARE_METATYPE(CostType)

class FrameGraphicsItem : public QGraphicsRectItem
{
public:
    FrameGraphicsItem(const qint64 cost, CostType costType, const QString& function, FrameGraphicsItem* parent = nullptr);
    FrameGraphicsItem(const qint64 cost, const QString& function, FrameGraphicsItem* parent);

    qint64 cost() const;
    void setCost(qint64 cost);
    QString function() const;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    QString description() const;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    qint64 m_cost;
    QString m_function;
    CostType m_costType;
    bool m_isHovered;
};

Q_DECLARE_METATYPE(FrameGraphicsItem*)

FrameGraphicsItem::FrameGraphicsItem(const qint64 cost, CostType costType, const QString& function, FrameGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_cost(cost)
    , m_function(function)
    , m_costType(costType)
    , m_isHovered(false)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

FrameGraphicsItem::FrameGraphicsItem(const qint64 cost, const QString& function, FrameGraphicsItem* parent)
    : FrameGraphicsItem(cost, parent->m_costType, function, parent)
{
}

qint64 FrameGraphicsItem::cost() const
{
    return m_cost;
}

void FrameGraphicsItem::setCost(qint64 cost)
{
    m_cost = cost;
}

QString FrameGraphicsItem::function() const
{
    return m_function;
}

void FrameGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    if (isSelected() || m_isHovered) {
        auto selectedColor = brush().color();
        selectedColor.setAlpha(255);
        painter->fillRect(rect(), selectedColor);
    } else {
        painter->fillRect(rect(), brush());
    }

    const QPen oldPen = painter->pen();
    auto pen = oldPen;
    pen.setColor(brush().color());
    if (isSelected()) {
        pen.setWidth(2);
    }
    painter->setPen(pen);
    painter->drawRect(rect());
    painter->setPen(oldPen);

    const int margin = 4;
    const int width = rect().width() - 2 * margin;
    if (width < option->fontMetrics.averageCharWidth() * 6) {
        // text is too wide for the current LOD, don't paint it
        return;
    }

    const int height = rect().height();

    painter->drawText(margin + rect().x(), rect().y(), width, height, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      option->fontMetrics.elidedText(m_function, Qt::ElideRight, width));
}

void FrameGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsRectItem::hoverEnterEvent(event);
    m_isHovered = true;
}

QString FrameGraphicsItem::description() const
{
    // we build the tooltip text on demand, which is much faster than doing that for potentially thousands of items when we load the data
    QString tooltip;
    qint64 totalCost = 0;
    {
        auto item = this;
        while (item->parentItem()) {
            item = static_cast<const FrameGraphicsItem*>(item->parentItem());
        }
        totalCost = item->cost();
    }
    const auto fraction = QString::number(double(m_cost)  * 100. / totalCost, 'g', 3);
    const auto function = QString(QLatin1String("<span style='font-family:monospace'>") + m_function.toHtmlEscaped() + QLatin1String("</span>"));
    if (!parentItem()) {
        return function;
    }

    switch (m_costType) {
    case Samples:
        tooltip = i18nc("%1: number of samples, %2: relative number, %3: function label",
                        "%1 (%2%) samples in %3 and below.", m_cost, fraction, function);
        break;
    }

    return tooltip;
}

void FrameGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsRectItem::hoverLeaveEvent(event);
    m_isHovered = false;
}

namespace {

/**
 * Generate a brush from the "mem" color space used in upstream FlameGraph.pl
 */
QBrush brush()
{
    // intern the brushes, to reuse them across items which can be thousands
    // otherwise we'd end up with dozens of allocations and higher memory consumption
    static QVector<QBrush> brushes;
    if (brushes.isEmpty()) {
        std::generate_n(std::back_inserter(brushes), 100, [] () {
            return QColor(0, 190 + 50 * qreal(rand()) / RAND_MAX, 210 * qreal(rand()) / RAND_MAX, 125);
        });
    }
    return brushes.at(rand() % brushes.size());
}

/**
 * Layout the flame graph and hide tiny items.
 */
void layoutItems(FrameGraphicsItem *parent)
{
    const auto& parentRect = parent->rect();
    const auto pos = parentRect.topLeft();
    const qreal maxWidth = parentRect.width();
    const qreal h = parentRect.height();
    const qreal y_margin = 2.;
    const qreal y = pos.y() - h - y_margin;
    qreal x = pos.x();

    foreach (auto child, parent->childItems()) {
        auto frameChild = static_cast<FrameGraphicsItem*>(child);
        const qreal w = maxWidth * double(frameChild->cost()) / parent->cost();
        frameChild->setVisible(w > 1);
        if (frameChild->isVisible()) {
            frameChild->setRect(QRectF(x, y, w, h));
            layoutItems(frameChild);
            x += w;
        }
    }
}

FrameGraphicsItem* findItemByFunction(const QList<QGraphicsItem*>& items, const QString& function)
{
    foreach (auto item_, items) {
        auto item = static_cast<FrameGraphicsItem*>(item_);
        if (item->function() == function) {
            return item;
        }
    }
    return nullptr;
}

/**
 * Convert the top-down graph into a tree of FrameGraphicsItem.
 */
void toGraphicsItems(const QVector<FrameData>& data, FrameGraphicsItem *parent,
                     const double costThreshold, bool collapseRecursion)
{
    foreach (const auto& row, data) {
        if (collapseRecursion && row.symbol == parent->function()) {
            continue;
        }
        auto item = findItemByFunction(parent->childItems(), row.symbol);
        if (!item) {
            item = new FrameGraphicsItem(row.inclusiveCost, row.symbol, parent);
            item->setPen(parent->pen());
            item->setBrush(brush());
        } else {
            item->setCost(item->cost() + row.inclusiveCost);
        }
        if (item->cost() > costThreshold) {
            toGraphicsItems(row.children, item, costThreshold, collapseRecursion);
        }
    }
}

FrameGraphicsItem* parseData(const QVector<FrameData>& topDownData, CostType type,
                             double costThreshold, bool collapseRecursion)
{
    double totalCost = 0;
    foreach(const auto& frame, topDownData) {
        totalCost += frame.inclusiveCost;
    }

    KColorScheme scheme(QPalette::Active);
    const QPen pen(scheme.foreground().color());

    QString label;
    switch (type) {
    case Samples:
        label = i18n("%1 samples in total", totalCost);
        break;
    }
    auto rootItem = new FrameGraphicsItem(totalCost, type, label);
    rootItem->setBrush(scheme.background());
    rootItem->setPen(pen);
    toGraphicsItems(topDownData, rootItem,
                    totalCost * costThreshold / 100., collapseRecursion);
    return rootItem;
}

}

FlameGraph::FlameGraph(QWidget* parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_costSource(new QComboBox(this))
    , m_scene(new QGraphicsScene(this))
    , m_view(new QGraphicsView(this))
    , m_displayLabel(new QLabel)
{
    qRegisterMetaType<FrameGraphicsItem*>();

    m_costSource->addItem(i18n("Samples"), QVariant::fromValue(Samples));
    m_costSource->setItemData(0, i18n("Show a flame graph over the number of samples triggered by functions in your code."), Qt::ToolTipRole);

    connect(m_costSource, static_cast<void (QComboBox::*) (int)>(&QComboBox::currentIndexChanged),
            this, &FlameGraph::showData);
    m_costSource->setToolTip(i18n("Select the data source that should be visualized in the flame graph."));

    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_view->setScene(m_scene);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);
    m_view->setFont(QFont(QStringLiteral("monospace")));
    m_view->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto bottomUpCheckbox = new QCheckBox(i18n("Bottom-Down View"), this);
    bottomUpCheckbox->setToolTip(i18n("Enable the bottom-down flame graph view. When this is unchecked, the top-down view is enabled by default."));
    bottomUpCheckbox->setChecked(m_showBottomUpData);
    bottomUpCheckbox->hide(); // Hide this checkbox until the top-down feature is implemented
    connect(bottomUpCheckbox, &QCheckBox::toggled, this, [this, bottomUpCheckbox] {
        m_showBottomUpData = bottomUpCheckbox->isChecked();
        showData();
    });

    auto collapseRecursionCheckbox = new QCheckBox(i18n("Collapse Recursion"), this);
    collapseRecursionCheckbox->setChecked(m_collapseRecursion);
    collapseRecursionCheckbox->setToolTip(i18n("Collapse stack frames for functions calling themselves. "
                                               "When this is unchecked, recursive frames will be visualized separately."));
    connect(collapseRecursionCheckbox, &QCheckBox::toggled, this, [this, collapseRecursionCheckbox] {
        m_collapseRecursion = collapseRecursionCheckbox->isChecked();
        showData();
    });

    auto costThreshold = new QDoubleSpinBox(this);
    costThreshold->setDecimals(2);
    costThreshold->setMinimum(0);
    costThreshold->setMaximum(99.90);
    costThreshold->setPrefix(i18n("Cost Threshold: "));
    costThreshold->setSuffix(QStringLiteral("%"));
    costThreshold->setValue(m_costThreshold);
    costThreshold->setSingleStep(0.01);
    costThreshold->setToolTip(i18n("<qt>The cost threshold defines a fractional cut-off value. "
                                   "Items with a relative cost below this value will not be shown in the flame graph. "
                                   "This is done as an optimization to quickly generate graphs for large data sets with "
                                   "low memory overhead. If you need more details, decrease the threshold value, or set it to zero.</qt>"));
    connect(costThreshold, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
            this, [this] (double threshold) {
                m_costThreshold = threshold;
                showData();
            });

    m_displayLabel->setWordWrap(true);
    m_displayLabel->setTextInteractionFlags(m_displayLabel->textInteractionFlags() | Qt::TextSelectableByMouse);

    auto controls = new QWidget(this);
    controls->setLayout(new QHBoxLayout);
    controls->layout()->addWidget(m_costSource);
    controls->layout()->addWidget(bottomUpCheckbox);
    controls->layout()->addWidget(collapseRecursionCheckbox);
    controls->layout()->addWidget(costThreshold);

    setLayout(new QVBoxLayout);
    layout()->addWidget(controls);
    layout()->addWidget(m_view);
    layout()->addWidget(m_displayLabel);

    {
        auto action = new QAction(tr("back"), this);
        action->setShortcuts({QKeySequence::Back, Qt::Key_Backspace});
        connect(action, &QAction::triggered,
                this, &FlameGraph::navigateBack);
        addAction(action);
    }
    {
        auto action = new QAction(tr("forward"), this);
        action->setShortcuts(QKeySequence::Forward);
        connect(action, &QAction::triggered,
                this, &FlameGraph::navigateForward);
        addAction(action);
    }
}

FlameGraph::~FlameGraph() = default;

bool FlameGraph::eventFilter(QObject* object, QEvent* event)
{
    bool ret = QObject::eventFilter(object, event);

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            auto item = static_cast<FrameGraphicsItem*>(m_view->itemAt(mouseEvent->pos()));
            if (item && item != m_selectionHistory.at(m_selectedItem)) {
                selectItem(item);
                if (m_selectedItem != m_selectionHistory.size() - 1) {
                    m_selectionHistory.remove(m_selectedItem + 1, m_selectionHistory.size() - m_selectedItem - 1);
                }
                m_selectedItem = m_selectionHistory.size();
                m_selectionHistory.push_back(item);
            }
        }
    } else if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        auto item = static_cast<FrameGraphicsItem*>(m_view->itemAt(mouseEvent->pos()));
        setTooltipItem(item);
    } else if (event->type() == QEvent::Leave) {
        setTooltipItem(nullptr);
    } else if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
        if (!m_rootItem) {
            showData();
        } else {
            selectItem(m_selectionHistory.at(m_selectedItem));
        }
        updateTooltip();
    } else if (event->type() == QEvent::Hide) {
        setData(nullptr);
    }
    return ret;
}

void FlameGraph::setTopDownData(const FrameData& topDownData)
{
    m_topDownData = topDownData;

    if (isVisible()) {
        showData();
    }
}

void FlameGraph::setBottomUpData(const FrameData& bottomUpData)
{
    m_bottomUpData = bottomUpData;
}

void FlameGraph::showData()
{
    setData(nullptr);

    using namespace ThreadWeaver;
    auto data = m_showBottomUpData ? m_bottomUpData.children : m_topDownData.children;
    bool collapseRecursion = m_collapseRecursion;
    auto source = m_costSource->currentData().value<CostType>();
    auto threshold = m_costThreshold;
    stream() << make_job([data, source, threshold, collapseRecursion, this]() {
        auto parsedData = parseData(data, source, threshold, collapseRecursion);
        QMetaObject::invokeMethod(this, "setData", Qt::QueuedConnection,
                                  Q_ARG(FrameGraphicsItem*, parsedData));
    });
}

void FlameGraph::setTooltipItem(const FrameGraphicsItem* item)
{
    if (!item && m_selectedItem != -1 && m_selectionHistory.at(m_selectedItem)) {
        item = m_selectionHistory.at(m_selectedItem);
        m_view->setCursor(Qt::ArrowCursor);
    } else {
        m_view->setCursor(Qt::PointingHandCursor);
    }
    m_tooltipItem = item;
    updateTooltip();
}

void FlameGraph::updateTooltip()
{
    const auto text = m_tooltipItem ? m_tooltipItem->description() : QString();
    m_displayLabel->setToolTip(text);
    const auto metrics = m_displayLabel->fontMetrics();
    // FIXME: the HTML text has tons of stuff that is not printed,
    //        which lets the text get cut-off too soon...
    m_displayLabel->setText(metrics.elidedText(text, Qt::ElideRight, m_displayLabel->width()));
}

void FlameGraph::setData(FrameGraphicsItem* rootItem)
{
    m_scene->clear();
    m_rootItem = rootItem;
    m_selectionHistory.clear();
    m_selectionHistory.push_back(rootItem);
    m_selectedItem = 0;
    if (!rootItem) {
        auto text = m_scene->addText(i18n("generating flame graph..."));
        m_view->centerOn(text);
        m_view->setCursor(Qt::BusyCursor);
        return;
    }

    m_view->setCursor(Qt::ArrowCursor);
    // layouting needs a root item with a given height, the rest will be overwritten later
    rootItem->setRect(0, 0, 800, m_view->fontMetrics().height() + 4);
    m_scene->addItem(rootItem);

    if (isVisible()) {
        selectItem(m_rootItem);
    }
}

void FlameGraph::selectItem(FrameGraphicsItem* item)
{
    if (!item) {
        return;
    }

    // scale item and its parents to the maximum available width
    // also hide all siblings of the parent items
    const auto rootWidth = m_view->viewport()->width() - 40;
    auto parent = item;
    while (parent) {
        auto rect = parent->rect();
        rect.setLeft(0);
        rect.setWidth(rootWidth);
        parent->setRect(rect);
        if (parent->parentItem()) {
            foreach (auto sibling, parent->parentItem()->childItems()) {
                sibling->setVisible(sibling == parent);
            }
        }
        parent = static_cast<FrameGraphicsItem*>(parent->parentItem());
    }

    // then layout all items below the selected on
    layoutItems(item);

    // and make sure it's visible
    m_view->centerOn(item);

    setTooltipItem(item);
}

void FlameGraph::navigateBack()
{
    if (m_selectedItem > 0) {
        --m_selectedItem;
    }
    selectItem(m_selectionHistory.at(m_selectedItem));
}

void FlameGraph::navigateForward()
{
    if ((m_selectedItem + 1) < m_selectionHistory.size()) {
        ++m_selectedItem;
    }
    selectItem(m_selectionHistory.at(m_selectedItem));
}
