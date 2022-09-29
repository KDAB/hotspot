/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class SharedLib
{
    unsigned long m_max;

public:
    SharedLib(unsigned long max)
        : m_max(max)
    {
    }

    double foo() const;

private:
    double bar(unsigned long) const;
};
