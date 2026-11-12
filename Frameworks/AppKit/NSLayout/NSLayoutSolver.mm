/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#include "NSLayoutSolver.h"
#include <kiwi/kiwi.h>
#include <string>
#include <unordered_map>
#include <vector>

/* What a pass gave the solver, by what it says (variables, coefficients,
 * constant, relation and strength): kept from one pass to the next, so a
 * pass takes out what is gone and adds what is new instead of building
 * the whole solver again (seconds for a window of a few hundred views). A
 * required constraint that could not be met is kept as the weaker one
 * that stood for it. */
struct NSLayoutSolverRecord {
   kiwi::Constraint constraint;
   bool             added;
   bool             seen;
   int              status;
};

struct NSLayoutSolver {
   kiwi::Solver solver;
   std::vector<kiwi::Variable> variables;
   std::unordered_map<std::string,NSLayoutSolverRecord> records;
};

static std::string NSLayoutSolverKey(int count,const int *variables,const double *coefficients,double constant,int op,double strength) {
   std::string key;

   key.append((const char *)&count,sizeof(count));
   key.append((const char *)variables,sizeof(int)*count);
   key.append((const char *)coefficients,sizeof(double)*count);
   key.append((const char *)&constant,sizeof(constant));
   key.append((const char *)&op,sizeof(op));
   key.append((const char *)&strength,sizeof(strength));
   return key;
}

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

int NSLayoutSolverVariableCount(NSLayoutSolver *solver) {
   return (int)solver->variables.size();
}

void NSLayoutSolverBeginPass(NSLayoutSolver *solver) {
   for(auto &record : solver->records)
    record.second.seen=false;
}

int NSLayoutSolverKeep(NSLayoutSolver *solver,int count,const int *variables,const double *coefficients,double constant,int op,double strength,int *status) {
   auto found=solver->records.find(NSLayoutSolverKey(count,variables,coefficients,constant,op,strength));

   if(found==solver->records.end())
    return 0;
   found->second.seen=true;
   if(status!=NULL)
    *status=found->second.status;
   return 1;
}

void NSLayoutSolverSweep(NSLayoutSolver *solver) {
   for(auto record=solver->records.begin();record!=solver->records.end();){
    if(record->second.seen){
     ++record;
     continue;
    }
    if(record->second.added){
     try {
      solver->solver.removeConstraint(record->second.constraint);
     }
     catch(...){
     }
    }
    record=solver->records.erase(record);
   }
}

static int NSLayoutSolverTry(NSLayoutSolver *solver,const kiwi::Constraint &constraint) {
   try {
    solver->solver.addConstraint(constraint);
    return NSLayoutSolverAdded;
   }
   catch(kiwi::UnsatisfiableConstraint &){
    return NSLayoutSolverUnsatisfiable;
   }
   catch(...){
    return NSLayoutSolverNotAdded;
   }
}

int NSLayoutSolverAdd(NSLayoutSolver *solver,int count,const int *variables,const double *coefficients,double constant,int op,double strength,double fallbackStrength) {
   std::vector<kiwi::Term> terms;

   for(int i=0;i<count;i++)
    terms.push_back(kiwi::Term(solver->variables[variables[i]],coefficients[i]));

   kiwi::Expression         expression(terms,constant);
   kiwi::RelationalOperator relation=(op<0)?kiwi::OP_LE:(op>0)?kiwi::OP_GE:kiwi::OP_EQ;
   kiwi::Constraint         constraint(expression,relation,strength);
   int                      status=NSLayoutSolverTry(solver,constraint);
   bool                     added=(status==NSLayoutSolverAdded);

   if(status==NSLayoutSolverUnsatisfiable && fallbackStrength>0){
    constraint=kiwi::Constraint(expression,relation,fallbackStrength);
    added=(NSLayoutSolverTry(solver,constraint)==NSLayoutSolverAdded);
   }
   solver->records[NSLayoutSolverKey(count,variables,coefficients,constant,op,strength)]=
     NSLayoutSolverRecord{constraint,added,true,status};
   return status;
}

void NSLayoutSolverUpdate(NSLayoutSolver *solver) {
   solver->solver.updateVariables();
}

double NSLayoutSolverValue(NSLayoutSolver *solver,int variable) {
   return solver->variables[variable].value();
}
