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
#include <sys/stat.h>

#include "denvm/loader.h"
#include "denvm/nvm.h"

loader_error_t nvm_binary_load(const char *path, nvm_binary_t *out)
{
    if (!path || !out)
        return LOADER_ERR_OPEN;

    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return LOADER_ERR_OPEN;

    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        return LOADER_ERR_STAT;
    }

    size_t size = (size_t)st.st_size;

    if (size < NVM_HEADER_SIZE) {
        fclose(fp);
        return LOADER_ERR_TOO_SMALL;
    }

    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return LOADER_ERR_ALLOC;
    }

    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return LOADER_ERR_READ;
    }

    fclose(fp);

    if (data[0] != NVM_MAGIC_0 || data[1] != NVM_MAGIC_1 ||
        data[2] != NVM_MAGIC_2 || data[3] != NVM_MAGIC_3) {
        free(data);
        return LOADER_ERR_INVALID_MAGIC;
    }

    out->data = data;
    out->size = size;

    return LOADER_OK;
}

void nvm_binary_free(nvm_binary_t *bin)
{
    if (!bin)
        return;

    free(bin->data);
    bin->data = NULL;
    bin->size = 0;
}

const char *loader_strerror(loader_error_t err)
{
    switch (err) {
    case LOADER_OK:                 return "success";
    case LOADER_ERR_OPEN:           return "cannot open file";
    case LOADER_ERR_STAT:           return "cannot stat file";
    case LOADER_ERR_READ:           return "read error";
    case LOADER_ERR_ALLOC:          return "memory allocation failed";
    case LOADER_ERR_INVALID_MAGIC:  return "invalid NVM0 magic signature";
    case LOADER_ERR_TOO_SMALL:      return "file too small (minimum 4 bytes)";
    default:                        return "unknown error";
    }
}
