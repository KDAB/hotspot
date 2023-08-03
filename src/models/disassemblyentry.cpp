/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "disassemblyentry.h"

DisassemblyEntry::DisassemblyEntry(DisassemblyEntry* parent, const DisassemblyOutput::DisassemblyLine& disassemblyLine,
                                   QTextLine textLine)
    : m_parent(parent)
    , m_disassemblyLine(disassemblyLine)
    , m_textLine(textLine)
{
    if (parent) {
        m_row = m_parent->childCount();
    } else {
        m_row = 0;
    }
}

DisassemblyEntry* DisassemblyEntry::lastChild()
{
    if (m_lines.isEmpty()) {
        return nullptr;
    }

    return &m_lines[m_lines.size() - 1];
}

void DisassemblyEntry::addChild(const DisassemblyEntry& line)
{
    m_lines.push_back(line);
}

void DisassemblyEntry::clear()
{
    m_lines.clear();
}

bool DisassemblyEntry::operator==(const DisassemblyEntry& other) const
{
    return std::tie(m_parent, m_lines, m_disassemblyLine, m_row)
        == std::tie(other.m_parent, other.m_lines, other.m_disassemblyLine, other.m_row);
}

int DisassemblyEntry::findOffsetOf(const DisassemblyEntry* entry) const
{
    auto found = std::find_if(m_lines.begin(), m_lines.end(),
                              [needle = entry](const DisassemblyEntry& entry) { return needle == &entry; });

    if (found != m_lines.end()) {
        return std::distance(m_lines.begin(), found);
    }

    return -1;
}

DisassemblyEntry::TreeIterator::TreeIterator(const DisassemblyEntry* entry, int child)
    : m_entry(const_cast<DisassemblyEntry*>(entry))
    , m_child(child)
{
}

DisassemblyEntry& DisassemblyEntry::TreeIterator::operator*() const
{
    if (m_entry->childCount() == 0) {
        return *m_entry;
    }
    return *m_entry->child(m_child);
}

DisassemblyEntry* DisassemblyEntry::TreeIterator::operator->() const
{
    if (m_entry->childCount() == 0) {
        return m_entry;
    }
    return m_entry->child(m_child);
}

DisassemblyEntry::TreeIterator& DisassemblyEntry::TreeIterator::operator++()
{
    if (m_entry->childCount() == 0) {
        m_entry++;
        return *this;
    }

    m_child++;

    if (m_child >= m_entry->childCount()) {
        m_entry++;
        m_child = 0;
    }

    return *this;
}

bool DisassemblyEntry::TreeIterator::operator==(const DisassemblyEntry::TreeIterator other) const
{
    return this->m_entry == other.m_entry && this->m_child == other.m_child;
}

DisassemblyEntry::TreeIterator DisassemblyEntry::tree_begin() const
{
    return TreeIterator(&m_lines[0], 0);
}

DisassemblyEntry::TreeIterator DisassemblyEntry::tree_end() const
{
    return TreeIterator(m_lines.end(), 0);
}
