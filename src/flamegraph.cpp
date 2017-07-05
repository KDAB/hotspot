/*
  flamegraph.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

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
#include <QMenu>
#include <QLineEdit>

#include <ThreadWeaver/ThreadWeaver>
#include <KLocalizedString>
#include <KColorScheme>
#include <KStandardAction>

namespace {
QString fraction(qint64 cost, qint64 totalCost)
{
    return QString::number(double(cost)  * 100. / totalCost, 'g', 3);
}

enum SearchMatchType
{
    NoSearch,
    NoMatch,
    DirectMatch,
    ChildMatch
};
}
class FrameGraphicsItem : public QGraphicsRectItem
{
public:
    FrameGraphicsItem(const qint64 cost, const Data::Symbol& symbol, FrameGraphicsItem* parent = nullptr);

    qint64 cost() const;
    void setCost(qint64 cost);
    Data::Symbol symbol() const;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    QString description() const;
    void setSearchMatchType(SearchMatchType matchType);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    qint64 m_cost;
    Data::Symbol m_symbol;
    bool m_isHovered;
    SearchMatchType m_searchMatch = NoSearch;
};

Q_DECLARE_METATYPE(FrameGraphicsItem*)

FrameGraphicsItem::FrameGraphicsItem(const qint64 cost, const Data::Symbol& symbol, FrameGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_cost(cost)
    , m_symbol(symbol)
    , m_isHovered(false)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

qint64 FrameGraphicsItem::cost() const
{
    return m_cost;
}

void FrameGraphicsItem::setCost(qint64 cost)
{
    m_cost = cost;
}

Data::Symbol FrameGraphicsItem::symbol() const
{
    return m_symbol;
}

void FrameGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    if (isSelected() || m_isHovered || m_searchMatch == DirectMatch) {
        auto selectedColor = brush().color();
        selectedColor.setAlpha(255);
        painter->fillRect(rect(), selectedColor);
    } else if (m_searchMatch == NoMatch) {
        auto noMatchColor = brush().color();
        noMatchColor.setAlpha(50);
        painter->fillRect(rect(), noMatchColor);
    } else { // default, when no search is running, or a sub-item is matched
        painter->fillRect(rect(), brush());
    }

    const QPen oldPen = painter->pen();
    auto pen = oldPen;
    if (m_searchMatch != NoMatch) {
        pen.setColor(brush().color());
        if (isSelected()) {
            pen.setWidth(2);
        }
        painter->setPen(pen);
        painter->drawRect(rect());
        painter->setPen(oldPen);
    }

    const int margin = 4;
    const int width = rect().width() - 2 * margin;
    if (width < option->fontMetrics.averageCharWidth() * 6) {
        // text is too wide for the current LOD, don't paint it
        return;
    }

    if (m_searchMatch == NoMatch) {
        auto color = oldPen.color();
        color.setAlpha(125);
        pen.setColor(color);
        painter->setPen(pen);
    }

    const int height = rect().height();
    const auto symbol = m_symbol.symbol.isEmpty() ? QObject::tr("??") : m_symbol.symbol;
    painter->drawText(margin + rect().x(), rect().y(), width, height, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      option->fontMetrics.elidedText(symbol, Qt::ElideRight, width));

    if (m_searchMatch == NoMatch) {
        painter->setPen(oldPen);
    }
}

void FrameGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsRectItem::hoverEnterEvent(event);
    m_isHovered = true;
    update();
}

void FrameGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsRectItem::hoverLeaveEvent(event);
    m_isHovered = false;
    update();
}

QString FrameGraphicsItem::description() const
{
    // we build the tooltip text on demand, which is much faster than doing that for potentially thousands of items when we load the data
    qint64 totalCost = 0;
    {
        auto item = this;
        while (item->parentItem()) {
            item = static_cast<const FrameGraphicsItem*>(item->parentItem());
        }
        totalCost = item->cost();
    }
    const auto symbol = m_symbol.symbol.isEmpty() ? QObject::tr("??") : m_symbol.symbol;
    if (!parentItem()) {
        return symbol;
    }

    return i18nc("%1: aggregated sample costs, %2: relative number, %3: function label, %4: binary",
        "%1 (%2%) aggregated sample costs in %3 (%4) and below.", m_cost, fraction(m_cost, totalCost), symbol, m_symbol.binary);
}

void FrameGraphicsItem::setSearchMatchType(SearchMatchType matchType)
{
    if (m_searchMatch != matchType) {
        m_searchMatch = matchType;
        update();
    }
}

namespace {

/**
 * Generate a brush from the "mem" color space used in upstream flamegraph.pl
 */
Q_DECL_UNUSED QBrush memBrush()
{
    // intern the brushes, to reuse them across items which can be thousands
    // otherwise we'd end up with dozens of allocations and higher memory consumption
    static const QVector<QBrush> brushes = ([] () -> QVector<QBrush> {
        QVector<QBrush> ret;
        std::generate_n(std::back_inserter(ret), 100, [] () {
            return QColor(0, 190 + 50 * qreal(rand()) / RAND_MAX, 210 * qreal(rand()) / RAND_MAX, 125);
        });
        return ret;
    }) ();
    return brushes.at(rand() % brushes.size());
}

/**
 * Generate a brush from the "hot" color space used in upstream flamegraph.pl
 */
QBrush hotBrush()
{
    // intern the brushes, to reuse them across items which can be thousands
    // otherwise we'd end up with dozens of allocations and higher memory consumption
    static const QVector<QBrush> brushes = ([] () -> QVector<QBrush> {
        QVector<QBrush> ret;
        std::generate_n(std::back_inserter(ret), 100, [] () {
            return QColor(205 + 50 * qreal(rand()) / RAND_MAX, 230 * qreal(rand()) / RAND_MAX, 55 * qreal(rand()) / RAND_MAX, 125);
        });
        return ret;
    }) ();
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

    auto children = parent->childItems();
    // sort to get reproducible graphs
    std::sort(children.begin(), children.end(),
              [](const QGraphicsItem* lhs, const QGraphicsItem* rhs) {
                  return static_cast<const FrameGraphicsItem*>(lhs)->symbol()
                       < static_cast<const FrameGraphicsItem*>(rhs)->symbol();
              });

    foreach (auto child, children) {
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

FrameGraphicsItem* findItemBySymbol(const QList<QGraphicsItem*>& items, const Data::Symbol& symbol)
{
    foreach (auto item_, items) {
        auto item = static_cast<FrameGraphicsItem*>(item_);
        if (item->symbol() == symbol) {
            return item;
        }
    }
    return nullptr;
}

/**
 * Convert the top-down graph into a tree of FrameGraphicsItem.
 */
template<typename Tree>
void toGraphicsItems(const Data::Costs& costs, int type, const QVector<Tree>& data, FrameGraphicsItem *parent,
                     const double costThreshold, bool collapseRecursion)
{
    foreach (const auto& row, data) {
        if (collapseRecursion && !row.symbol.symbol.isEmpty() && row.symbol == parent->symbol()) {
            if (costs.cost(type, row.id) > costThreshold) {
                toGraphicsItems(costs, type, row.children, parent, costThreshold, collapseRecursion);
            }
            continue;
        }
        auto item = findItemBySymbol(parent->childItems(), row.symbol);
        if (!item) {
            item = new FrameGraphicsItem(costs.cost(type, row.id), row.symbol, parent);
            item->setPen(parent->pen());
            item->setBrush(hotBrush());
        } else {
            item->setCost(item->cost() + costs.cost(type, row.id));
        }
        if (item->cost() > costThreshold) {
            toGraphicsItems(costs, type, row.children, item, costThreshold, collapseRecursion);
        }
    }
}

template<typename Tree>
FrameGraphicsItem* parseData(const Data::Costs& costs, int type, const QVector<Tree>& topDownData,
                             double costThreshold, bool collapseRecursion)
{
    double totalCost = costs.totalCost(type);

    KColorScheme scheme(QPalette::Active);
    const QPen pen(scheme.foreground().color());

    QString label = i18n("%1 aggregated sample costs in total", totalCost);
    auto rootItem = new FrameGraphicsItem(totalCost, {label, {}});
    rootItem->setBrush(scheme.background());
    rootItem->setPen(pen);
    toGraphicsItems(costs, type, topDownData, rootItem,
                    totalCost * costThreshold / 100., collapseRecursion);
    return rootItem;
}

struct SearchResults
{
    SearchMatchType matchType = NoMatch;
    qint64 directCost = 0;
};

SearchResults applySearch(FrameGraphicsItem* item, const QString& searchValue)
{
    SearchResults result;
    if (searchValue.isEmpty()) {
        result.matchType = NoSearch;
    } else if (item->symbol().symbol.contains(searchValue, Qt::CaseInsensitive)
                || (searchValue == QLatin1String("??") && item->symbol().symbol.isEmpty())
                || item->symbol().binary.contains(searchValue, Qt::CaseInsensitive))
    {
        result.directCost += item->cost();
        result.matchType = DirectMatch;
    }

    // recurse into the child items, we always need to update all items
    for (auto* child : item->childItems()) {
        auto* childFrame = static_cast<FrameGraphicsItem*>(child);
        auto childMatch = applySearch(childFrame, searchValue);
        if (result.matchType != DirectMatch &&
            (childMatch.matchType == DirectMatch || childMatch.matchType == ChildMatch))
        {
            result.matchType = ChildMatch;
            result.directCost += childMatch.directCost;
        }
    }

    item->setSearchMatchType(result.matchType);
    return result;
}

}

FlameGraph::FlameGraph(QWidget* parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_costSource(new QComboBox(this))
    , m_scene(new QGraphicsScene(this))
    , m_view(new QGraphicsView(this))
    , m_displayLabel(new QLabel)
    , m_searchResultsLabel(new QLabel)
{
    qRegisterMetaType<FrameGraphicsItem*>();

    m_costSource->setToolTip(i18n("Select the data source that should be visualized in the flame graph."));

    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_view->setScene(m_scene);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);
    m_view->setFont(QFont(QStringLiteral("monospace")));

    auto bottomUpCheckbox = new QCheckBox(i18n("Bottom-Up View"), this);
    bottomUpCheckbox->setToolTip(i18n("Enable the bottom-up flame graph view. When this is unchecked, the top-down view is enabled by default."));
    bottomUpCheckbox->setChecked(m_showBottomUpData);
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

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(i18n("Search..."));
    m_searchInput->setToolTip(i18n("<qt>Search the flame graph for a symbol.</qt>"));
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::textChanged,
            this, &FlameGraph::setSearchValue);

    auto controls = new QWidget(this);
    controls->setLayout(new QHBoxLayout);
    controls->layout()->addWidget(m_costSource);
    controls->layout()->addWidget(bottomUpCheckbox);
    controls->layout()->addWidget(collapseRecursionCheckbox);
    controls->layout()->addWidget(costThreshold);
    controls->layout()->addWidget(m_searchInput);

    m_displayLabel->setWordWrap(true);
    m_displayLabel->setTextInteractionFlags(m_displayLabel->textInteractionFlags() | Qt::TextSelectableByMouse);

    m_searchResultsLabel->setWordWrap(true);
    m_searchResultsLabel->setTextInteractionFlags(m_searchResultsLabel->textInteractionFlags() | Qt::TextSelectableByMouse);
    m_searchResultsLabel->hide();

    setLayout(new QVBoxLayout);
    layout()->addWidget(controls);
    layout()->addWidget(m_view);
    layout()->addWidget(m_displayLabel);
    layout()->addWidget(m_searchResultsLabel);

    m_backAction = KStandardAction::back(this, SLOT(navigateBack()), this);
    addAction(m_backAction);
    m_forwardAction = KStandardAction::forward(this, SLOT(navigateForward()), this);
    addAction(m_forwardAction);
    m_resetAction = new QAction(QIcon::fromTheme(QStringLiteral("go-first")), tr("Reset View"), this);
    m_resetAction->setShortcut(Qt::Key_Escape);
    connect(m_resetAction, &QAction::triggered, this, [this]() {
        selectItem(0);
    });
    addAction(m_resetAction);
    updateNavigationActions();
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
                updateNavigationActions();
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
            if (!m_buildingScene) {
                showData();
            }
        } else {
            selectItem(m_selectionHistory.at(m_selectedItem));
        }
        updateTooltip();
    } else if (event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent*>(event);
        auto item = static_cast<FrameGraphicsItem*>(m_view->itemAt(m_view->mapFromGlobal(contextEvent->globalPos())));

        QMenu contextMenu;
        QAction* viewCallerCallee = nullptr;
        if (item) {
            viewCallerCallee = contextMenu.addAction(tr("View Caller/Callee"));
            contextMenu.addSeparator();
        }
        contextMenu.addActions(actions());

        QAction *action = contextMenu.exec(QCursor::pos());

        if (action && action == viewCallerCallee) {
            emit jumpToCallerCallee(item->symbol());
        }
    } else if (event->type() == QEvent::ToolTip) {
        const auto& tooltip = m_displayLabel->toolTip();
        if (tooltip.isEmpty()) {
            QToolTip::hideText();
        } else {
            QToolTip::showText(QCursor::pos(), QLatin1String("<qt>")
                + tooltip.toHtmlEscaped() + QLatin1String("</qt>"), this);
        }
        event->accept();
        return true;
    }
    return ret;
}

void FlameGraph::setTopDownData(const Data::TopDownResults& topDownData)
{
    m_topDownData = topDownData;

    if (isVisible()) {
        showData();
    }
}

void FlameGraph::setBottomUpData(const Data::BottomUpResults& bottomUpData)
{
    m_bottomUpData = bottomUpData;

    disconnect(m_costSource, 0, this, 0);
    m_costSource->clear();
    for (int i = 0, c = bottomUpData.costs.numTypes(); i < c; ++i) {
        const auto& typeName = bottomUpData.costs.typeName(i);
        m_costSource->addItem(typeName, QVariant::fromValue(i));
        m_costSource->setItemData(i, i18n("Show a flame graph over the aggregated %1 sample costs.", typeName),
                                  Qt::ToolTipRole);
    }
    connect(m_costSource, static_cast<void (QComboBox::*) (int)>(&QComboBox::currentIndexChanged),
            this, &FlameGraph::showData);
}

void FlameGraph::showData()
{
    setData(nullptr);

    m_buildingScene = true;
    using namespace ThreadWeaver;
    auto showBottomUpData = m_showBottomUpData;
    auto bottomUpData = m_bottomUpData;
    auto topDownData = m_topDownData;
    bool collapseRecursion = m_collapseRecursion;
    auto type = m_costSource->currentData().value<int>();
    auto threshold = m_costThreshold;
    stream() << make_job([showBottomUpData, bottomUpData, topDownData, type, threshold, collapseRecursion, this]() {
        FrameGraphicsItem* parsedData = nullptr;
        if (showBottomUpData) {
            parsedData = parseData(bottomUpData.costs, type, bottomUpData.root.children, threshold, collapseRecursion);
        } else {
            parsedData = parseData(topDownData.inclusiveCosts, type, topDownData.root.children, threshold, collapseRecursion);
        }
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
    m_displayLabel->setText(metrics.elidedText(text, Qt::ElideRight, m_displayLabel->width()));
}

void FlameGraph::setData(FrameGraphicsItem* rootItem)
{
    m_scene->clear();
    m_buildingScene = false;
    m_tooltipItem = nullptr;
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

    if (!m_searchInput->text().isEmpty()) {
        setSearchValue(m_searchInput->text());
    }

    if (isVisible()) {
        selectItem(m_rootItem);
    }
}

void FlameGraph::selectItem(int item)
{
    m_selectedItem = item;
    updateNavigationActions();
    selectItem(m_selectionHistory.at(m_selectedItem));
}

void FlameGraph::selectItem(FrameGraphicsItem* item)
{
    if (!item) {
        return;
    }

    // scale item and its parents to the maximum available width
    // also hide all siblings of the parent items
    const auto padding = 10;
    const auto rootWidth = m_view->viewport()->width() - padding * 2;
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

void FlameGraph::setSearchValue(const QString& value)
{
    if (!m_rootItem) {
        return;
    }

    auto match = applySearch(m_rootItem, value);

    if (value.isEmpty()) {
        m_searchResultsLabel->hide();
    } else {
        m_searchResultsLabel->setText(i18n("%1 (%2% of total of %3) aggregated costs matched by search.",
                                           match.directCost, fraction(match.directCost, m_rootItem->cost()),
                                           m_rootItem->cost()));
        m_searchResultsLabel->show();
    }
}

void FlameGraph::navigateBack()
{
    if (m_selectedItem > 0) {
        selectItem(m_selectedItem - 1);
    }
}

void FlameGraph::navigateForward()
{
    if ((m_selectedItem + 1) < m_selectionHistory.size()) {
        selectItem(m_selectedItem + 1);
    }
}

void FlameGraph::updateNavigationActions()
{
    m_backAction->setEnabled(m_selectedItem > 0);
    m_forwardAction->setEnabled(m_selectedItem + 1 < m_selectionHistory.size());
    m_resetAction->setEnabled(m_selectedItem > 0);
}
