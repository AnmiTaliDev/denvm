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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "denvm/decode.h"
#include "denvm/nvm.h"

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void)

#define RUN(name) \
    do { \
        tests_run++; \
        printf("  %-50s", #name); \
        test_##name(); \
        tests_passed++; \
        printf("ok\n"); \
    } while (0)

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL\n    assertion failed: %s  (%s:%d)\n", \
                   #cond, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

TEST(empty_bytecode)
{
    static const uint8_t bc[] = { 0x4E, 0x56, 0x4D, 0x30 };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 0);
    nvm_insn_list_free(&list);
}

TEST(single_halt)
{
    static const uint8_t bc[] = { 0x4E, 0x56, 0x4D, 0x30, 0x00 };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].opcode == OP_HALT);
    ASSERT(list.insns[0].offset == 4);
    ASSERT(list.insns[0].size == 1);
    ASSERT(list.insns[0].operand_type == OPERAND_NONE);
    ASSERT(list.insns[0].known == 1);
    nvm_insn_list_free(&list);
}

TEST(push_big_endian)
{
    /* PUSH 0xDEADBEEF */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x02, 0xDE, 0xAD, 0xBE, 0xEF
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].opcode == OP_PUSH);
    ASSERT(list.insns[0].operand == 0xDEADBEEF);
    ASSERT(list.insns[0].size == 5);
    ASSERT(list.insns[0].operand_type == OPERAND_U32);
    nvm_insn_list_free(&list);
}

TEST(push_zero)
{
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x02, 0x00, 0x00, 0x00, 0x00
    };
    nvm_insn_list_t list = {0};
    nvm_decode(bc, sizeof(bc), &list);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].operand == 0);
    nvm_insn_list_free(&list);
}

TEST(u8_operand_syscall)
{
    /* SYSCALL 0x0E (SYS_PRINT) */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x50, 0x0E
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].opcode == OP_SYSCALL);
    ASSERT(list.insns[0].operand == 0x0E);
    ASSERT(list.insns[0].size == 2);
    ASSERT(list.insns[0].operand_type == OPERAND_U8);
    nvm_insn_list_free(&list);
}

TEST(enter_operand)
{
    /* ENTER 3 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x35, 0x03
    };
    nvm_insn_list_t list = {0};
    nvm_decode(bc, sizeof(bc), &list);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].opcode == OP_ENTER);
    ASSERT(list.insns[0].operand == 3);
    ASSERT(list.insns[0].operand_type == OPERAND_U8);
    nvm_insn_list_free(&list);
}

TEST(multi_instruction_sequence)
{
    /* PUSH 72, SYSCALL print, HALT */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x02, 0x00, 0x00, 0x00, 0x48,
        0x50, 0x0E,
        0x00
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 3);

    ASSERT(list.insns[0].opcode == OP_PUSH);
    ASSERT(list.insns[0].offset == 4);
    ASSERT(list.insns[0].operand == 0x48);

    ASSERT(list.insns[1].opcode == OP_SYSCALL);
    ASSERT(list.insns[1].offset == 9);
    ASSERT(list.insns[1].operand == 0x0E);

    ASSERT(list.insns[2].opcode == OP_HALT);
    ASSERT(list.insns[2].offset == 11);

    nvm_insn_list_free(&list);
}

TEST(unknown_opcode)
{
    /* opcode 0xFF is not defined in NVM */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0xFF
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].known == 0);
    ASSERT(list.insns[0].size == 1);
    nvm_insn_list_free(&list);
}

TEST(truncated_push)
{
    /* PUSH with only 2 bytes of operand instead of 4 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x02, 0x00, 0x01
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_ERR_TRUNCATED);
    nvm_insn_list_free(&list);
}

TEST(truncated_syscall)
{
    /* SYSCALL with no operand byte */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x50
    };
    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_ERR_TRUNCATED);
    nvm_insn_list_free(&list);
}

TEST(all_no_operand_opcodes)
{
    static const uint8_t opcodes[] = {
        OP_HALT, OP_NOP, OP_POP, OP_DUP, OP_SWAP,
        OP_ADD,  OP_SUB, OP_MUL, OP_DIV, OP_MOD,
        OP_CMP,  OP_EQ,  OP_NEQ, OP_GT,  OP_LT,
        OP_RET,  OP_LEAVE, OP_LOAD_ABS, OP_STORE_ABS, OP_BREAK
    };

    uint8_t bc[4 + sizeof(opcodes)];
    bc[0] = 0x4E; bc[1] = 0x56; bc[2] = 0x4D; bc[3] = 0x30;
    for (size_t i = 0; i < sizeof(opcodes); i++)
        bc[4 + i] = opcodes[i];

    nvm_insn_list_t list = {0};
    decode_error_t err = nvm_decode(bc, sizeof(bc), &list);
    ASSERT(err == DECODE_OK);
    ASSERT(list.count == sizeof(opcodes));

    for (size_t i = 0; i < sizeof(opcodes); i++) {
        ASSERT(list.insns[i].operand_type == OPERAND_NONE);
        ASSERT(list.insns[i].size == 1);
        ASSERT(list.insns[i].known == 1);
    }

    nvm_insn_list_free(&list);
}

TEST(jmp_offset)
{
    /* JMP to absolute address 0x00000010 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x30, 0x00, 0x00, 0x00, 0x10
    };
    nvm_insn_list_t list = {0};
    nvm_decode(bc, sizeof(bc), &list);
    ASSERT(list.count == 1);
    ASSERT(list.insns[0].opcode == OP_JMP);
    ASSERT(list.insns[0].operand == 0x10);
    ASSERT(list.insns[0].operand_type == OPERAND_U32);
    nvm_insn_list_free(&list);
}

TEST(opcode_names)
{
    ASSERT(strcmp(nvm_opcode_name(OP_HALT),      "halt")      == 0);
    ASSERT(strcmp(nvm_opcode_name(OP_PUSH),      "push")      == 0);
    ASSERT(strcmp(nvm_opcode_name(OP_SYSCALL),   "syscall")   == 0);
    ASSERT(strcmp(nvm_opcode_name(OP_CALL),      "call")      == 0);
    ASSERT(strcmp(nvm_opcode_name(OP_LOAD_ARG),  "load_arg")  == 0);
    ASSERT(strcmp(nvm_opcode_name(OP_STORE_REL), "store_rel") == 0);
    ASSERT(nvm_opcode_name(0xFF) == NULL);
}

TEST(opcode_known)
{
    ASSERT(nvm_opcode_known(OP_HALT)   == 1);
    ASSERT(nvm_opcode_known(OP_PUSH)   == 1);
    ASSERT(nvm_opcode_known(OP_BREAK)  == 1);
    ASSERT(nvm_opcode_known(0x03)      == 0);
    ASSERT(nvm_opcode_known(0xFF)      == 0);
    ASSERT(nvm_opcode_known(0x07)      == 0);
}

TEST(operand_types)
{
    ASSERT(nvm_opcode_operand_type(OP_HALT)      == OPERAND_NONE);
    ASSERT(nvm_opcode_operand_type(OP_NOP)       == OPERAND_NONE);
    ASSERT(nvm_opcode_operand_type(OP_PUSH)      == OPERAND_U32);
    ASSERT(nvm_opcode_operand_type(OP_JMP)       == OPERAND_U32);
    ASSERT(nvm_opcode_operand_type(OP_JZ)        == OPERAND_U32);
    ASSERT(nvm_opcode_operand_type(OP_JNZ)       == OPERAND_U32);
    ASSERT(nvm_opcode_operand_type(OP_CALL)      == OPERAND_U32);
    ASSERT(nvm_opcode_operand_type(OP_SYSCALL)   == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_ENTER)     == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_LOAD)      == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_STORE)     == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_LOAD_REL)  == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_STORE_REL) == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_LOAD_ARG)  == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_STORE_ARG) == OPERAND_U8);
    ASSERT(nvm_opcode_operand_type(OP_LOAD_ABS)  == OPERAND_NONE);
    ASSERT(nvm_opcode_operand_type(OP_STORE_ABS) == OPERAND_NONE);
}

int main(void)
{
    printf("decode\n");
    RUN(empty_bytecode);
    RUN(single_halt);
    RUN(push_big_endian);
    RUN(push_zero);
    RUN(u8_operand_syscall);
    RUN(enter_operand);
    RUN(multi_instruction_sequence);
    RUN(unknown_opcode);
    RUN(truncated_push);
    RUN(truncated_syscall);
    RUN(all_no_operand_opcodes);
    RUN(jmp_offset);
    RUN(opcode_names);
    RUN(opcode_known);
    RUN(operand_types);

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
