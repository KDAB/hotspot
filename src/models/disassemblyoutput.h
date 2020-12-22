/*
  disassemblyoutput.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Darya Knysh <d.knysh@nips.ru>

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

#include <QString>

namespace Data {
    struct Symbol;
}

struct DisassemblyOutput
{
    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
    };
    QVector<DisassemblyLine> disassemblyLines;

    QString errorMessage;
    explicit operator bool() const
    {
        return errorMessage.isEmpty();
    }

    static DisassemblyOutput disassemble(const QString& objdump, const QString& arch, const Data::Symbol& symbol);
};
Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);
