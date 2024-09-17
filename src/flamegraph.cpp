/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flamegraph.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontDatabase>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QStyleOption>
#include <QSvgGenerator>
#include <QToolBar>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <KColorScheme>
#include <KLocalizedString>
#include <KSqueezedTextLabel>
#include <KStandardAction>
#include <ThreadWeaver/ThreadWeaver>

#include "models/filterandzoomstack.h"
#include "resultsutil.h"
#include "settings.h"
#include "util.h"

namespace {
enum SearchMatchType
{
    NoSearch,
    NoMatch,
    DirectMatch,
    ChildMatch
};

template<class CreateInstance>
class CustomWidgetAction : public QWidgetAction
{
public:
    explicit CustomWidgetAction(CreateInstance createInstance, QWidget* parent)
        : QWidgetAction(parent)
        , _createInstance(createInstance)
    {
    }

    QWidget* createWidget(QWidget* parent) override
    {
        auto widget = new QWidget(parent);
        auto layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        _createInstance(widget, layout);
        return widget;
    }

private:
    CreateInstance _createInstance;
};
}

class FrameGraphicsItem : public QGraphicsRectItem
{
public:
    FrameGraphicsItem(qint64 cost, Data::Symbol symbol, FrameGraphicsItem* parent);

    qint64 cost() const;
    void setCost(qint64 cost);
    Data::Symbol symbol() const;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    QString description() const;
    void setSearchMatchType(SearchMatchType matchType);

    bool isExternallyHovered() const;
    void setIsExternallyHovered(bool isExternallyHovered);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    qint64 m_cost;
    Data::Symbol m_symbol;
    bool m_isHovered = false;
    bool m_isExternallyHovered = false;
    SearchMatchType m_searchMatch = NoSearch;
};
Q_DECLARE_METATYPE(FrameGraphicsItem*)

class FrameGraphicsRootItem : public FrameGraphicsItem
{
public:
    FrameGraphicsRootItem(const qint64 totalCost, Data::Costs::Unit unit, QString costName, const QString& label)
        : FrameGraphicsItem(totalCost, {label, {}}, nullptr)
        , m_costName(std::move(costName))
        , m_unit(unit)
    {
    }

    Data::Costs::Unit unit() const
    {
        return m_unit;
    }
    QString costName() const
    {
        return m_costName;
    }

private:
    QString m_costName;
    Data::Costs::Unit m_unit;
};

Q_DECLARE_METATYPE(FrameGraphicsRootItem*)

FrameGraphicsItem::FrameGraphicsItem(const qint64 cost, Data::Symbol symbol, FrameGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_cost(cost)
    , m_symbol(std::move(symbol))
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
    if (isSelected() || m_isHovered || m_isExternallyHovered || m_searchMatch == DirectMatch) {
        auto selectedColor = brush().color();
        selectedColor.setAlpha(255);
        painter->fillRect(rect(), selectedColor);
    } else if (m_searchMatch == NoMatch) {
        auto noMatchColor = brush().color();
        noMatchColor.setAlpha(50);
        painter->fillRect(rect(), noMatchColor);
    } else {
        // default, when no search is running, or a sub-item is matched
        auto background = brush();

        // give inline frames a slightly different background color
        if (m_symbol.isInline) {
            auto color = background.color();
            if (qGray(pen().color().rgb()) < 128) {
                color = color.lighter();
            } else {
                color = color.darker();
            }
            background.setColor(color);
        }
        painter->fillRect(rect(), background);

        // give inline frames a border with the normal background color
        if (m_symbol.isInline) {
            const auto oldPen = painter->pen();
            painter->setPen(QPen(brush().color(), 0.));
            painter->drawRect(rect().adjusted(-1, -1, -1, -1));
            painter->setPen(oldPen);
        }
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
    const auto binary = Util::formatString(m_symbol.binary);
    const auto symbol = Util::formatSymbol(m_symbol, false);
    const auto symbolText = symbol.isEmpty() ? QObject::tr("?? [%1]").arg(binary) : symbol;
    painter->drawText(margin + rect().x(), rect().y(), width, height,
                      Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      Util::elideSymbol(symbolText, option->fontMetrics, width));

    if (m_searchMatch == NoMatch) {
        painter->setPen(oldPen);
    }
}

void FrameGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsRectItem::hoverEnterEvent(event);
    m_isHovered = true;
    update();
}

void FrameGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsRectItem::hoverLeaveEvent(event);
    m_isHovered = false;
    update();
}

QString FrameGraphicsItem::description() const
{
    // we build the tooltip text on demand, which is much faster than doing that for potentially thousands of items when
    // we load the data
    const FrameGraphicsRootItem* root = nullptr;
    {
        auto item = this;
        while (item->parentItem()) {
            item = static_cast<const FrameGraphicsItem*>(item->parentItem());
        }
        root = static_cast<const FrameGraphicsRootItem*>(item);
    }

    auto symbol = Util::formatSymbolExtended(m_symbol);
    if (root == this) {
        return symbol;
    }

    switch (root->unit()) {
    case Data::Costs::Unit::Unknown:
        return i18nc("%1: aggregated sample costs, %2: relative number, %3: function label, %4: binary, %5: cost name",
                     "%1 (%2%) aggregated %5 costs in %3 (%4) and below.",
                     Data::Costs::formatCost(root->unit(), m_cost), Util::formatCostRelative(m_cost, root->cost()),
                     symbol, m_symbol.binary, root->costName());
    case Data::Costs::Unit::Tracepoint:
        return i18nc("%1: number of tracepoint events, %2: relative number, %3: function label, %4: binary",
                     "%1 (%2%) aggregated %5 events in %3 (%4) and below.",
                     Data::Costs::formatCost(root->unit(), m_cost), Util::formatCostRelative(m_cost, root->cost()),
                     symbol, m_symbol.binary, root->costName());
    case Data::Costs::Unit::Time:
        return i18nc("%1: elapsed time, %2: relative number, %3: function label, %4: binary",
                     "%1 (%2%) aggregated %5 in %3 (%4) and below.", Data::Costs::formatCost(root->unit(), m_cost),
                     Util::formatCostRelative(m_cost, root->cost()), symbol, m_symbol.binary, root->costName());
    }
    Q_UNREACHABLE();
}

void FrameGraphicsItem::setSearchMatchType(SearchMatchType matchType)
{
    if (m_searchMatch != matchType) {
        m_searchMatch = matchType;
        update();
    }
}

bool FrameGraphicsItem::isExternallyHovered() const
{
    return m_isExternallyHovered;
}

void FrameGraphicsItem::setIsExternallyHovered(bool externallyHovered)
{
    if (m_isExternallyHovered != externallyHovered) {
        m_isExternallyHovered = externallyHovered;
        update();
    }
}

namespace {

int randInt(int max)
{
    return max * QRandomGenerator::global()->generateDouble();
}

/**
 * Generate a brush from the "mem" color space used in upstream flamegraph.pl
 */
Q_DECL_UNUSED QBrush memBrush()
{
    return QColor(0, 190 + randInt(50), randInt(210), 125);
}

/**
 * Generate a brush from the "hot" color space used in upstream flamegraph.pl
 */
QBrush hotBrush()
{
    return QColor(205 + randInt(50), randInt(230), randInt(55), 125);
}

template<typename Generator>
QVector<QBrush> generateBrushes(Generator generator)
{
    QVector<QBrush> ret;
    std::generate_n(std::back_inserter(ret), 100, generator);
    return ret;
}

enum class BrushType
{
    Hot,
    Memory
};

/**
 * Generate a brush from the "hot" color space used in upstream flamegraph.pl
 */
QBrush brushImpl(uint hash, BrushType type)
{
    // intern the brushes, to reuse them across items which can be thousands
    // otherwise we'd end up with dozens of allocations and higher memory consumption
    static const QVector<QBrush> allBrushes[2] = {generateBrushes(hotBrush), generateBrushes(memBrush)};
    const auto& brushes = allBrushes[static_cast<uint>(type)];
    return brushes.at(hash % brushes.size());
}

QBrush brushBinary(const Data::Symbol& symbol)
{
    static QHash<QString, QBrush> brushes;

    auto& brush = brushes[symbol.binary];
    if (brush == QBrush()) {
        brush = brushImpl(qHash(symbol.binary), BrushType::Hot);
    }
    return brush;
}

QBrush brushKernel(const Data::Symbol& symbol)
{
    static auto kernel = QBrush(QColor(255, 0, 0, 125));
    static auto user = QBrush(QColor(0, 0, 255, 125));

    if (symbol.isKernel) {
        return kernel;
    }
    return user;
}

bool isInPathList(const QStringList& paths, const QString& subPath)
{
    auto containsSubPath = [subPath](const auto& path) { return subPath.startsWith(path); };

    return std::any_of(paths.cbegin(), paths.cend(), containsSubPath);
}

struct BrushConfig
{
    bool isSystemPath(const QString& path) const
    {
        return isInPathList(systemPaths, path);
    }

    bool isUserPath(const QString& path) const
    {
        return isInPathList(userPaths, path);
    }

    Settings::ColorScheme scheme;
    QStringList systemPaths;
    QStringList userPaths;
};

// construct brush config in main thread and query settings that then can be used in the background
// without introducing data races
BrushConfig brushConfig(Settings::ColorScheme scheme)
{
    return {scheme, Settings::instance()->systemPaths(), Settings::instance()->userPaths()};
}

QBrush brushSystem(const Data::Symbol& symbol, const BrushConfig& brushConfig)
{
    static const auto system = QBrush(QColor(0, 125, 0, 125));
    static const auto user = QBrush(QColor(200, 200, 0, 125));
    static const auto unknown = QBrush(QColor(50, 50, 50, 125));

    // remark lievenhey: I have seen [ only on kernel calls
    if (symbol.path.isEmpty() || symbol.path.startsWith(QLatin1Char('['))) {
        return unknown;
    } else if (!brushConfig.isUserPath(symbol.path) && brushConfig.isSystemPath(symbol.path)) {
        return system;
    }
    return user;
}

QBrush costRatioBrush(quint32 cost, quint32 totalCost)
{
    // interpolate between red and yellow
    // where yellow = 0% and red = 100%
    const float ratio = (1 - static_cast<float>(cost) / totalCost);
    const int hue = ratio * ratio * 60;
    return QColor::fromHsv(hue, 230, 200, 125);
}

QBrush brush(const Data::Symbol& entry, const BrushConfig& brushConfig, quint32 cost = 0, quint32 totalCost = 1)
{
    switch (brushConfig.scheme) {
    case Settings::ColorScheme::Binary:
        return brushBinary(entry);
    case Settings::ColorScheme::Kernel:
        return brushKernel(entry);
    case Settings::ColorScheme::System:
        return brushSystem(entry, brushConfig);
    case Settings::ColorScheme::Default:
        return brushImpl(qHash(entry), BrushType::Hot);
    case Settings::ColorScheme::CostRatio:
        return costRatioBrush(cost, totalCost);
    case Settings::ColorScheme::NumColorSchemes:
        Q_UNREACHABLE();
    }
    return {};
}

/**
 * Layout the flame graph and hide tiny items.
 */
void layoutItems(FrameGraphicsItem* parent)
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
    std::sort(children.begin(), children.end(), [](const QGraphicsItem* lhs, const QGraphicsItem* rhs) {
        return static_cast<const FrameGraphicsItem*>(lhs)->symbol()
            < static_cast<const FrameGraphicsItem*>(rhs)->symbol();
    });

    for (auto child : children) {
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
    for (auto item_ : items) {
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
void toGraphicsItems(const Data::Costs& costs, int type, const QVector<Tree>& data, FrameGraphicsItem* parent,
                     const double costThreshold, const BrushConfig& brushConfig, bool collapseRecursion)
{
    for (const auto& row : data) {
        if (collapseRecursion && !row.symbol.symbol.isEmpty() && row.symbol == parent->symbol()) {
            if (costs.cost(type, row.id) > costThreshold) {
                toGraphicsItems(costs, type, row.children, parent, costThreshold, brushConfig, collapseRecursion);
            }
            continue;
        }
        auto item = findItemBySymbol(parent->childItems(), row.symbol);
        if (!item) {
            item = new FrameGraphicsItem(costs.cost(type, row.id), row.symbol, parent);
            item->setPen(parent->pen());
            item->setBrush(brush(row.symbol, brushConfig, item->cost(), costs.totalCost(type)));
        } else {
            item->setCost(item->cost() + costs.cost(type, row.id));
        }
        if (item->cost() > costThreshold) {
            toGraphicsItems(costs, type, row.children, item, costThreshold, brushConfig, collapseRecursion);
        }
    }
}

template<typename Tree>
FrameGraphicsItem* parseData(const Data::Costs& costs, int type, const QVector<Tree>& topDownData, double costThreshold,
                             const BrushConfig& brushConfig, bool collapseRecursion)
{
    const auto totalCost = costs.totalCost(type);

    const auto scheme = KColorScheme(QPalette::Active);
    const auto pen = QPen(scheme.foreground().color());

    const auto label = i18n("%1 aggregated %2 cost in total", costs.formatCost(type, totalCost), costs.typeName(type));
    auto rootItem = new FrameGraphicsRootItem(totalCost, costs.unit(type), costs.typeName(type), label);
    rootItem->setBrush(scheme.background());
    rootItem->setPen(pen);
    toGraphicsItems(costs, type, topDownData, rootItem, static_cast<double>(totalCost) * costThreshold / 100.,
                    brushConfig, collapseRecursion);
    return rootItem;
}

struct SearchResults
{
    SearchMatchType matchType = NoMatch;
    qint64 directCost = 0;
};

SearchResults applySearch(FrameGraphicsItem* item, const QRegularExpression& expression)
{
    SearchResults result;
    if (expression.pattern().isEmpty()) {
        result.matchType = NoSearch;
    } else if (expression.match(item->symbol().symbol).hasMatch() || expression.match(item->symbol().binary).hasMatch()
               || (expression.pattern() == QLatin1String("\\?\\?") && item->symbol().symbol.isEmpty())) {
        result.directCost += item->cost();
        result.matchType = DirectMatch;
    }

    // recurse into the child items, we always need to update all items
    const auto children = item->childItems();
    for (auto* child : children) {
        auto* childFrame = static_cast<FrameGraphicsItem*>(child);
        auto childMatch = applySearch(childFrame, expression);
        if (result.matchType != DirectMatch
            && (childMatch.matchType == DirectMatch || childMatch.matchType == ChildMatch)) {
            result.matchType = ChildMatch;
            result.directCost += childMatch.directCost;
        }
    }

    item->setSearchMatchType(result.matchType);
    return result;
}

// only apply positive matching, resetting is handled globally once before
// this way we can correctly match multiple stacks
bool hoverStack(FrameGraphicsItem* item, const QVector<Data::Symbol>& stack, int depth)
{
    if ((stack.size() - 1) == depth && item->symbol() == stack.constFirst()) {
        item->setIsExternallyHovered(true);
        return true;
    } else if (stack.size() <= depth || item->symbol() != stack[stack.size() - 1 - depth]) {
        return false;
    }

    const auto children = item->childItems();
    for (auto* child : children) {
        if (hoverStack(static_cast<FrameGraphicsItem*>(child), stack, depth + 1)) {
            item->setIsExternallyHovered(true);
            return true;
        }
    }

    return false;
}

void resetIsExternallyHovered(FrameGraphicsItem* item)
{
    if (!item->isExternallyHovered()) {
        // when nothing is hovered we don't need to recurse
        return;
    }
    item->setIsExternallyHovered(false);
    const auto children = item->childItems();
    for (auto* child : children) {
        resetIsExternallyHovered(static_cast<FrameGraphicsItem*>(child));
    }
}

void hoverStacks(FrameGraphicsItem* rootItem, const QVector<QVector<Data::Symbol>>& stacks)
{
    auto matchStacks = [&stacks](QGraphicsItem* item) {
        return std::any_of(stacks.begin(), stacks.end(), [item](const auto& stack) {
            return hoverStack(static_cast<FrameGraphicsItem*>(item), stack, 0);
        });
    };

    const auto children = rootItem->childItems();
    const auto costAggregation = Settings::instance()->costAggregation();
    const auto skipFirstLevel = costAggregation != Settings::CostAggregation::BySymbol;
    for (auto* child : children) {
        // reset everything first
        resetIsExternallyHovered(static_cast<FrameGraphicsItem*>(child));

        // then match all stacks
        if (skipFirstLevel) {
            // skip first level
            const auto grandChildren = child->childItems();
            bool anyMatched = false;
            for (auto* grandChild : grandChildren) {
                anyMatched |= matchStacks(grandChild);
            }
            static_cast<FrameGraphicsItem*>(child)->setIsExternallyHovered(anyMatched);
        } else {
            matchStacks(child);
        }
    }
}
}

void updateFlameGraphColorScheme(FrameGraphicsItem* item, const BrushConfig& brushConfig, quint32 totalCost)
{
    item->setBrush(brush(item->symbol(), brushConfig, item->cost(), totalCost));
    const auto children = item->childItems();
    for (const auto& child : children) {
        updateFlameGraphColorScheme(static_cast<FrameGraphicsItem*>(child), brushConfig, totalCost);
    }
}

FlameGraph::FlameGraph(QWidget* parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_costSource(new QComboBox(this))
    , m_scene(new QGraphicsScene(this))
    , m_view(new QGraphicsView(this))
    , m_displayLabel(new KSqueezedTextLabel(this))
    , m_searchResultsLabel(new QLabel(this))
{
    m_displayLabel->setTextElideMode(Qt::ElideRight);
    qRegisterMetaType<FrameGraphicsItem*>();

    m_costSource->setToolTip(i18n("Select the data source that should be visualized in the flame graph."));

    const auto updateHelper = [this]() {
        m_scene->update(m_scene->sceneRect());
        updateTooltip();
    };

    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, updateHelper);

    connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, updateHelper);

    connect(Settings::instance(), &Settings::collapseDepthChanged, this, updateHelper);

    m_scene->setBackgroundBrush(QBrush());

    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_view->setScene(m_scene);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);
    // fix for  QTBUG-105237 view->setFont does not update fontMetrics only the rendered font
    m_scene->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto bottomUpAction = new CustomWidgetAction(
        [this](QWidget* widget, QHBoxLayout* layout) {
            auto bottomUpCheckbox = new QCheckBox(i18n("Bottom-Up View"), widget);
            layout->addWidget(bottomUpCheckbox);
            connect(this, &FlameGraph::uiResetRequested, bottomUpCheckbox,
                    [bottomUpCheckbox]() { bottomUpCheckbox->setChecked(false); });
            bottomUpCheckbox->setToolTip(i18n("Enable the bottom-up flame graph view. When this is unchecked, the "
                                              "top-down view is enabled by default."));
            bottomUpCheckbox->setChecked(m_showBottomUpData);
            connect(bottomUpCheckbox, &QCheckBox::toggled, this, [this, bottomUpCheckbox] {
                const auto showBottomUpData = bottomUpCheckbox->isChecked();
                if (showBottomUpData == m_showBottomUpData) {
                    return;
                }

                m_showBottomUpData = showBottomUpData;
                for (auto& stack : m_hoveredStacks) {
                    std::reverse(stack.begin(), stack.end());
                }
                showData();
            });
        },
        this);

    m_costThreshold = DEFAULT_COST_THRESHOLD;

    auto costThresholdAction = new CustomWidgetAction(
        [this](QWidget* widget, QHBoxLayout* layout) {
            auto costThreshold = new QDoubleSpinBox(widget);
            costThreshold->setDecimals(2);
            costThreshold->setMinimum(0);
            costThreshold->setMaximum(99.90);
            costThreshold->setPrefix(i18n("Cost Threshold: "));
            costThreshold->setSuffix(QStringLiteral("%"));
            costThreshold->setValue(m_costThreshold);
            connect(this, &FlameGraph::uiResetRequested, costThreshold,
                    [costThreshold]() { costThreshold->setValue(DEFAULT_COST_THRESHOLD); });
            costThreshold->setSingleStep(0.01);
            costThreshold->setToolTip(
                i18n("<qt>The cost threshold defines a fractional cut-off value. "
                     "Items with a relative cost below this value will not be shown in the flame graph. "
                     "This is done as an optimization to quickly generate graphs for large data sets with "
                     "low memory overhead. If you need more details, decrease the threshold value, or set it to "
                     "zero.</qt>"));

            connect(costThreshold, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double threshold) {
                m_costThreshold = threshold;
                showData();
            });
            layout->addWidget(costThreshold);
        },
        this);

    auto collapseRecursionAction = new CustomWidgetAction(
        [this](QWidget* widget, QHBoxLayout* layout) {
            auto checkbox = new QCheckBox(tr("Collapse Recursion"), widget);
            checkbox->setChecked(m_collapseRecursion);
            layout->addWidget(checkbox);

            connect(checkbox, &QCheckBox::clicked, widget, [this](bool checked) {
                m_collapseRecursion = checked;
                showData();
            });
        },
        this);

    auto costAggregation = new CustomWidgetAction(
        [](QWidget* widget, QHBoxLayout* layout) {
            auto costAggregationLabel = new QLabel(tr("Aggregate cost by:"), widget);
            layout->addWidget(costAggregationLabel);

            auto costAggregation = new QComboBox(widget);
            ResultsUtil::setupResultsAggregation(costAggregation);
            layout->addWidget(costAggregation);
        },
        this);

    auto colorSchemeSelector = new CustomWidgetAction(
        [this](QWidget* widget, QHBoxLayout* layout) {
            auto label = new QLabel(tr("Color Scheme:"), widget);
            layout->addWidget(label);

            auto comboBox = new QComboBox(widget);
            layout->addWidget(comboBox);

            comboBox->addItem(tr("Default"), QVariant::fromValue(Settings::ColorScheme::Default));
            comboBox->addItem(tr("Binary"), QVariant::fromValue(Settings::ColorScheme::Binary));
            comboBox->addItem(tr("Kernel"), QVariant::fromValue(Settings::ColorScheme::Kernel));
            comboBox->addItem(tr("System"), QVariant::fromValue(Settings::ColorScheme::System));
            comboBox->addItem(tr("Cost Ratio"), QVariant::fromValue(Settings::ColorScheme::CostRatio));

            auto setColorScheme = [this](Settings::ColorScheme scheme) {
                Settings::instance()->setColorScheme(scheme);

                if (m_rootItem) {
                    const auto children = m_rootItem->childItems();
                    // don't recolor the root item
                    for (const auto& child : children) {
                        updateFlameGraphColorScheme(static_cast<FrameGraphicsItem*>(child), brushConfig(scheme),
                                                    m_rootItem->cost());
                    }
                }
            };

            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [comboBox, setColorScheme] {
                setColorScheme(comboBox->currentData().value<Settings::ColorScheme>());
            });

            connect(Settings::instance(), &Settings::pathsChanged, this, [setColorScheme] {
                if (Settings::instance()->colorScheme() == Settings::ColorScheme::System) {
                    // setColorScheme triggers a redraw
                    // we only need to redraw the flamegraph if the current color scheme is affected by the changed
                    // paths
                    setColorScheme(Settings::ColorScheme::System);
                }
            });
        },
        this);

    auto searchInput = new CustomWidgetAction(
        [this](QWidget* widget, QHBoxLayout* layout) {
            auto searchInput = new QLineEdit(widget);
            searchInput->setMinimumWidth(200);
            layout->addWidget(searchInput);

            auto regexCheckBox = new QCheckBox(widget);
            regexCheckBox->setText(tr("Regex Search"));
            layout->addWidget(regexCheckBox);

            searchInput->setPlaceholderText(i18n("Search..."));
            searchInput->setToolTip(i18n("<qt>Search the flame graph for a symbol.</qt>"));
            searchInput->setClearButtonEnabled(true);
            connect(searchInput, &QLineEdit::textChanged, this,
                    [this](const QString& value) { this->setSearchValue(value, m_useRegex); });
            auto applyRegexCheckBox = [this](bool checked) { this->setSearchValue(m_search, checked); };
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
            connect(regexCheckBox, &QCheckBox::stateChanged, this, applyRegexCheckBox);
#else
            connect(regexCheckBox, &QCheckBox::checkStateChanged, this, applyRegexCheckBox);
#endif
            connect(this, &FlameGraph::uiResetRequested, this, [this, searchInput, regexCheckBox] {
                m_search.clear();
                m_useRegex = false;
                searchInput->clear();
                regexCheckBox->setChecked(false);
            });
        },
        this);

    m_backAction = KStandardAction::back(this, &FlameGraph::navigateBack, this);
    m_backAction->setToolTip(QStringLiteral("Go back in symbol view history"));
    m_forwardAction = KStandardAction::forward(this, &FlameGraph::navigateForward, this);
    m_forwardAction->setToolTip(QStringLiteral("Go forward in symbol view history"));

    m_resetAction = new QAction(QIcon::fromTheme(QStringLiteral("go-first")), tr("Reset View"), this);
    m_resetAction->setShortcut(tr("Escape"));
    connect(m_resetAction, &QAction::triggered, this, [this]() { selectItem(0); });

    // use a QToolBar to automatically hide widgets in a menu that don't fit into the window
    auto controls = new QToolBar(this);
    controls->layout()->setContentsMargins(0, 0, 0, 0);

    // these control widgets can stay as they are, since they should always be visible
    controls->addAction(m_resetAction);
    controls->addAction(m_backAction);
    controls->addAction(m_forwardAction);
    controls->addWidget(m_costSource);

    // these can be hidden as necessary
    controls->addAction(searchInput);
    controls->addAction(costAggregation);
    controls->addAction(colorSchemeSelector);
    controls->addAction(bottomUpAction);
    controls->addAction(collapseRecursionAction);
    controls->addAction(costThresholdAction);

    m_displayLabel->setWordWrap(true);
    m_displayLabel->setTextInteractionFlags(m_displayLabel->textInteractionFlags() | Qt::TextSelectableByMouse);

    m_searchResultsLabel->setWordWrap(true);
    m_searchResultsLabel->setTextInteractionFlags(m_searchResultsLabel->textInteractionFlags()
                                                  | Qt::TextSelectableByMouse);
    m_searchResultsLabel->hide();

    setLayout(new QVBoxLayout);
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->addWidget(controls);
    layout()->addWidget(m_view);
    layout()->addWidget(m_displayLabel);
    layout()->addWidget(m_searchResultsLabel);

    addAction(m_backAction);
    addAction(m_forwardAction);
    addAction(m_resetAction);
    updateNavigationActions();
}

FlameGraph::~FlameGraph() = default;

void FlameGraph::setHoveredStacks(const QVector<QVector<Data::Symbol>>& hoveredStacks)
{
    if (m_hoveredStacks == hoveredStacks) {
        return;
    }

    m_hoveredStacks = hoveredStacks;
    if (m_showBottomUpData) {
        for (auto& stack : m_hoveredStacks) {
            std::reverse(stack.begin(), stack.end());
        }
    }

    if (m_rootItem) {
        hoverStacks(m_rootItem, m_hoveredStacks);
    }
}

void FlameGraph::setFilterStack(FilterAndZoomStack* filterStack)
{
    m_filterStack = filterStack;
}

bool FlameGraph::eventFilter(QObject* object, QEvent* event)
{
    const auto ret = QObject::eventFilter(object, event);

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
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
        } else if (mouseEvent->button() == Qt::BackButton) {
            m_backAction->trigger();
        } else if (mouseEvent->button() == Qt::ForwardButton) {
            m_forwardAction->trigger();
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
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
        auto* contextEvent = static_cast<QContextMenuEvent*>(event);
        auto item = static_cast<FrameGraphicsItem*>(m_view->itemAt(m_view->mapFromGlobal(contextEvent->globalPos())));

        QMenu contextMenu;
        if (item) {
            auto* viewCallerCallee = contextMenu.addAction(tr("View Caller/Callee"));
            connect(viewCallerCallee, &QAction::triggered, this,
                    [this, item]() { emit jumpToCallerCallee(item->symbol()); });
            auto* openEditorAction = contextMenu.addAction(tr("Open in Editor"));
            connect(openEditorAction, &QAction::triggered, this, [this, item]() { emit openEditor(item->symbol()); });
            openEditorAction->setEnabled(item->symbol().isValid());
            contextMenu.addSeparator();
            auto* viewDisassembly = contextMenu.addAction(tr("Disassembly"));
            connect(viewDisassembly, &QAction::triggered, this,
                    [this, item]() { emit jumpToDisassembly(item->symbol()); });
            viewDisassembly->setEnabled(item->symbol().canDisassemble());

            auto* copy = contextMenu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy"));
            connect(copy, &QAction::triggered, this, [item]() { qApp->clipboard()->setText(item->description()); });

            contextMenu.addSeparator();
        }
        ResultsUtil::addFilterActions(&contextMenu, item ? item->symbol() : Data::Symbol(), m_filterStack);
        contextMenu.addSeparator();
        contextMenu.addActions(actions());

        contextMenu.exec(QCursor::pos());
        return true;
    } else if (event->type() == QEvent::ToolTip) {
        const auto& tooltip = m_displayLabel->toolTip();
        if (tooltip.isEmpty()) {
            QToolTip::hideText();
        } else {
            QToolTip::showText(QCursor::pos(), QLatin1String("<qt>") + tooltip.toHtmlEscaped() + QLatin1String("</qt>"),
                               this);
        }
        event->accept();
        return true;
    }
    return ret;
}

void FlameGraph::setTopDownData(const Data::TopDownResults& topDownData)
{
    m_topDownData = topDownData;

    if (!m_showBottomUpData)
        rebuild();
}

void FlameGraph::setBottomUpData(const Data::BottomUpResults& bottomUpData)
{
    m_bottomUpData = bottomUpData;
    m_topDownData = {};

    disconnect(m_costSource, nullptr, this, nullptr);
    ResultsUtil::fillEventSourceComboBox(m_costSource, bottomUpData.costs,
                                         tr("Show a flame graph over the aggregated %1 sample costs."));
    connect(m_costSource, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &FlameGraph::showData);

    rebuild();
}

void FlameGraph::rebuild()
{
    if (isVisible()) {
        showData();
    } else {
        setData(nullptr);
    }
}

void FlameGraph::clear()
{
    emit uiResetRequested();
}

QImage FlameGraph::toImage() const
{
    if (!m_rootItem)
        return {};

    const auto sceneRect = m_scene->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    m_scene->render(&painter, {{0, 0}, sceneRect.size()}, sceneRect);
    return image;
}

void FlameGraph::saveSvg(const QString& fileName) const
{
    if (!m_rootItem)
        return;

    const auto sceneRect = m_scene->sceneRect();

    QSvgGenerator generator;
    generator.setSize(sceneRect.size().toSize());
    generator.setViewBox({{0, 0}, sceneRect.size()});
    generator.setFileName(fileName);
    if (m_showBottomUpData)
        generator.setTitle(tr("Bottom Up FlameGraph"));
    else
        generator.setTitle(tr("Top Down FlameGraph"));
    const auto costType = m_bottomUpData.costs.typeName(m_costSource->currentData().value<int>());
    generator.setDescription(tr("Cost type: %1, cost threshold: %2\n%3")
                                 .arg(costType, QString::number(m_costThreshold), m_displayLabel->text())
                                 .toHtmlEscaped());

    const auto oldPen = m_rootItem->pen();
    const auto oldBrush = m_rootItem->brush();
    m_rootItem->setPen(QPen(Qt::black));
    m_rootItem->setBrush(QBrush(Qt::white));

    QPainter painter(&generator);
    m_scene->render(&painter, generator.viewBoxF(), sceneRect);

    m_rootItem->setPen(oldPen);
    m_rootItem->setBrush(oldBrush);
}

void FlameGraph::showData()
{
    auto showBottomUpData = m_showBottomUpData;
    if ((showBottomUpData && !m_bottomUpData.costs.numTypes())
        || (!showBottomUpData && !m_topDownData.selfCosts.numTypes())) {
        // gammaray asks for the data to be shown too early, ensure we don't crash then
        return;
    }

    setData(nullptr);

    m_buildingScene = true;
    using namespace ThreadWeaver;
    auto bottomUpData = m_bottomUpData;
    auto topDownData = m_topDownData;
    const auto collapseRecursion = m_collapseRecursion;
    auto type = m_costSource->currentData().value<int>();
    auto threshold = m_costThreshold;
    auto brushConfig = ::brushConfig(Settings::instance()->colorScheme());

    stream() << make_job(
        [showBottomUpData, bottomUpData, topDownData, type, threshold, brushConfig, collapseRecursion, this]() {
            FrameGraphicsItem* parsedData = nullptr;
            if (showBottomUpData) {
                parsedData = parseData(bottomUpData.costs, type, bottomUpData.root.children, threshold, brushConfig,
                                       collapseRecursion);
            } else {
                parsedData = parseData(topDownData.inclusiveCosts, type, topDownData.root.children, threshold,
                                       brushConfig, collapseRecursion);
            }
            QMetaObject::invokeMethod(this, "setData", Qt::QueuedConnection, Q_ARG(FrameGraphicsItem*, parsedData));
        });
    updateNavigationActions();
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

    if (item) {
        emit selectSymbol(item->symbol());

        const auto costAggregation = Settings::instance()->costAggregation();
        const auto skipFirstLevel = costAggregation != Settings::CostAggregation::BySymbol;
        QVector<Data::Symbol> stack;
        stack.reserve(32);
        while (item && (item != m_rootItem && (!skipFirstLevel || item->parentItem() != m_rootItem))) {
            stack.append(item->symbol());
            item = static_cast<const FrameGraphicsItem*>(item->parentItem());
        }
        emit selectStack(stack, m_showBottomUpData);
    }
}

void FlameGraph::updateTooltip()
{
    const auto text = m_tooltipItem ? m_tooltipItem->description() : QString();
    m_displayLabel->setToolTip(text);
    m_displayLabel->setText(text);
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

    if (!m_search.isEmpty()) {
        setSearchValue(m_search, m_useRegex);
    }
    if (!m_hoveredStacks.isEmpty()) {
        hoverStacks(rootItem, m_hoveredStacks);
    }

    if (isVisible()) {
        selectItem(m_rootItem);
    }

    emit canConvertToImageChanged();
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
    const auto padding = 8;
    const auto rootWidth = m_view->viewport()->width() - padding * 2
        - (m_view->verticalScrollBar()->isVisible() ? 0 : m_view->verticalScrollBar()->sizeHint().width());
    auto parent = item;
    while (parent) {
        auto rect = parent->rect();
        rect.setLeft(0);
        rect.setWidth(rootWidth);
        parent->setRect(rect);
        if (parent->parentItem()) {
            const auto children = parent->parentItem()->childItems();
            for (auto sibling : children) {
                sibling->setVisible(sibling == parent);
            }
        }
        parent = static_cast<FrameGraphicsItem*>(parent->parentItem());
    }

    // then layout all items below the selected on
    layoutItems(item);

    // Triggers a refresh of the scene's bounding rect without going via the
    // event loop. This makes the centerOn call below work as expected in all cases.
    m_scene->sceneRect();

    // and make sure it's visible
    m_view->centerOn(item);

    setTooltipItem(item);
}

void FlameGraph::setSearchValue(const QString& value, bool useRegex)
{
    if (!m_rootItem) {
        return;
    }

    m_search = value;
    m_useRegex = useRegex;
    auto regex = useRegex ? value : QRegularExpression::escape(value);
    auto match = applySearch(m_rootItem, QRegularExpression(regex));

    if (value.isEmpty()) {
        m_searchResultsLabel->hide();
    } else {
        m_searchResultsLabel->setText(
            i18n("%1 (%2% of total of %3) aggregated costs matched by search.", Util::formatCost(match.directCost),
                 Util::formatCostRelative(match.directCost, m_rootItem->cost()), m_rootItem->cost()));
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
    const bool hasItems = m_selectedItem > 0;
    const bool isNotLastItem = m_selectedItem + 1 < m_selectionHistory.size();
    m_backAction->setEnabled(hasItems);
    m_forwardAction->setEnabled(isNotLastItem);
    m_resetAction->setEnabled(hasItems);
}

bool FlameGraph::canConvertToImage() const
{
    return m_rootItem != nullptr;
}
