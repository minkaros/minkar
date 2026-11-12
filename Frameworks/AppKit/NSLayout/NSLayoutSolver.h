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
/* Adds a constraint (variable i times coefficient i, plus the constant,
   related to zero), remembered by what it says. A required one that
   cannot be met is added at fallbackStrength instead, when that is above
   zero; the result says it was unsatisfiable. */
int NSLayoutSolverAdd(NSLayoutSolver *solver, int count, const int *variables, const double *coefficients,
                      double constant, int op, double strength, double fallbackStrength);
/* A pass: what it keeps is marked (1 and its first result when the solver
   has it already), then the sweep takes out what was not. */
void NSLayoutSolverBeginPass(NSLayoutSolver *solver);
int NSLayoutSolverKeep(NSLayoutSolver *solver, int count, const int *variables, const double *coefficients,
                       double constant, int op, double strength, int *status);
void NSLayoutSolverSweep(NSLayoutSolver *solver);
int NSLayoutSolverVariableCount(NSLayoutSolver *solver);
void NSLayoutSolverUpdate(NSLayoutSolver *solver);
double NSLayoutSolverValue(NSLayoutSolver *solver, int variable);

// A priority (1 to 1000) as a strength; 1000 and more is required.
double NSLayoutSolverStrength(double priority);
double NSLayoutSolverRequired(void);

#ifdef __cplusplus
}
#endif
