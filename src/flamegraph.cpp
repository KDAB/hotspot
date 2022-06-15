/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "flamegraph.h"

#include <cmath>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QStyleOption>
#include <QSvgGenerator>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <KColorScheme>
#include <KLocalizedString>
#include <KStandardAction>
#include <KSqueezedTextLabel>
#include <ThreadWeaver/ThreadWeaver>

#include "models/filterandzoomstack.h"
#include "resultsutil.h"
#include "settings.h"

namespace {
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
    FrameGraphicsItem(const qint64 cost, const Data::Symbol& symbol, FrameGraphicsItem* parent);

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
    bool m_isHovered;
    bool m_isExternallyHovered;
    SearchMatchType m_searchMatch = NoSearch;
};
Q_DECLARE_METATYPE(FrameGraphicsItem*)

class FrameGraphicsRootItem : public FrameGraphicsItem
{
public:
    FrameGraphicsRootItem(const qint64 totalCost, Data::Costs::Unit unit, const QString& costName, const QString& label)
        : FrameGraphicsItem(totalCost, {label, {}}, nullptr)
        , m_costName(costName)
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

FrameGraphicsItem::FrameGraphicsItem(const qint64 cost, const Data::Symbol& symbol, FrameGraphicsItem* parent)
    : QGraphicsRectItem(parent)
    , m_cost(cost)
    , m_symbol(symbol)
    , m_isHovered(false)
    , m_isExternallyHovered(false)
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
    const auto binary = Util::formatString(m_symbol.binary);
    const auto symbol = Util::formatSymbol(m_symbol, false);
    const auto symbolText = symbol.isEmpty() ? QObject::tr("?? [%1]").arg(binary) : symbol;
    painter->drawText(margin + rect().x(), rect().y(), width, height,
                      Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      option->fontMetrics.elidedText(symbolText, Qt::ElideRight, width));

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
    const auto symbol = Util::formatSymbol(m_symbol);
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

int rand(int max)
{
    return max * QRandomGenerator::global()->generateDouble();
}

/**
 * Generate a brush from the "mem" color space used in upstream flamegraph.pl
 */
Q_DECL_UNUSED QBrush memBrush()
{
    return QColor(0, 190 + rand(50), rand(210), 125);
}

/**
 * Generate a brush from the "hot" color space used in upstream flamegraph.pl
 */
QBrush hotBrush()
{
    return QColor(205 + rand(50), rand(230), rand(55), 125);
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
    static QBrush kernel(QColor(255, 0, 0, 125));
    static QBrush user(QColor(0, 0, 255, 125));

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

bool isSystemPath(const QString& path)
{
    return isInPathList(Settings::instance()->systemPaths(), path);
}

bool isUserPath(const QString& path)
{
    return isInPathList(Settings::instance()->userPaths(), path);
}

QBrush brushSystem(const Data::Symbol& symbol)
{
    static QBrush system(QColor(0, 125, 0, 125));
    static QBrush user(QColor(200, 200, 0, 125));
    static QBrush unkown(QColor(50, 50, 50, 125));

    // I have seen [ only on kernel calls
    if (symbol.path.isEmpty() || symbol.path.startsWith(QLatin1Char('['))) {
        return unkown;
    } else if (isSystemPath(symbol.path)) {
        if (isUserPath(symbol.path)) {
            return user;
        }
        return system;
    }
    return user;
}

QBrush brush(const Data::Symbol& entry, Settings::ColorScheme scheme)
{
    switch (scheme) {
    case Settings::ColorScheme::Binary:
        return brushBinary(entry);
    case Settings::ColorScheme::Kernel:
        return brushKernel(entry);
    case Settings::ColorScheme::System:
        return brushSystem(entry);
    case Settings::ColorScheme::Default:
        return brushImpl(qHash(entry), BrushType::Hot);
    case Settings::ColorScheme::NumColorSchemes:
        Q_UNREACHABLE();
    }
    return QBrush();
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
void toGraphicsItems(const Data::Costs& costs, int type, const QVector<Tree>& data, FrameGraphicsItem* parent,
                     const double costThreshold, const Settings::ColorScheme& colorScheme, bool collapseRecursion)
{
    foreach (const auto& row, data) {
        if (collapseRecursion && !row.symbol.symbol.isEmpty() && row.symbol == parent->symbol()) {
            if (costs.cost(type, row.id) > costThreshold) {
                toGraphicsItems(costs, type, row.children, parent, costThreshold, colorScheme, collapseRecursion);
            }
            continue;
        }
        auto item = findItemBySymbol(parent->childItems(), row.symbol);
        if (!item) {
            item = new FrameGraphicsItem(costs.cost(type, row.id), row.symbol, parent);
            item->setPen(parent->pen());
            item->setBrush(brush(row.symbol, colorScheme));
        } else {
            item->setCost(item->cost() + costs.cost(type, row.id));
        }
        if (item->cost() > costThreshold) {
            toGraphicsItems(costs, type, row.children, item, costThreshold, colorScheme, collapseRecursion);
        }
    }
}

template<typename Tree>
FrameGraphicsItem* parseData(const Data::Costs& costs, int type, const QVector<Tree>& topDownData, double costThreshold,
                             const Settings::ColorScheme& colorScheme, bool collapseRecursion)
{
    const auto totalCost = costs.totalCost(type);

    KColorScheme scheme(QPalette::Active);
    const QPen pen(scheme.foreground().color());

    QString label = i18n("%1 aggregated %2 cost in total", costs.formatCost(type, totalCost), costs.typeName(type));
    auto rootItem = new FrameGraphicsRootItem(totalCost, costs.unit(type), costs.typeName(type), label);
    rootItem->setBrush(scheme.background());
    rootItem->setPen(pen);
    toGraphicsItems(costs, type, topDownData, rootItem, static_cast<double>(totalCost) * costThreshold / 100.,
                    colorScheme, collapseRecursion);
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
               || item->symbol().binary.contains(searchValue, Qt::CaseInsensitive)) {
        result.directCost += item->cost();
        result.matchType = DirectMatch;
    }

    // recurse into the child items, we always need to update all items
    for (auto* child : item->childItems()) {
        auto* childFrame = static_cast<FrameGraphicsItem*>(child);
        auto childMatch = applySearch(childFrame, searchValue);
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
    }
    if (stack.size() <= depth) {
        return false;
    } else if (item->symbol() != stack[stack.size() - 1 - depth]) {
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
    const auto children = rootItem->childItems();
    for (auto* child : children) {
        // reset everything first
        resetIsExternallyHovered(static_cast<FrameGraphicsItem*>(child));

        // then match all stacks
        for (const auto& stack : stacks) {
            if (hoverStack(static_cast<FrameGraphicsItem*>(child), stack, 0)) {
                break;
            }
        }
    }
}
}

void updateFlameGraphColorScheme(FrameGraphicsItem* item, Settings::ColorScheme scheme)
{
    item->setBrush(brush(item->symbol(), scheme));
    const auto children = item->childItems();
    for (const auto& child : children) {
        updateFlameGraphColorScheme(static_cast<FrameGraphicsItem*>(child), scheme);
    }
}

FlameGraph::FlameGraph(QWidget* parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_costSource(new QComboBox(this))
    , m_scene(new QGraphicsScene(this))
    , m_view(new QGraphicsView(this))
    , m_displayLabel(new KSqueezedTextLabel(this))
    , m_searchResultsLabel(new QLabel(this))
    , m_colorSchemeLabel(new QLabel(this))
    , m_colorSchemeSelector(new QComboBox(this))
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

    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_view->setScene(m_scene);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);
    m_view->setFont(QFont(QStringLiteral("monospace")));

    m_backButton = new QPushButton(this);
    m_backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    m_backButton->setToolTip(QStringLiteral("Go back in symbol view history"));
    m_forwardButton = new QPushButton(this);
    m_forwardButton->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    m_forwardButton->setToolTip(QStringLiteral("Go forward in symbol view history"));

    auto bottomUpCheckbox = new QCheckBox(i18n("Bottom-Up View"), this);
    connect(this, &FlameGraph::uiResetRequested, bottomUpCheckbox,
            [bottomUpCheckbox]() { bottomUpCheckbox->setChecked(false); });
    bottomUpCheckbox->setToolTip(i18n(
        "Enable the bottom-up flame graph view. When this is unchecked, the top-down view is enabled by default."));
    bottomUpCheckbox->setChecked(m_showBottomUpData);
    connect(bottomUpCheckbox, &QCheckBox::toggled, this, [this, bottomUpCheckbox] {
        m_showBottomUpData = bottomUpCheckbox->isChecked();
        showData();
    });

    auto collapseRecursionCheckbox = new QCheckBox(i18n("Collapse Recursion"), this);
    connect(this, &FlameGraph::uiResetRequested, collapseRecursionCheckbox,
            [collapseRecursionCheckbox]() { collapseRecursionCheckbox->setChecked(false); });
    collapseRecursionCheckbox->setChecked(m_collapseRecursion);
    collapseRecursionCheckbox->setToolTip(
        i18n("Collapse stack frames for functions calling themselves. "
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
    costThreshold->setValue(DEFAULT_COST_THRESHOLD);
    connect(this, &FlameGraph::uiResetRequested, costThreshold,
            [costThreshold]() { costThreshold->setValue(DEFAULT_COST_THRESHOLD); });
    costThreshold->setSingleStep(0.01);
    costThreshold->setToolTip(
        i18n("<qt>The cost threshold defines a fractional cut-off value. "
             "Items with a relative cost below this value will not be shown in the flame graph. "
             "This is done as an optimization to quickly generate graphs for large data sets with "
             "low memory overhead. If you need more details, decrease the threshold value, or set it to zero.</qt>"));
    connect(costThreshold, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [this](double threshold) {
                m_costThreshold = threshold;
                showData();
            });

    auto costAggregation = new QComboBox(this);
    ResultsUtil::setupResultsAggregation(costAggregation);
    auto costAggregationLabel = new QLabel(tr("Aggregate cost by:"));
    costAggregationLabel->setBuddy(costAggregation);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(i18n("Search..."));
    m_searchInput->setToolTip(i18n("<qt>Search the flame graph for a symbol.</qt>"));
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::textChanged, this, &FlameGraph::setSearchValue);
    connect(this, &FlameGraph::uiResetRequested, this, [this]() { m_searchInput->clear(); });

    auto controls = new QWidget(this);
    controls->setLayout(new QHBoxLayout);
    controls->layout()->setContentsMargins(0, 0, 0, 0);
    controls->layout()->addWidget(m_backButton);
    controls->layout()->addWidget(m_forwardButton);
    controls->layout()->addWidget(m_costSource);
    controls->layout()->addWidget(bottomUpCheckbox);
    controls->layout()->addWidget(collapseRecursionCheckbox);
    controls->layout()->addWidget(costThreshold);
    controls->layout()->addWidget(costAggregationLabel);
    controls->layout()->addWidget(costAggregation);
    controls->layout()->addWidget(m_searchInput);
    controls->layout()->addWidget(m_colorSchemeLabel);
    controls->layout()->addWidget(m_colorSchemeSelector);

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

    m_backAction = KStandardAction::back(this, SLOT(navigateBack()), this);
    addAction(m_backAction);
    connect(m_backButton, &QPushButton::released, m_backAction, &QAction::trigger);

    m_forwardAction = KStandardAction::forward(this, SLOT(navigateForward()), this);
    addAction(m_forwardAction);
    connect(m_forwardButton, &QPushButton::released, m_forwardAction, &QAction::trigger);

    m_resetAction = new QAction(QIcon::fromTheme(QStringLiteral("go-first")), tr("Reset View"), this);
    m_resetAction->setShortcut(Qt::Key_Escape);
    connect(m_resetAction, &QAction::triggered, this, [this]() { selectItem(0); });
    addAction(m_resetAction);
    updateNavigationActions();

    m_colorSchemeLabel->setText(tr("Color Scheme:"));

    m_colorSchemeSelector->addItem(tr("Default"), QVariant::fromValue(Settings::ColorScheme::Default));
    m_colorSchemeSelector->addItem(tr("Binary"), QVariant::fromValue(Settings::ColorScheme::Binary));
    m_colorSchemeSelector->addItem(tr("Kernel"), QVariant::fromValue(Settings::ColorScheme::Kernel));
    m_colorSchemeSelector->addItem(tr("System"), QVariant::fromValue(Settings::ColorScheme::System));

    auto setColorScheme = [this](Settings::ColorScheme scheme) {
        Settings::instance()->setColorScheme(scheme);

        if (m_rootItem) {
            const auto children = m_rootItem->childItems();
            // don't recolor the root item
            for (const auto& child : children) {
                updateFlameGraphColorScheme(static_cast<FrameGraphicsItem*>(child), scheme);
            }
        }
    };

    connect(m_colorSchemeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, setColorScheme] {
        setColorScheme(m_colorSchemeSelector->currentData().value<Settings::ColorScheme>());
    });

    connect(Settings::instance(), &Settings::pathsChanged, this, [setColorScheme] {
        if (Settings::instance()->colorScheme() == Settings::ColorScheme::System) {
            // setColorScheme triggers a redraw
            // we only need to redraw the flamegraph if the current color scheme is affected by the changed paths
            setColorScheme(Settings::ColorScheme::System);
        }
    });
}

FlameGraph::~FlameGraph() = default;

void FlameGraph::setHoveredStacks(const QVector<QVector<Data::Symbol>>& hoveredStacks)
{
    if (m_hoveredStacks == hoveredStacks) {
        return;
    }

    m_hoveredStacks = hoveredStacks;

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
        } else if (mouseEvent->button() == Qt::BackButton) {
            m_backAction->trigger();
        } else if (mouseEvent->button() == Qt::ForwardButton) {
            m_forwardAction->trigger();
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
        QContextMenuEvent* contextEvent = static_cast<QContextMenuEvent*>(event);
        auto item = static_cast<FrameGraphicsItem*>(m_view->itemAt(m_view->mapFromGlobal(contextEvent->globalPos())));

        QMenu contextMenu;
        if (item) {
            auto* viewCallerCallee = contextMenu.addAction(tr("View Caller/Callee"));
            connect(viewCallerCallee, &QAction::triggered, this,
                    [this, item]() { emit jumpToCallerCallee(item->symbol()); });
            auto* openEditorAction = contextMenu.addAction(tr("Open in Editor"));
            connect(openEditorAction, &QAction::triggered, this, [this, item]() { emit openEditor(item->symbol()); });
            contextMenu.addSeparator();
            auto* viewDisassembly = contextMenu.addAction(tr("Disassembly"));
            connect(viewDisassembly, &QAction::triggered, this,
                    [this, item]() { emit jumpToDisassembly(item->symbol()); });

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
    bool collapseRecursion = m_collapseRecursion;
    auto type = m_costSource->currentData().value<int>();
    auto threshold = m_costThreshold;
    const auto colorScheme = Settings::instance()->colorScheme();
    stream() << make_job(
        [showBottomUpData, bottomUpData, topDownData, type, threshold, colorScheme, collapseRecursion, this]() {
            FrameGraphicsItem* parsedData = nullptr;
            if (showBottomUpData) {
                parsedData = parseData(bottomUpData.costs, type, bottomUpData.root.children, threshold, colorScheme,
                                       collapseRecursion);
            } else {
                parsedData = parseData(topDownData.inclusiveCosts, type, topDownData.root.children, threshold,
                                       colorScheme, collapseRecursion);
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
        QVector<Data::Symbol> stack;
        stack.reserve(32);
        while (item && item != m_rootItem) {
            stack.append(item->symbol());
            item = static_cast<const FrameGraphicsItem*>(item->parentItem());
        }
        emit selectStack(stack);
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

    if (!m_searchInput->text().isEmpty()) {
        setSearchValue(m_searchInput->text());
    }
    if (!m_hoveredStacks.isEmpty()) {
        hoverStacks(rootItem, m_hoveredStacks);
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
            foreach (auto sibling, parent->parentItem()->childItems()) {
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

void FlameGraph::setSearchValue(const QString& value)
{
    if (!m_rootItem) {
        return;
    }

    auto match = applySearch(m_rootItem, value);

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
    m_backButton->setEnabled(hasItems);
    m_forwardButton->setEnabled(isNotLastItem);
}
