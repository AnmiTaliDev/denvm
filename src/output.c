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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "denvm/decode.h"
#include "denvm/loader.h"
#include "denvm/nvm.h"
#include "denvm/output.h"
#include "denvm/xref.h"

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_RED     "\033[31m"
#define ANSI_BLUE    "\033[34m"

static const char *syscall_name(uint8_t id)
{
    switch (id) {
    case SYS_EXIT:           return "exit";
    case SYS_SPAWN:          return "spawn";
    case SYS_OPEN:           return "open";
    case SYS_READ:           return "read";
    case SYS_WRITE:          return "write";
    case SYS_MSG_SEND:       return "msg_send";
    case SYS_MSG_RECEIVE:    return "msg_receive";
    case SYS_PORT_IN_BYTE:   return "port_in_byte";
    case SYS_PORT_OUT_BYTE:  return "port_out_byte";
    case SYS_PRINT:          return "print";
    default:                 return NULL;
    }
}

static const char *syscall_cap_comment(uint8_t id)
{
    switch (id) {
    case SYS_SPAWN:          return "requires CAP_FS_READ";
    case SYS_OPEN:           return "requires CAP_FS_READ";
    case SYS_READ:           return "requires CAP_FS_READ";
    case SYS_WRITE:          return "requires CAP_FS_WRITE";
    case SYS_PORT_IN_BYTE:   return "requires CAP_DRV_ACCESS";
    case SYS_PORT_OUT_BYTE:  return "requires CAP_DRV_ACCESS";
    default:                 return NULL;
    }
}

static void print_hex_bytes(FILE *fp, const uint8_t *data, uint32_t offset, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        fprintf(fp, "%02x ", data[offset + i]);
    for (uint32_t i = size; i < 6; i++)
        fprintf(fp, "   ");
}

static void print_label(FILE *fp, uint32_t offset, xref_type_t type, int color)
{
    if (color) {
        if (type == XREF_CALL)
            fprintf(fp, "%s%ssub_%04x%s:\n", ANSI_BOLD, ANSI_GREEN, offset, ANSI_RESET);
        else
            fprintf(fp, "%s%sloc_%04x%s:\n", ANSI_BOLD, ANSI_YELLOW, offset, ANSI_RESET);
    } else {
        if (type == XREF_CALL)
            fprintf(fp, "sub_%04x:\n", offset);
        else
            fprintf(fp, "loc_%04x:\n", offset);
    }
}

static void print_operand_xref(FILE *fp, uint32_t target, xref_type_t type, int color)
{
    if (color) {
        if (type == XREF_CALL)
            fprintf(fp, "%s%ssub_%04x%s", ANSI_BOLD, ANSI_GREEN, target, ANSI_RESET);
        else
            fprintf(fp, "%s%sloc_%04x%s", ANSI_BOLD, ANSI_YELLOW, target, ANSI_RESET);
    } else {
        if (type == XREF_CALL)
            fprintf(fp, "sub_%04x", target);
        else
            fprintf(fp, "loc_%04x", target);
    }
}

static void print_comment(FILE *fp, const char *text, int color)
{
    if (color)
        fprintf(fp, "%s; %s%s", ANSI_DIM, text, ANSI_RESET);
    else
        fprintf(fp, "; %s", text);
}

static int is_printable_ascii(uint32_t value)
{
    return value >= 0x20 && value <= 0x7E;
}

void output_disassembly(FILE *fp,
                        const nvm_binary_t *bin,
                        const nvm_insn_list_t *insns,
                        const xref_table_t *xrefs,
                        const output_options_t *opts)
{
    int color = opts->color;

    for (size_t i = 0; i < insns->count; i++) {
        const nvm_insn_t *insn = &insns->insns[i];

        if (xref_is_target(xrefs, insn->offset)) {
            if (i > 0)
                fprintf(fp, "\n");
            xref_type_t ltype = xref_target_type(xrefs, insn->offset);
            print_label(fp, insn->offset, ltype, color);
        }

        if (opts->show_offsets) {
            if (color)
                fprintf(fp, "  %s0x%04x%s  ", ANSI_DIM, insn->offset, ANSI_RESET);
            else
                fprintf(fp, "  0x%04x  ", insn->offset);
        } else {
            fprintf(fp, "  ");
        }

        if (opts->show_hex) {
            print_hex_bytes(fp, bin->data, insn->offset, insn->size);
            fprintf(fp, " ");
        }

        if (!insn->known) {
            if (color)
                fprintf(fp, "%s.db%s     0x%02x", ANSI_RED, ANSI_RESET, insn->opcode);
            else
                fprintf(fp, ".db     0x%02x", insn->opcode);
            if (opts->show_comments) {
                fprintf(fp, "  ");
                print_comment(fp, "unknown opcode", color);
            }
            fprintf(fp, "\n");
            continue;
        }

        const char *mnemonic = nvm_opcode_name(insn->opcode);

        if (color) {
            switch (insn->opcode) {
            case OP_HALT:
            case OP_BREAK:
                fprintf(fp, "%s%s%-12s%s", ANSI_BOLD, ANSI_RED, mnemonic, ANSI_RESET);
                break;
            case OP_CALL:
            case OP_RET:
                fprintf(fp, "%s%s%-12s%s", ANSI_BOLD, ANSI_CYAN, mnemonic, ANSI_RESET);
                break;
            case OP_JMP:
            case OP_JZ:
            case OP_JNZ:
                fprintf(fp, "%s%-12s%s", ANSI_CYAN, mnemonic, ANSI_RESET);
                break;
            case OP_SYSCALL:
                fprintf(fp, "%s%-12s%s", ANSI_MAGENTA, mnemonic, ANSI_RESET);
                break;
            default:
                fprintf(fp, "%-12s", mnemonic);
                break;
            }
        } else {
            fprintf(fp, "%-12s", mnemonic);
        }

        char comment[256] = {0};

        switch (insn->operand_type) {
        case OPERAND_NONE:
            break;

        case OPERAND_U8:
            if (insn->opcode == OP_SYSCALL) {
                const char *sname = syscall_name((uint8_t)insn->operand);
                if (sname) {
                    if (color)
                        fprintf(fp, "%s%s%s", ANSI_MAGENTA, sname, ANSI_RESET);
                    else
                        fprintf(fp, "%s", sname);
                    const char *cap = syscall_cap_comment((uint8_t)insn->operand);
                    if (cap && opts->show_comments)
                        snprintf(comment, sizeof(comment), "%s", cap);
                } else {
                    fprintf(fp, "0x%02x", insn->operand);
                    if (opts->show_comments)
                        snprintf(comment, sizeof(comment), "unknown syscall");
                }
            } else {
                fprintf(fp, "%-4u", insn->operand);
                if (opts->show_comments) {
                    if (insn->opcode == OP_ENTER)
                        snprintf(comment, sizeof(comment), "%u local slot%s",
                                 insn->operand, insn->operand == 1 ? "" : "s");
                }
            }
            break;

        case OPERAND_U32:
            if (insn->opcode == OP_JMP || insn->opcode == OP_JZ || insn->opcode == OP_JNZ) {
                print_operand_xref(fp, insn->operand, XREF_JUMP, color);
            } else if (insn->opcode == OP_CALL) {
                print_operand_xref(fp, insn->operand, XREF_CALL, color);
            } else {
                if (color)
                    fprintf(fp, "%s0x%08x%s", ANSI_BLUE, insn->operand, ANSI_RESET);
                else
                    fprintf(fp, "0x%08x", insn->operand);
                if (opts->show_comments && insn->opcode == OP_PUSH) {
                    if (is_printable_ascii(insn->operand))
                        snprintf(comment, sizeof(comment), "'%c'", (char)insn->operand);
                    else if (insn->operand == 0)
                        snprintf(comment, sizeof(comment), "null");
                }
            }
            break;
        }

        if (opts->show_comments && comment[0]) {
            int pad = 14 - (int)strlen(comment);
            if (pad < 2) pad = 2;
            fprintf(fp, "%*s", pad, " ");
            print_comment(fp, comment, color);
        }

        fprintf(fp, "\n");
    }
}

void output_hex_dump(FILE *fp, const nvm_binary_t *bin)
{
    const uint8_t *data = bin->data;
    size_t size = bin->size;

    for (size_t i = 0; i < size; i += 16) {
        fprintf(fp, "  %04zx  ", i);

        for (size_t j = i; j < i + 16; j++) {
            if (j < size)
                fprintf(fp, "%02x ", data[j]);
            else
                fprintf(fp, "   ");
            if (j == i + 7)
                fprintf(fp, " ");
        }

        fprintf(fp, " |");
        for (size_t j = i; j < i + 16 && j < size; j++)
            fprintf(fp, "%c", isprint(data[j]) ? (char)data[j] : '.');
        fprintf(fp, "|\n");
    }
}

void output_header_info(FILE *fp, const nvm_binary_t *bin, const output_options_t *opts)
{
    int color = opts->color;

    if (color)
        fprintf(fp, "%sFile:%s    %s\n", ANSI_BOLD, ANSI_RESET, bin->path);
    else
        fprintf(fp, "File:    %s\n", bin->path);

    fprintf(fp, "Size:    %zu bytes\n", bin->size);
    fprintf(fp, "Magic:   NVM0 (valid)\n");

    if (bin->size > NVM_HEADER_SIZE)
        fprintf(fp, "Code:    %zu bytes (0x%04x - 0x%04zx)\n",
                bin->size - NVM_HEADER_SIZE,
                NVM_HEADER_SIZE,
                bin->size - 1);
    else
        fprintf(fp, "Code:    empty\n");

    fprintf(fp, "\n");
}
