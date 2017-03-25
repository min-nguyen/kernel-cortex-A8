#ifndef __HILEVEL_H
#define __HILEVEL_H

// Include functionality relating to newlib (the standard C library).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

// Include functionality relating to the platform.

#include   "GIC.h"
#include   "MMU.h"
#include "PL011.h"
#include "SP804.h"

// Include functionality relating to the   kernel.

#include "lolevel.h"
#include     "int.h"

/* The kernel source code is made simpler via three type definitions:
 *
 * - a type that captures a Process IDentifier (PID), which is really
 *   just an integer,
 * - a type that captures each component of an execution context (i.e.,
 *   processor state) in a compatible order wrt. the low-level handler
 *   preservation and restoration prologue and epilogue, and
 * - a type that captures a process PCB.
 */



void hilevel_write(char* c, int n);

#define PCB_SIZE 50
#define BUFFER_SIZE 50

typedef int pid_t;
typedef uint32_t pte_t;
typedef struct {
  uint32_t cpsr, pc, gpr[ 13 ], sp, lr;
} ctx_t;

typedef struct {
  uint32_t value;
  int fromPID, toPID;
} buffer_t;

typedef struct {
  pid_t pid;
  ctx_t ctx;
  int timer;
  int priority;
  int page;
  bool waiting;
  bool active;
  pte_t T[4096] __attribute__ ((aligned (1 << 14)));
} pcb_t;

#endif
