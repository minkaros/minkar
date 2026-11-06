/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
/* The constraint solver (kiwi, a Cassowary implementation), behind a C
 * interface: variables are numbers, a constraint is
 *     sum(coefficients[i] * variables[i]) + constant  (op)  0
 * at a strength. */

typedef struct NSLayoutSolver NSLayoutSolver;

enum {
   NSLayoutSolverLessOrEqual = -1,
   NSLayoutSolverEqual = 0,
   NSLayoutSolverGreaterOrEqual = 1,
};

// What adding a constraint did.
enum {
   NSLayoutSolverAdded = 0,
   NSLayoutSolverUnsatisfiable = 1,  // required, and impossible with the others
   NSLayoutSolverNotAdded = 2,
};

#ifdef __cplusplus
extern "C" {
#endif

NSLayoutSolver *NSLayoutSolverCreate(void);
void NSLayoutSolverFree(NSLayoutSolver *solver);
int NSLayoutSolverNewVariable(NSLayoutSolver *solver);
int NSLayoutSolverAdd(NSLayoutSolver *solver, int count, const int *variables, const double *coefficients,
                      double constant, int op, double strength);
void NSLayoutSolverUpdate(NSLayoutSolver *solver);
double NSLayoutSolverValue(NSLayoutSolver *solver, int variable);

// A priority (1 to 1000) as a strength; 1000 and more is required.
double NSLayoutSolverStrength(double priority);
double NSLayoutSolverRequired(void);

#ifdef __cplusplus
}
#endif
