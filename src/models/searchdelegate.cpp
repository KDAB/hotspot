#include "searchdelegate.h"
#include "costdelegate.h"
#include "disassemblymodel.h"

#include <QDebug>
#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QStyledItemDelegate>

#include <cmath>

SearchDelegate::SearchDelegate(QObject *parent)
        : QStyledItemDelegate(parent) {
    costDelegate = new CostDelegate(SortRole, TotalCostRole, parent);
}

SearchDelegate::~SearchDelegate() = default;

/**
 *  Overriden paint
 * @param painter
 * @param option
 * @param index
 */
void SearchDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (!index.isValid())
        return;

    if (index.column() == 0) {
        QString text = index.model()->data(index, Qt::DisplayRole).toString();
        QTextDocument *document = new QTextDocument(text);

        if (option.state & QStyle::State_Selected) {
            painter->setPen(Qt::white);
            painter->fillRect(option.rect, option.palette.highlight());
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        Highlighter *highlighter = new Highlighter(document);
        highlighter->setSearchText(m_searchText);
        highlighter->setArch(m_arch);
        highlighter->setCallee(m_callees.contains(index.row()));
        highlighter->setDiagnosticStyle(m_diagnosticStyle);
        highlighter->setHighlightColor(option.palette.highlight());

        highlighter->rehighlight();

        painter->save();
        document->setDefaultFont(painter->font());
        painter->setClipRect(option.rect.x(), option.rect.y(), option.rect.width(), option.rect.height());

        int offset_y = (option.rect.height() - document->size().height()) / 2;
        painter->translate(option.rect.x(), option.rect.y() + offset_y);
        document->drawContents(painter);
        painter->restore();
        delete document;
    } else {
        costDelegate->paint(painter, option, index);
    }
}
