/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#include "NSLayoutSolver.h"
#include <kiwi/kiwi.h>
#include <vector>

struct NSLayoutSolver {
   kiwi::Solver solver;
   std::vector<kiwi::Variable> variables;
};

NSLayoutSolver *NSLayoutSolverCreate(void) {
   return new NSLayoutSolver();
}

void NSLayoutSolverFree(NSLayoutSolver *solver) {
   delete solver;
}

int NSLayoutSolverNewVariable(NSLayoutSolver *solver) {
   solver->variables.push_back(kiwi::Variable());
   return (int)solver->variables.size()-1;
}

double NSLayoutSolverRequired(void) {
   return kiwi::strength::required;
}

double NSLayoutSolverStrength(double priority) {
   if(priority>=1000)
    return kiwi::strength::required;
   if(priority<=0)
    return 0;
   return kiwi::strength::create(0,priority,0);
}

int NSLayoutSolverAdd(NSLayoutSolver *solver,int count,const int *variables,const double *coefficients,double constant,int op,double strength) {
   std::vector<kiwi::Term> terms;

   for(int i=0;i<count;i++)
    terms.push_back(kiwi::Term(solver->variables[variables[i]],coefficients[i]));

   kiwi::Expression expression(terms,constant);
   kiwi::RelationalOperator relation=(op<0)?kiwi::OP_LE:(op>0)?kiwi::OP_GE:kiwi::OP_EQ;

   try {
    solver->solver.addConstraint(kiwi::Constraint(expression,relation,strength));
    return NSLayoutSolverAdded;
   }
   catch(kiwi::UnsatisfiableConstraint &){
    return NSLayoutSolverUnsatisfiable;
   }
   catch(...){
    return NSLayoutSolverNotAdded;
   }
}

void NSLayoutSolverUpdate(NSLayoutSolver *solver) {
   solver->solver.updateVariables();
}

double NSLayoutSolverValue(NSLayoutSolver *solver,int variable) {
   return solver->variables[variable].value();
}
