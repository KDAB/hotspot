/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cstdio>
#include <dwarf.h>
#include <libdw.h>
#include <libdwfl.h>

#include <vector>

namespace {

struct DieRanges
{
    Dwarf_Die die;
    struct Range
    {
        Range(Dwarf_Addr low, Dwarf_Addr high)
            : low(low)
            , high(high)
        {
        }
        Dwarf_Addr low = 0;
        Dwarf_Addr high = 0;
    };
    std::vector<Range> ranges;
    std::vector<DieRanges> children;
};

// see libdw_visit_scopes.c
bool may_have_scopes(Dwarf_Die* die)
{
    switch (dwarf_tag(die)) {
    /* DIEs with addresses we can try to match.  */
    case DW_TAG_compile_unit:
    case DW_TAG_module:
    case DW_TAG_lexical_block:
    case DW_TAG_with_stmt:
    case DW_TAG_catch_block:
    case DW_TAG_try_block:
    case DW_TAG_entry_point:
    case DW_TAG_inlined_subroutine:
    case DW_TAG_subprogram:
        return true;

    /* DIEs without addresses that can own DIEs with addresses.  */
    case DW_TAG_namespace:
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
        return true;

    /* Other DIEs we have no reason to descend.  */
    default:
        break;
    }
    return false;
}

void walk_die(Dwarf_Die* die, DieRanges* parent, int depth)
{
    if (!may_have_scopes(die))
        return;

    if (dwarf_tag(die) == DW_TAG_subprogram) {
        DieRanges ranges;
        ranges.die = *die;

        Dwarf_Addr low = 0;
        Dwarf_Addr high = 0;
        Dwarf_Addr base = 0;
        ptrdiff_t offset = 0;
        while ((offset = dwarf_ranges(die, offset, &base, &low, &high)) > 0) {
            ranges.ranges.emplace_back(low, high);
            printf("  %lx - %lx\n", low, high);
        }

        if (!ranges.ranges.empty()) {
            printf("%6d: %x %s %lx\n", depth, dwarf_tag(die), dwarf_diename(die), dwarf_dieoffset(die));
            parent->children.push_back(ranges);
            parent = &parent->children.back();
            ++depth;
        }
        return;
    }

    Dwarf_Die childDie;
    if (dwarf_child(die, &childDie) == 0) {
        walk_die(&childDie, parent, depth);

        Dwarf_Die siblingDie;
        while (dwarf_siblingof(&childDie, &siblingDie) == 0) {
            walk_die(&siblingDie, parent, depth);
            childDie = siblingDie;
        }
    }
}

DieRanges walk_cudie(Dwarf_Die* cudie)
{
    DieRanges ranges;
    ranges.die = *cudie;
    walk_die(cudie, &ranges, 0);
    return ranges;
}

void walk_cudies(Dwfl_Module* mod)
{
    std::vector<DieRanges> ranges;
    Dwarf_Die* cudie = nullptr;
    Dwarf_Addr bias = 0;
    while ((cudie = dwfl_module_nextcu(mod, cudie, &bias))) {
        ranges.emplace_back(walk_cudie(cudie));
    }
}
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "ERROR: missing <file> arg\n");
        return 1;
    }

    const auto file = argv[1];

    Dwfl_Callbacks callbacks = {
        &dwfl_build_id_find_elf,
        &dwfl_standard_find_debuginfo,
        &dwfl_offline_section_address,
        nullptr,
    };
    auto* dwfl = dwfl_begin(&callbacks);

    dwfl_report_begin(dwfl);
    auto* mod = dwfl_report_elf(dwfl, file, file, -1, 0, false);
    if (mod)
        walk_cudies(mod);
    else
        fprintf(stderr, "ERROR: failed to report elf: %s\n", dwfl_errmsg(dwfl_errno()));
    dwfl_report_end(dwfl, nullptr, nullptr);

    dwfl_end(dwfl);
}
