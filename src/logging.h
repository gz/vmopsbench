/*
 * Virtual Memory Operations Benchmark
 *
 * Copyright 2020 Reto Achermann
 * SPDX-License-Identifier: GPL-3.0
 */

#ifndef __VMOPS_LOGGING_H_
#define __VMOPS_LOGGING_H_ 1

#include <stdio.h>


// General Formatting
#define FORMAT_RESET "0"
#define FORMAT_BRIGHT "1"
#define FORMAT_DIM "2"
#define FORMAT_ITALIC "3"
#define FORMAT_UNDERSCORE "4"
#define FORMAT_REVERSE "5"
#define FORMAT_HIDDEN "6"

// Foreground Colors
#define COLOR_BLACK "30"
#define COLOR_RED "31"
#define COLOR_GREEN "32"
#define COLOR_YELLOW "33"
#define COLOR_BLUE "34"
#define COLOR_MAGENTA "35"
#define COLOR_CYAN "36"
#define COLOR_WHITE "37"


#define SHELL_COLOR_ESCAPE_SEQ(X) "\x1b[" X "m"

#define SHELL_FORMAT_RESET ANSI_COLOR_ESCAPE_SEQ(GEN_FORMAT_RESET)

#define BUILD_FORMAT(_STYLE, _COLOR) "\x1b[" _STYLE ";" _COLOR "m"

#define COLOR_RESET "\x1b[" FORMAT_RESET "m"
#define COLOR_INFO BUILD_FORMAT(FORMAT_BRIGHT, COLOR_GREEN)
#define COLOR_WARN BUILD_FORMAT(FORMAT_BRIGHT, COLOR_YELLOW)
#define COLOR_ERR BUILD_FORMAT(FORMAT_BRIGHT, COLOR_RED)
#define COLOR_RESULT                                                                              \
    BUILD_FORMAT(FORMAT_BRIGHT, COLOR_CYAN) BUILD_FORMAT(FORMAT_UNDERSCORE, COLOR_CYAN)

#define VMOPS_PRINT_PREFIX "+VMOPS "

#define LOG_INFO(...)                                                                             \
    fprintf(stdout, VMOPS_PRINT_PREFIX COLOR_INFO "INFO " COLOR_RESET __VA_ARGS__)

#define LOG_WARN(...)                                                                             \
    fprintf(stdout, VMOPS_PRINT_PREFIX COLOR_WARN "WARN " COLOR_RESET __VA_ARGS__)

#define LOG_ERR(...) fprintf(stdout, VMOPS_PRINT_PREFIX COLOR_ERR "ERROR " COLOR_RESET __VA_ARGS__)


#define LOG_PRINT(...) fprintf(stdout, VMOPS_PRINT_PREFIX __VA_ARGS__)
#define LOG_PRINT_CONT(...) fprintf(stdout, __VA_ARGS__)
#define LOG_PRINT_END(...) fprintf(stdout, __VA_ARGS__)


#define RESULT_FMT_STRING "benchmark=%s, memsize=%zu, time=%d, ncores=%d, ops=%zu"

#define LOG_RESULT(_b, _m, _t, _n, _o)                                                             \
    fprintf(stdout,                                                                                \
            VMOPS_PRINT_PREFIX COLOR_RESULT "RESULT [[ " RESULT_FMT_STRING " ]]" COLOR_RESET "\n", \
            _b, _m, _t, _n, _o)

#define LOG_CSV_HEADER()                                                                          \
    fprintf(stdout, "===================== BEGIN CSV =====================\n");                   \
    fprintf(stdout, "thread_id,benchmark,core,ncores,memsize,duration,operations\n");

#define LOG_CSV_FOOTER() \
    fprintf(stdout, "====================== END CSV ======================\n");

#define LOG_CSV(_b, _t, _c, _n, _m, _d, _tpt)                                                     \
    fprintf(stdout, "%d,%s,%d,%d,%zu,%.3f,%zu\n", _t, _b, _c, _n, _m, _d, _tpt);

#endif /* __VMOPS_LOGGING_H_ */