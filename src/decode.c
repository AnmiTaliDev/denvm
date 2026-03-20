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

#include "denvm/decode.h"
#include "denvm/nvm.h"

typedef struct {
    uint8_t        opcode;
    const char    *name;
    operand_type_t operand_type;
} opcode_info_t;

static const opcode_info_t opcode_table[] = {
    { OP_HALT,       "halt",       OPERAND_NONE },
    { OP_NOP,        "nop",        OPERAND_NONE },
    { OP_PUSH,       "push",       OPERAND_U32  },
    { OP_POP,        "pop",        OPERAND_NONE },
    { OP_DUP,        "dup",        OPERAND_NONE },
    { OP_SWAP,       "swap",       OPERAND_NONE },
    { OP_ADD,        "add",        OPERAND_NONE },
    { OP_SUB,        "sub",        OPERAND_NONE },
    { OP_MUL,        "mul",        OPERAND_NONE },
    { OP_DIV,        "div",        OPERAND_NONE },
    { OP_MOD,        "mod",        OPERAND_NONE },
    { OP_CMP,        "cmp",        OPERAND_NONE },
    { OP_EQ,         "eq",         OPERAND_NONE },
    { OP_NEQ,        "neq",        OPERAND_NONE },
    { OP_GT,         "gt",         OPERAND_NONE },
    { OP_LT,         "lt",         OPERAND_NONE },
    { OP_JMP,        "jmp",        OPERAND_U32  },
    { OP_JZ,         "jz",         OPERAND_U32  },
    { OP_JNZ,        "jnz",        OPERAND_U32  },
    { OP_CALL,       "call",       OPERAND_U32  },
    { OP_RET,        "ret",        OPERAND_NONE },
    { OP_ENTER,      "enter",      OPERAND_U8   },
    { OP_LEAVE,      "leave",      OPERAND_NONE },
    { OP_LOAD_ARG,   "load_arg",   OPERAND_U8   },
    { OP_STORE_ARG,  "store_arg",  OPERAND_U8   },
    { OP_LOAD,       "load",       OPERAND_U8   },
    { OP_STORE,      "store",      OPERAND_U8   },
    { OP_LOAD_REL,   "load_rel",   OPERAND_U8   },
    { OP_STORE_REL,  "store_rel",  OPERAND_U8   },
    { OP_LOAD_ABS,   "load_abs",   OPERAND_NONE },
    { OP_STORE_ABS,  "store_abs",  OPERAND_NONE },
    { OP_SYSCALL,    "syscall",    OPERAND_U8   },
    { OP_BREAK,      "break",      OPERAND_NONE },
};

#define OPCODE_TABLE_SIZE (sizeof(opcode_table) / sizeof(opcode_table[0]))

static const opcode_info_t *find_opcode_info(uint8_t opcode)
{
    for (size_t i = 0; i < OPCODE_TABLE_SIZE; i++) {
        if (opcode_table[i].opcode == opcode)
            return &opcode_table[i];
    }
    return NULL;
}

operand_type_t nvm_opcode_operand_type(uint8_t opcode)
{
    const opcode_info_t *info = find_opcode_info(opcode);
    return info ? info->operand_type : OPERAND_NONE;
}

const char *nvm_opcode_name(uint8_t opcode)
{
    const opcode_info_t *info = find_opcode_info(opcode);
    return info ? info->name : NULL;
}

int nvm_opcode_known(uint8_t opcode)
{
    return find_opcode_info(opcode) != NULL;
}

const char *decode_strerror(decode_error_t err)
{
    switch (err) {
    case DECODE_OK:              return "success";
    case DECODE_ERR_ALLOC:       return "memory allocation failed";
    case DECODE_ERR_TRUNCATED:   return "truncated instruction at end of bytecode";
    default:                     return "unknown error";
    }
}

static int insn_list_grow(nvm_insn_list_t *list)
{
    size_t new_cap = list->capacity ? list->capacity * 2 : 64;
    nvm_insn_t *new_insns = realloc(list->insns, new_cap * sizeof(nvm_insn_t));
    if (!new_insns)
        return 0;
    list->insns = new_insns;
    list->capacity = new_cap;
    return 1;
}

decode_error_t nvm_decode(const uint8_t *bytecode, size_t size, nvm_insn_list_t *out)
{
    memset(out, 0, sizeof(*out));

    if (size <= NVM_HEADER_SIZE)
        return DECODE_OK;

    size_t ip = NVM_HEADER_SIZE;

    while (ip < size) {
        if (out->count >= out->capacity) {
            if (!insn_list_grow(out)) {
                nvm_insn_list_free(out);
                return DECODE_ERR_ALLOC;
            }
        }

        nvm_insn_t insn = {0};
        insn.offset = (uint32_t)ip;
        insn.opcode = bytecode[ip];
        ip++;

        const opcode_info_t *info = find_opcode_info(insn.opcode);
        insn.known = (info != NULL);

        if (info) {
            insn.operand_type = info->operand_type;
        } else {
            insn.operand_type = OPERAND_NONE;
        }

        switch (insn.operand_type) {
        case OPERAND_NONE:
            insn.size = 1;
            break;

        case OPERAND_U8:
            if (ip >= size) {
                insn.size = 1;
                out->insns[out->count++] = insn;
                return DECODE_ERR_TRUNCATED;
            }
            insn.operand = bytecode[ip];
            insn.size = 2;
            ip++;
            break;

        case OPERAND_U32:
            if (ip + 3 >= size) {
                insn.size = 1;
                out->insns[out->count++] = insn;
                return DECODE_ERR_TRUNCATED;
            }
            insn.operand = ((uint32_t)bytecode[ip]     << 24) |
                           ((uint32_t)bytecode[ip + 1] << 16) |
                           ((uint32_t)bytecode[ip + 2] <<  8) |
                            (uint32_t)bytecode[ip + 3];
            insn.size = 5;
            ip += 4;
            break;
        }

        out->insns[out->count++] = insn;
    }

    return DECODE_OK;
}

void nvm_insn_list_free(nvm_insn_list_t *list)
{
    if (!list)
        return;
    free(list->insns);
    list->insns = NULL;
    list->count = 0;
    list->capacity = 0;
}
