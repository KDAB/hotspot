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
        if (!m_searchText.isEmpty() && !selectedIndexes.contains(index)) {
            QString text = index.model()->data(index, Qt::DisplayRole).toString();

            QTextDocument *document = new QTextDocument(text);
            QTextCharFormat selection;

            int position = 0;
            QTextCursor cursor;
            do {
                cursor = document->find(m_searchText, position);
                cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
                cursor.selectionStart();
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                cursor.selectionEnd();
                position = cursor.position();
                selection.setForeground(Qt::white);
                selection.setBackground(option.palette.highlight());
                cursor.setCharFormat(selection);
            } while (!cursor.isNull());

            painter->save();
            document->setDefaultFont(painter->font());

            int offset_y = (option.rect.height() - document->size().height()) / 2;
            painter->translate(option.rect.x(), option.rect.y() + offset_y);
            document->drawContents(painter);
            painter->restore();
            delete document;
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    } else {
        costDelegate->paint(painter, option, index);
    }
}
