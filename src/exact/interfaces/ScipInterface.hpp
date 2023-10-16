/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    PaPILO --- Parallel Presolve for Integer and Linear Optimization       */
/*                                                                           */
/* Copyright (C) 2020-2023 Konrad-Zuse-Zentrum                               */
/*                     fuer Informationstechnik Berlin                       */
/*                                                                           */
/* This program is free software: you can redistribute it and/or modify      */
/* it under the terms of the GNU Lesser General Public License as published  */
/* by the Free Software Foundation, either version 3 of the License, or      */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Lesser General Public License for more details.                       */
/*                                                                           */
/* You should have received a copy of the GNU Lesser General Public License  */
/* along with this program.  If not, see <https://www.gnu.org/licenses/>.    */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef EXACT_INTERFACES_SCIP_INTERFACE_HPP_
#define EXACT_INTERFACES_SCIP_INTERFACE_HPP_

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "exact/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "exact/data/Problem.hpp"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"

namespace exact {


   class ScipInterface {
   private:
      SCIP *scip;

   public:
      ScipInterface( ) : scip(nullptr) {
         if( SCIPcreate(&scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      SCIP*
      getSCIP()
      {
         return scip;
      }

      SCIP_RETCODE
      doSetUp(const Problem<exact::Rational> &problem) {
         auto return_code = SCIPcreate(&scip);
         SCIP_CALL_ABORT(SCIPincludeDefaultPlugins(scip));
         assert(return_code == SCIP_OKAY);
         int ncols = problem.getNCols( );
         int nrows = problem.getNRows( );
         const Vec<String> &varNames = problem.getVariableNames( );
         const Vec<String> &consNames = problem.getConstraintNames( );
         const VariableDomains<exact::Rational> &domains = problem.getVariableDomains( );
         const Objective<exact::Rational> &obj = problem.getObjective( );
         const auto &consMatrix = problem.getConstraintMatrix( );
         const auto &lhs_values = consMatrix.getLeftHandSides( );
         const auto &rhs_values = consMatrix.getRightHandSides( );
         const auto &rflags = problem.getRowFlags( );

         SCIP_CALL(SCIPcreateProbBasic(scip, problem.getName( ).c_str( )));

         Vec<SCIP_VAR *> variables;
         variables.resize(problem.getNCols( ));

         for( int i = 0; i < ncols; ++i )
         {
            SCIP_VAR *var;
            assert(!domains.flags[ i ].test(ColFlag::kInactive));

            SCIP_Real lb = domains.flags[ i ].test(ColFlag::kLbInf)
                           ? -SCIPinfinity(scip)
                           : SCIP_Real(domains.lower_bounds[ i ]);
            SCIP_Real ub = domains.flags[ i ].test(ColFlag::kUbInf)
                           ? SCIPinfinity(scip)
                           : SCIP_Real(domains.upper_bounds[ i ]);
            SCIP_VARTYPE type;
            if( domains.flags[ i ].test(ColFlag::kIntegral))
            {
               if( lb == exact::Rational { 0 } && ub == exact::Rational { 1 } )
                  type = SCIP_VARTYPE_BINARY;
               else
                  type = SCIP_VARTYPE_INTEGER;
            }
            else if( domains.flags[ i ].test(ColFlag::kImplInt))
               type = SCIP_VARTYPE_IMPLINT;
            else
               type = SCIP_VARTYPE_CONTINUOUS;

            SCIP_CALL(SCIPcreateVarBasic(
                  scip, &var, varNames[ i ].c_str( ), lb, ub,
                  SCIP_Real(obj.coefficients[ i ]), type));
            SCIP_CALL(SCIPaddVar(scip, var));
            variables[ i ] = var;

            SCIP_CALL(SCIPreleaseVar(scip, &var));
         }

         Vec<SCIP_VAR *> consvars;
         Vec<SCIP_Real> consvals;
         consvars.resize(problem.getNCols( ));
         consvals.resize(problem.getNCols( ));

         auto vars = SCIPgetVars(scip);

         const char *string = SCIPvarGetName(vars[ 0 ]);

         for( int i = 0; i < nrows; ++i )
         {
            SCIP_CONS *cons;

            auto rowvec = consMatrix.getRowCoefficients(i);
            const exact::Rational *vals = rowvec.getValues( );
            const int *inds = rowvec.getIndices( );
            SCIP_Real lhs = rflags[ i ].test(RowFlag::kLhsInf)
                            ? -SCIPinfinity(scip)
                            : SCIP_Real(lhs_values[ i ]);
            SCIP_Real rhs = rflags[ i ].test(RowFlag::kRhsInf)
                            ? SCIPinfinity(scip)
                            : SCIP_Real(rhs_values[ i ]);

            for( int k = 0; k < rowvec.getLength( ); ++k )
            {
               consvars[ k ] = variables[ inds[ k ]];
               consvals[ k ] = SCIP_Real(vals[ k ]);
            }

            SCIP_CALL(SCIPcreateConsBasicLinear(
                  scip, &cons, consNames[ i ].c_str( ), rowvec.getLength( ),
                  consvars.data( ), consvals.data( ), lhs, rhs));
            SCIP_CALL(SCIPaddCons(scip, cons));
            SCIP_CALL(SCIPreleaseCons(scip, &cons));

         }

         if( obj.offset != exact::Rational { 0 } )
            SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));

         return SCIP_OKAY;
      }


      void
      solve( ) {
         SCIP_RETCODE returncode = SCIPsolve(scip);
         assert(returncode == SCIP_OKAY);
      };

      void
      enable_branch_and_bound_only( ) {
         SCIP_RETCODE status = SCIPsetBoolParam(scip, "conflict/enable", false);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/maxrestarts", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/maxroundsroot", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/nonlinear/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/nonlinear/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/linear/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/linear/maxprerounds", 0);
         assert(status == SCIP_OKAY);
//         status = SCIPsetIntParam(scip, "constraints/linear-exact/sepafreq", -1);
//         assert(status == SCIP_OKAY);
//         status = SCIPsetIntParam(scip, "constraints/linear-exact/maxprerounds", 0);
//         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/and/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/and/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/bounddisjunction/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/cardinality/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/cardinality/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/conjunction/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/cumulative/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/cumulative/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/disjunction/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/indicator/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/indicator/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/knapsack/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/knapsack/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/linking/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/linking/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/logicor/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/logicor/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/or/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/or/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/orbisack/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/orbisack/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/orbitope/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/pseudoboolean/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/setppc/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/setppc/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/SOS1/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/SOS1/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/SOS2/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/SOS2/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/superindicator/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/symresack/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/symresack/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/varbound/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/varbound/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/xor/sepafreq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/xor/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "constraints/components/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/domcol/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/dualcomp/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/gateextraction/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/implics/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/inttobinary/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/milp/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/trivial/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/sparsify/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "presolving/dualsparsify/maxrounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/adaptivediving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/clique/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/completesol/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/conflictdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/crossover/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/distributiondiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/farkasdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/feaspump/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/fracdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/gins/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/guideddiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/indicator/freq", -1);
         assert(status == SCIP_OKAY);
//         status = SCIPsetIntParam(scip, "heuristics/indicatordiving/freq", -1);
//         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/intshifting/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/linesearchdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/locks/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/lpface/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/alns/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/nlpdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/multistart/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/mpec/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/objpscostdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/ofins/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/oneopt/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/padm/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/pscostdiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/randrounding/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/rens/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/reoptsols/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/rins/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/rootsoldiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/rounding/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/shiftandpropagate/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/shifting/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/simplerounding/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/subnlp/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/trivial/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/trivialnegation/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/trysol/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/undercover/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/vbounds/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/veclendiving/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "heuristics/zirounding/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/dualfix/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/genvbounds/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/obbt/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/nlobbt/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/probing/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/pseudoobj/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/redcost/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/rootredcost/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/symmetry/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "propagating/vbounds/maxprerounds", 0);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/clique/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/flowcover/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/cmir/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/knapsackcover/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/aggregation/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/disjunctive/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/gomory/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/strongcg/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/gomorymi/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/impliedbounds/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/mcf/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/minor/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/mixing/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/rapidlearning/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/rlt/freq", -1);
         assert(status == SCIP_OKAY);
         status = SCIPsetIntParam(scip, "separating/zerohalf/freq", -1);
         assert(status == SCIP_OKAY);

      }

      ~ScipInterface( ) {
         if( scip != nullptr )
         {
            SCIP_RETCODE retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }
   };

} // namespace exact

#endif
