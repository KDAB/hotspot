/*
  hotspot-config.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#ifndef HOTSPOT_CONFIG_H
#define HOTSPOT_CONFIG_H

#define HOTSPOT_VERSION_STRING "@HOTSPOT_VERSION_STRING@"
#define HOTSPOT_VERSION_MAJOR @hotspot_VERSION_MAJOR@
#define HOTSPOT_VERSION_MINOR @hotspot_VERSION_MINOR@
#define HOTSPOT_VERSION_PATCH @hotspot_VERSION_PATCH@
#define HOTSPOT_VERSION ((hotspot_VERSION_MAJOR<<16)|(hotspot_VERSION_MINOR<<8)|(hotspot_VERSION_PATCH))

#define HOTSPOT_LIBEXEC_REL_PATH "@LIBEXEC_REL_PATH@"

#cmakedefine01 APPIMAGE_BUILD

#endif // HOTSPOT_CONFIG_H
