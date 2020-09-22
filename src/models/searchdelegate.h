#pragma once

#include <QStyledItemDelegate>
#include "costdelegate.h"

class SearchDelegate : public QStyledItemDelegate {
Q_OBJECT
public:
    SearchDelegate(QObject *parent = nullptr);

    ~SearchDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setSearchText(QString text) {
        m_searchText = text;
    }

    QString getSearchText() {
        return m_searchText;
    }

    const QModelIndexList getSelectedIndexes() const {
        return selectedIndexes;
    }

    void setSelectedIndexes(const QModelIndexList &selectedIndexes) {
        SearchDelegate::selectedIndexes = selectedIndexes;
    }

private:
    QString m_searchText;
    QModelIndexList selectedIndexes;
    CostDelegate *costDelegate;
};
