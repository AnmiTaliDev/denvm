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

#include "denvm/decode.h"
#include "denvm/loader.h"
#include "denvm/nvm.h"
#include "denvm/output.h"
#include "denvm/xref.h"

#define DENVM_VERSION "1.0.0"

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options] <file.nvm>\n"
        "\n"
        "Options:\n"
        "  -h, --help         Show this help message\n"
        "  -V, --version      Show version information\n"
        "  -x, --hex          Show hex bytes alongside disassembly\n"
        "  -d, --dump         Show hex dump of the entire file\n"
        "  -n, --no-color     Disable ANSI color output\n"
        "  --no-comments      Disable inline comments\n"
        "  --no-offsets       Hide byte offsets\n"
        "  -o <file>          Write output to file (default: stdout)\n"
        "\n"
        "denvm %s - NVM Bytecode Decompiler\n"
        "Copyright 2026 AnmiTaliDev <anmitalidev@nuros.org>\n"
        "Licensed under Apache License 2.0\n",
        argv0, DENVM_VERSION);
}

static void print_version(void)
{
    printf("denvm %s\n", DENVM_VERSION);
    printf("NVM Bytecode Decompiler\n");
    printf("Copyright 2026 AnmiTaliDev <anmitalidev@nuros.org>\n");
    printf("Licensed under Apache License 2.0\n");
}

static int is_tty(FILE *fp)
{
    return isatty(fileno(fp));
}

int main(int argc, char *argv[])
{
    const char *input_path  = NULL;
    const char *output_path = NULL;
    int show_hex      = 0;
    int show_dump     = 0;
    int no_color      = 0;
    int no_comments   = 0;
    int no_offsets    = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }

        if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            print_version();
            return 0;
        }

        if (strcmp(arg, "-x") == 0 || strcmp(arg, "--hex") == 0) {
            show_hex = 1;
            continue;
        }

        if (strcmp(arg, "-d") == 0 || strcmp(arg, "--dump") == 0) {
            show_dump = 1;
            continue;
        }

        if (strcmp(arg, "-n") == 0 || strcmp(arg, "--no-color") == 0) {
            no_color = 1;
            continue;
        }

        if (strcmp(arg, "--no-comments") == 0) {
            no_comments = 1;
            continue;
        }

        if (strcmp(arg, "--no-offsets") == 0) {
            no_offsets = 1;
            continue;
        }

        if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: -o requires a filename argument\n");
                return 1;
            }
            output_path = argv[++i];
            continue;
        }

        if (arg[0] == '-') {
            fprintf(stderr, "error: unknown option: %s\n", arg);
            fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
            return 1;
        }

        if (input_path) {
            fprintf(stderr, "error: multiple input files specified\n");
            return 1;
        }
        input_path = arg;
    }

    if (!input_path) {
        fprintf(stderr, "error: no input file specified\n");
        fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
        return 1;
    }

    FILE *out_fp = stdout;
    if (output_path) {
        out_fp = fopen(output_path, "w");
        if (!out_fp) {
            fprintf(stderr, "error: cannot open output file: %s\n", output_path);
            return 1;
        }
    }

    output_options_t opts = {
        .show_hex      = show_hex,
        .show_offsets  = !no_offsets,
        .show_comments = !no_comments,
        .color         = !no_color && is_tty(out_fp),
    };

    nvm_binary_t bin = {0};
    loader_error_t lerr = nvm_binary_load(input_path, &bin);
    if (lerr != LOADER_OK) {
        fprintf(stderr, "error: %s: %s\n", input_path, loader_strerror(lerr));
        if (out_fp != stdout)
            fclose(out_fp);
        return 1;
    }

    if (show_dump) {
        output_header_info(out_fp, &bin, &opts);
        output_hex_dump(out_fp, &bin);
        nvm_binary_free(&bin);
        if (out_fp != stdout)
            fclose(out_fp);
        return 0;
    }

    nvm_insn_list_t insns = {0};
    decode_error_t derr = nvm_decode(bin.data, bin.size, &insns);
    if (derr != DECODE_OK && derr != DECODE_ERR_TRUNCATED) {
        fprintf(stderr, "error: decode failed: %s\n", decode_strerror(derr));
        nvm_binary_free(&bin);
        if (out_fp != stdout)
            fclose(out_fp);
        return 1;
    }

    if (derr == DECODE_ERR_TRUNCATED)
        fprintf(stderr, "warning: %s: truncated instruction near end of file\n", input_path);

    xref_table_t xrefs = {0};
    xref_error_t xerr = xref_build(&insns, &xrefs);
    if (xerr != XREF_OK) {
        fprintf(stderr, "error: xref analysis failed\n");
        nvm_insn_list_free(&insns);
        nvm_binary_free(&bin);
        if (out_fp != stdout)
            fclose(out_fp);
        return 1;
    }

    output_header_info(out_fp, &bin, &opts);
    output_disassembly(out_fp, &bin, &insns, &xrefs, &opts);

    xref_free(&xrefs);
    nvm_insn_list_free(&insns);
    nvm_binary_free(&bin);

    if (out_fp != stdout)
        fclose(out_fp);

    return 0;
}
