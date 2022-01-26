/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemblydelegate.h"

#include <QEvent>
#include <QMouseEvent>

#include "disassemblymodel.h"

DisassemblyDelegate::DisassemblyDelegate(QObject* parent)
    : QItemDelegate(parent)
{
}

DisassemblyDelegate::~DisassemblyDelegate() = default;

bool DisassemblyDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& /*option*/,
                                      const QModelIndex& index)
{
    if (event->type() != QMouseEvent::MouseButtonPress)
        return false;

    const QString functionName =
        model->data(model->index(index.row(), DisassemblyModel::LinkedFunctionName), Qt::DisplayRole).toString();
    if (functionName.isEmpty())
        return false;

    int funtionOffset =
        model->data(model->index(index.row(), DisassemblyModel::LinkedFunctionOffset), Qt::DisplayRole).toInt();
    emit gotoFunction(functionName, funtionOffset);

    return true;
}
