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

#include "denvm/decode.h"
#include "denvm/nvm.h"
#include "denvm/xref.h"

static int tests_run    = 0;
static int tests_passed = 0;

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

#define TEST(name) static void test_##name(void)

static void decode_from_bytes(const uint8_t *bc, size_t size, nvm_insn_list_t *out)
{
    nvm_decode(bc, size, out);
}

TEST(empty_no_xrefs)
{
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x00
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_error_t err = xref_build(&insns, &xrefs);
    ASSERT(err == XREF_OK);
    ASSERT(xrefs.count == 0);
    ASSERT(xref_is_target(&xrefs, 4) == 0);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(jmp_creates_jump_xref)
{
    /* JMP 0x0020 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x30, 0x00, 0x00, 0x00, 0x20
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 1);
    ASSERT(xrefs.entries[0].source == 4);
    ASSERT(xrefs.entries[0].target == 0x20);
    ASSERT(xrefs.entries[0].type == XREF_JUMP);
    ASSERT(xref_is_target(&xrefs, 0x20) == 1);
    ASSERT(xref_is_target(&xrefs, 0x21) == 0);
    ASSERT(xref_is_call_target(&xrefs, 0x20) == 0);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(call_creates_call_xref)
{
    /* CALL 0x0040 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x33, 0x00, 0x00, 0x00, 0x40
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 1);
    ASSERT(xrefs.entries[0].type == XREF_CALL);
    ASSERT(xrefs.entries[0].target == 0x40);
    ASSERT(xref_is_call_target(&xrefs, 0x40) == 1);
    ASSERT(xref_is_target(&xrefs, 0x40) == 1);
    ASSERT(xref_target_type(&xrefs, 0x40) == XREF_CALL);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(jz_jnz_both_recorded)
{
    /* JZ 0x10, JNZ 0x20 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x31, 0x00, 0x00, 0x00, 0x10,
        0x32, 0x00, 0x00, 0x00, 0x20
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 2);
    ASSERT(xref_is_target(&xrefs, 0x10) == 1);
    ASSERT(xref_is_target(&xrefs, 0x20) == 1);
    ASSERT(xrefs.entries[0].type == XREF_JUMP);
    ASSERT(xrefs.entries[1].type == XREF_JUMP);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(call_target_overrides_jump_type)
{
    /* CALL 0x10, JMP 0x10 — both point to same target; call takes priority */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x33, 0x00, 0x00, 0x00, 0x10,
        0x30, 0x00, 0x00, 0x00, 0x10
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 2);
    ASSERT(xref_is_target(&xrefs, 0x10) == 1);
    ASSERT(xref_is_call_target(&xrefs, 0x10) == 1);
    ASSERT(xref_target_type(&xrefs, 0x10) == XREF_CALL);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(non_branch_opcodes_ignored)
{
    /* PUSH, ADD, STORE, SYSCALL — no xrefs */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x02, 0x00, 0x00, 0x00, 0x01,
        0x10,
        0x41, 0x00,
        0x50, 0x0E
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 0);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

TEST(multiple_calls_to_same_target)
{
    /* CALL 0x40, CALL 0x40 */
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x33, 0x00, 0x00, 0x00, 0x40,
        0x33, 0x00, 0x00, 0x00, 0x40
    };
    nvm_insn_list_t insns = {0};
    decode_from_bytes(bc, sizeof(bc), &insns);

    xref_table_t xrefs = {0};
    xref_build(&insns, &xrefs);

    ASSERT(xrefs.count == 2);
    ASSERT(xref_is_call_target(&xrefs, 0x40) == 1);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
}

int main(void)
{
    printf("xref\n");
    RUN(empty_no_xrefs);
    RUN(jmp_creates_jump_xref);
    RUN(call_creates_call_xref);
    RUN(jz_jnz_both_recorded);
    RUN(call_target_overrides_jump_type);
    RUN(non_branch_opcodes_ignored);
    RUN(multiple_calls_to_same_target);

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
