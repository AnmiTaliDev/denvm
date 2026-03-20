/*
 * Copyright 2026 AnmiTaliDev <anmitalidev@nuros.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include "denvm/xref.h"
#include "denvm/nvm.h"

static int xref_table_grow(xref_table_t *table)
{
    size_t new_cap = table->capacity ? table->capacity * 2 : 16;
    xref_entry_t *new_entries = realloc(table->entries, new_cap * sizeof(xref_entry_t));
    if (!new_entries)
        return 0;
    table->entries = new_entries;
    table->capacity = new_cap;
    return 1;
}

static int xref_add(xref_table_t *table, uint32_t source, uint32_t target, xref_type_t type)
{
    if (table->count >= table->capacity) {
        if (!xref_table_grow(table))
            return 0;
    }
    table->entries[table->count].source = source;
    table->entries[table->count].target = target;
    table->entries[table->count].type   = type;
    table->count++;
    return 1;
}

xref_error_t xref_build(const nvm_insn_list_t *insns, xref_table_t *out)
{
    memset(out, 0, sizeof(*out));

    for (size_t i = 0; i < insns->count; i++) {
        const nvm_insn_t *insn = &insns->insns[i];

        switch (insn->opcode) {
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
            if (!xref_add(out, insn->offset, insn->operand, XREF_JUMP)) {
                xref_free(out);
                return XREF_ERR_ALLOC;
            }
            break;

        case OP_CALL:
            if (!xref_add(out, insn->offset, insn->operand, XREF_CALL)) {
                xref_free(out);
                return XREF_ERR_ALLOC;
            }
            break;

        default:
            break;
        }
    }

    return XREF_OK;
}

void xref_free(xref_table_t *table)
{
    if (!table)
        return;
    free(table->entries);
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

int xref_is_target(const xref_table_t *table, uint32_t offset)
{
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].target == offset)
            return 1;
    }
    return 0;
}

int xref_is_call_target(const xref_table_t *table, uint32_t offset)
{
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].target == offset && table->entries[i].type == XREF_CALL)
            return 1;
    }
    return 0;
}

xref_type_t xref_target_type(const xref_table_t *table, uint32_t offset)
{
    int has_call = 0;
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].target == offset) {
            if (table->entries[i].type == XREF_CALL)
                has_call = 1;
        }
    }
    return has_call ? XREF_CALL : XREF_JUMP;
}
