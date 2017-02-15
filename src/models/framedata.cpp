/*
  framedata.cpp

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

#include "framedata.h"

namespace {
void setParents(QVector<FrameData>* children, const FrameData* parent)
{
    for (auto& frame : *children) {
        frame.parent = parent;
        setParents(&frame.children, &frame);
    }
}
}

void FrameData::initializeParents(FrameData* tree)
{
    // root has no parent
    Q_ASSERT(tree->parent == nullptr);
    // but neither do the top items have a parent. those belong to the "root" above
    // which has a different address for every model since we use value semantics
    setParents(&tree->children, nullptr);
}
