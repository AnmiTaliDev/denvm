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

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "denvm/loader.h"
#include "denvm/nvm.h"

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

static const char *write_tmp(const uint8_t *data, size_t size)
{
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/denvm_test_%d.nvm", (int)getpid());
    FILE *fp = fopen(path, "wb");
    if (!fp) return NULL;
    fwrite(data, 1, size, fp);
    fclose(fp);
    return path;
}

TEST(valid_file_loads)
{
    static const uint8_t bc[] = {
        0x4E, 0x56, 0x4D, 0x30,
        0x00
    };
    const char *path = write_tmp(bc, sizeof(bc));
    ASSERT(path != NULL);

    nvm_binary_t bin = {0};
    loader_error_t err = nvm_binary_load(path, &bin);
    ASSERT(err == LOADER_OK);
    ASSERT(bin.data != NULL);
    ASSERT(bin.size == sizeof(bc));
    ASSERT(bin.data[0] == 0x4E);
    ASSERT(bin.data[3] == 0x30);

    nvm_binary_free(&bin);
    ASSERT(bin.data == NULL);
    ASSERT(bin.size == 0);
}

TEST(nonexistent_file)
{
    nvm_binary_t bin = {0};
    loader_error_t err = nvm_binary_load("/nonexistent/path/file.nvm", &bin);
    ASSERT(err == LOADER_ERR_OPEN);
    ASSERT(bin.data == NULL);
}

TEST(invalid_magic)
{
    static const uint8_t bc[] = { 0x7F, 0x45, 0x4C, 0x46, 0x00 };
    const char *path = write_tmp(bc, sizeof(bc));
    ASSERT(path != NULL);

    nvm_binary_t bin = {0};
    loader_error_t err = nvm_binary_load(path, &bin);
    ASSERT(err == LOADER_ERR_INVALID_MAGIC);
    ASSERT(bin.data == NULL);
}

TEST(truncated_magic)
{
    static const uint8_t bc[] = { 0x4E, 0x56 };
    const char *path = write_tmp(bc, sizeof(bc));
    ASSERT(path != NULL);

    nvm_binary_t bin = {0};
    loader_error_t err = nvm_binary_load(path, &bin);
    ASSERT(err == LOADER_ERR_TOO_SMALL);
    ASSERT(bin.data == NULL);
}

TEST(exact_header_size)
{
    static const uint8_t bc[] = { 0x4E, 0x56, 0x4D, 0x30 };
    const char *path = write_tmp(bc, sizeof(bc));
    ASSERT(path != NULL);

    nvm_binary_t bin = {0};
    loader_error_t err = nvm_binary_load(path, &bin);
    ASSERT(err == LOADER_OK);
    ASSERT(bin.size == 4);

    nvm_binary_free(&bin);
}

TEST(path_stored_in_struct)
{
    static const uint8_t bc[] = { 0x4E, 0x56, 0x4D, 0x30, 0x00 };
    const char *path = write_tmp(bc, sizeof(bc));
    ASSERT(path != NULL);

    nvm_binary_t bin = {0};
    nvm_binary_load(path, &bin);
    ASSERT(strcmp(bin.path, path) == 0);

    nvm_binary_free(&bin);
}

TEST(strerror_coverage)
{
    ASSERT(loader_strerror(LOADER_OK)                != NULL);
    ASSERT(loader_strerror(LOADER_ERR_OPEN)          != NULL);
    ASSERT(loader_strerror(LOADER_ERR_STAT)          != NULL);
    ASSERT(loader_strerror(LOADER_ERR_READ)          != NULL);
    ASSERT(loader_strerror(LOADER_ERR_ALLOC)         != NULL);
    ASSERT(loader_strerror(LOADER_ERR_INVALID_MAGIC) != NULL);
    ASSERT(loader_strerror(LOADER_ERR_TOO_SMALL)     != NULL);
}

int main(void)
{
    printf("loader\n");
    RUN(valid_file_loads);
    RUN(nonexistent_file);
    RUN(invalid_magic);
    RUN(truncated_magic);
    RUN(exact_header_size);
    RUN(path_stored_in_struct);
    RUN(strerror_coverage);

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
