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

#ifndef EXACT_INTERFACES_EVENT_CATCHER_HPP_
#define EXACT_INTERFACES_EVENT_CATCHER_HPP_

#define UNUSED(expr) do { (void)(expr); } while (0)

#include <cassert>
#include <stdexcept>

#include "scip/scip.h"
#include "scip/sol.h"
//#include "scip/lpi.h"
#include "exact/data/Solution.hpp"
#include "exact/misc/Vec.hpp"
#include "exact/interfaces/ViprInterface.hpp"
#include "NodeInfeasible.hpp"

namespace exact {

   static constexpr const char *const EVENT_NODE_ACTION = "node_action";

   static constexpr const char *const DESC_NODE_ACTION = "event handler for best solutions found";

   /** LP reading data */
   struct Exact_eventdata {
      Vec<std::pair<Vec<Rational>, Rational>> &solutions;
      Vec<NodeInfeasible> &node_infeasible;
      const Problem<Rational> &problem;
      Rational &best_bound;
      Rational &dual_bound;
   };

   class EventCatcher {
   private:

      Vec<std::pair<Vec<Rational>, Rational>> &solutions;
      Vec<NodeInfeasible> &node_infeasible;
      const Problem<Rational> &problem;
      Rational &best_bound;
      Rational &dual_bound;

   public:
      EventCatcher(Vec<std::pair<Vec<Rational>, Rational>> &_solutions, Vec<NodeInfeasible> &_node_infeasible,
                   const Problem<Rational> &_problem, Rational &_best_bound, Rational &_dual_bound) :
            solutions(_solutions), node_infeasible(_node_infeasible), problem(_problem), best_bound(_best_bound), dual_bound(_dual_bound) {
      }

      SCIP_RETCODE
      register_scip(SCIP *scip) {
         auto *eventdata = new Exact_eventdata { solutions, node_infeasible, problem, best_bound, dual_bound };
         auto *eventhdlrdata = ( SCIP_EVENTHDLRDATA * ) eventdata;

         SCIP_EVENTHDLR *eventhdlr;

         eventhdlr = nullptr;
         SCIP_CALL(SCIPincludeEventhdlrBasic(scip, &eventhdlr, EVENT_NODE_ACTION, DESC_NODE_ACTION,
                                             eventNodeAction, eventhdlrdata));
         assert(eventhdlr != nullptr);

//         SCIP_CALL(SCIPsetEventhdlrCopy(scip, eventhdlr, eventCopyBestsol));
         SCIP_CALL(SCIPsetEventhdlrInit(scip, eventhdlr, eventInitBestsol));
         SCIP_CALL(SCIPsetEventhdlrExit(scip, eventhdlr, eventExitBestsol));
         return SCIP_OKAY;

      }

      static


      SCIP_DECL_EVENTINIT(eventInitBestsol) {  /*lint --e{715}*/
         assert(scip != nullptr);
         assert(eventhdlr != nullptr);
         assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENT_NODE_ACTION) == 0);

         SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, nullptr, nullptr));
         SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_POORSOLFOUND, eventhdlr, nullptr, nullptr));
         SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_NODEFEASIBLE, eventhdlr, nullptr, nullptr));
         SCIP_CALL(SCIPcatchEvent(scip, SCIP_EVENTTYPE_NODEINFEASIBLE, eventhdlr, nullptr, nullptr));

         return SCIP_OKAY;
      }

      static
      SCIP_DECL_EVENTEXIT(eventExitBestsol) {  /*lint --e{715}*/
         assert(scip != nullptr);
         assert(eventhdlr != nullptr);
         assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENT_NODE_ACTION) == 0);

         /* notify SCIP that your event handler wants to drop the event type best solution found */
         SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, nullptr, -1));
         SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_POORSOLFOUND, eventhdlr, nullptr, -1));
         SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_NODEFEASIBLE, eventhdlr, nullptr, -1));
         SCIP_CALL(SCIPdropEvent(scip, SCIP_EVENTTYPE_NODEINFEASIBLE, eventhdlr, nullptr, -1));

         return SCIP_OKAY;
      }

      static bool verify_solution(const Problem<Rational> &problem, Vec<Rational> solution) {

         const Vec<Rational> &ub = problem.getUpperBounds( );
         const Vec<Rational> &lb = problem.getLowerBounds( );
         const Vec<Rational> &rhs = problem.getConstraintMatrix( ).getRightHandSides( );
         const Vec<Rational> &lhs = problem.getConstraintMatrix( ).getLeftHandSides( );

         for( int col = 0; col < problem.getNCols( ); col++ )
         {
            if(( !problem.getColFlags( )[ col ].test(ColFlag::kLbInf)) &&
               solution[ col ] < lb[ col ] )
               return false;
            if(( !problem.getColFlags( )[ col ].test(ColFlag::kUbInf)) &&
               solution[ col ] > ub[ col ] )
               return false;
         }
         for( int row = 0; row < problem.getNRows( ); row++ )
         {
            Rational rowValue = 0;
            auto entries = problem.getConstraintMatrix( ).getRowCoefficients(row);
            for( int entry = 0; entry < entries.getLength( ); entry++ )
            {
               int col = entries.getIndices( )[ entry ];
               rowValue += entries.getValues( )[ entry ] * solution[ col ];

            }
            bool lhs_inf = problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf);
            if(( !lhs_inf ) && rowValue < lhs[ row ] )
               return false;
            bool rhs_inf = problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf);
            if(( !rhs_inf ) && rowValue > rhs[ row ] )
               return false;
         }
         return true;
      }

/** execution method of event handler */
      static
      SCIP_DECL_EVENTEXEC(eventNodeAction) {
         assert(eventhdlr != nullptr);
         assert(strcmp(SCIPeventhdlrGetName(eventhdlr), EVENT_NODE_ACTION) == 0);
         assert(event != nullptr);
         assert(scip != nullptr);
         auto *data = ( Exact_eventdata * ) SCIPeventhdlrGetData(eventhdlr);
         assert(data != nullptr);

//         assert(SCIPnodeGetType(SCIPeventGetNode(event)) == SCIP_NODETYPE_LEAF);
         switch( SCIPeventGetType(event))
         {
            case SCIP_EVENTTYPE_NODEINFEASIBLE:
            {
               if( SCIPgetLPSolstat(scip) == SCIP_LPSOLSTAT_INFEASIBLE )
               {

                  SCIPinfoMessage(scip, nullptr, "node infeasible: ");
                  print_path(scip, SCIPeventGetNode(event));
                  SCIPinfoMessage(scip, nullptr, "\n");

                  SCIP_VAR **vars;
                  int nvars;
                  SCIP_CALL(SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr));
//               double dual_obj = compute_dual_objective(scip);
//                  SCIP_Cons** cons = SCIPgetOrigConss(scip);
//                  int ncons = SCIPgetNLPRows(scip);

                  double primal_farkas[nvars];
                  double lhs = 0;
                  for( int i = 0; i < nvars; i++ )
                  {
                     primal_farkas[ i ] = SCIPgetVarFarkasCoef(scip, vars[ i ]);
                     //TODO global and local bounds should be equal except for the fixings
//                     assert(SCIPvarGetLbLocal(vars[ i ]) == SCIPvarGetLbGlobal(vars[ i ]));
//                     assert(SCIPvarGetUbLocal(vars[ i ]) == SCIPvarGetUbGlobal(vars[ i ]));
                     if( primal_farkas[ i ] != 0 )
                        SCIPinfoMessage(scip, nullptr, "%s: %g %g %g %g %g %d\n", SCIPvarGetName(vars[ i ]),
                                        primal_farkas[ i ], SCIPvarGetLbLocal(vars[ i ]), SCIPvarGetUbLocal(vars[ i ]),
                                        SCIPvarGetUbLocal(vars[ i ]), SCIPvarGetUbGlobal(vars[ i ]), SCIPvarGetIndex(vars[i]));
                     if( primal_farkas[ i ] == 0 )
                        continue;
                     else if( primal_farkas[ i ] < 0 )
                        lhs += SCIPvarGetUbLocal(vars[ i ]) * primal_farkas[ i ];
                     else
                     {
                        assert(primal_farkas[ i ] > 0);
                        lhs += SCIPvarGetLbLocal(vars[ i ]) * primal_farkas[ i ];
                     }
                  }

                  auto rows = SCIPgetLPRows(scip);
                  int nrows = SCIPgetNLPRows(scip);

                  double dual_farkas[nrows];
                  for( int i = 0; i < nrows; i++ )
                  {
                     dual_farkas[ i ] = SCIProwGetDualfarkas(rows[ i ]);
                     if( dual_farkas[ i ] != 0 )
                        SCIPinfoMessage(scip, nullptr, "%s: %g\n", SCIProwGetName(rows[ i ]), dual_farkas[ i ]);
                     if( dual_farkas[ i ] == 0 )
                        continue;
                     else if( dual_farkas[ i ] > 0 )
                        lhs += SCIProwGetLhs(rows[ i ]) * dual_farkas[ i ];
                     else
                     {
                        assert(dual_farkas[ i ] < 0);
                        int index = SCIProwGetIndex(rows[ i ]);
                        lhs += SCIProwGetRhs(rows[ i ]) * dual_farkas[ i ];
                     }
                  }

                  double rhs = 0;
//                  SCIPinfoMessage(scip, nullptr, "done %g \n", lhs);
               }
               else
               {
                  assert(SCIPgetLPSolstat(scip) == SCIP_LPSOLSTAT_OBJLIMIT);
                  SCIPinfoMessage(scip, nullptr, "obj limit: ");
                  print_path(scip, SCIPeventGetNode(event));

                  SCIPinfoMessage(scip, nullptr, "\n");
               }
               break;
            }
            case SCIP_EVENTTYPE_NODEFEASIBLE:
            {
               double dual = compute_dual_objective(scip);

               SCIP_NODE *pNode = SCIPeventGetNode(event);
               SCIPinfoMessage(scip, nullptr, "node feasible with dual (%g) :", dual);
               print_path(scip, pNode);

               SCIPinfoMessage(scip, nullptr, "\n");


               break;
            }
            case SCIP_EVENTTYPE_POORSOLFOUND:
            {
               SCIPinfoMessage(scip, nullptr, "node solved\n");
               SCIP_SOL *sol = SCIPeventGetSol(event);
               SCIP_Real solvalue = SCIPgetSolOrigObj(scip, sol);


               SCIPinfoMessage(scip, nullptr, "found new poor solution with solution value <%g> in SCIP <%s>\n",
                               solvalue, SCIPgetProbName(scip));
               break;
            }
            case SCIP_EVENTTYPE_BESTSOLFOUND:
            {
               SCIP_SOL *bestsol = SCIPgetBestSol(scip);
               assert(bestsol != nullptr);
               SCIP_Real solvalue = SCIPgetSolOrigObj(scip, bestsol);
               SCIP_VAR **vars;
               int nvars;
               SCIP_CALL(SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr));
               Vec<Rational> sol;
               sol.resize(nvars);
               for( int var = 0; var < nvars; var++ )
               {
                  int index = SCIPvarGetIndex(vars[ var ]);
                  sol[ index ] = Rational { SCIPgetSolVal(scip, bestsol, vars[ var ]) };
                  if( sol[ var ] == 0 )
                     continue;
               }
               bool is_verified = verify_solution(data->problem, sol);
               assert(is_verified);
               assert(data->solutions.empty( ) || data->best_bound >= solvalue);
               data->solutions.emplace_back(sol, solvalue);
               data->best_bound = solvalue;
               SCIPinfoMessage(scip, nullptr, "found new best solution with solution value <%g> in SCIP <%s>\n",
                               solvalue, SCIPgetProbName(scip));
               break;
            }
            default:
               assert(false);
         }
         return SCIP_OKAY;
      }

      static double compute_dual_objective(SCIP *scip) {
         SCIP_VAR **vars;
         int nvars;
         SCIP_CALL(SCIPgetOrigVarsData(scip, &vars, &nvars, nullptr, nullptr, nullptr, nullptr));

//         SCIP_Cons** cons = SCIPgetOrigConss(scip);
//         int ncons = SCIPgetNLPRows(scip);
         auto rows = SCIPgetLPRows(scip);
         int nrows = SCIPgetNLPRows(scip);
         double dual[nrows];
         double red[nvars];
         //TODO: warum sind die Rows nicht 18?
         double dual_obj = 0;
         for( int i = 0; i < nrows; i++ )
         {
            dual[ i ] = SCIProwGetDualsol(rows[ i ]);
//            SCIPinfoMessage(scip, nullptr, "%s: %g\n", SCIProwGetName(rows[ i ]), dual[i]);
            if( dual[ i ] == 0 )
               continue;
            else if( dual[ i ] > 0 )
               dual_obj += SCIProwGetLhs(rows[ i ]) * dual[ i ];
            else
            {
               assert(dual[ i ] < 0);
               dual_obj += SCIProwGetRhs(rows[ i ]) * dual[ i ];
            }
         }
         for( int i = 0; i < nvars; i++ )
         {
            red[ i ] = SCIPgetVarRedcost(scip, vars[ i ]);
//            SCIPinfoMessage(scip, nullptr, "%s: %g\n", SCIPvarGetName(vars[i]), red[ i ]);

            if( red[ i ] == 0 )
               continue;
            else if( red[ i ] < 0 )
               dual_obj += SCIPvarGetUbLocal(vars[ i ]) * red[ i ];
            else
            {
               assert(red[ i ] > 0);
               dual_obj += SCIPvarGetLbLocal(vars[ i ]) * red[ i ];
            }
         }
         return dual_obj;
      }


      /** compare SCIPprintNodeRootPath*/
      static void print_path(SCIP *scip, SCIP_NODE *node) {
         SCIP_VAR **branchvars;
         SCIP_Real *branchbounds;
         SCIP_BOUNDTYPE *boundtypes;
         int *nodeswitches;
         int nbranchvars;
         int nnodes;
         int branchvarssize = SCIPnodeGetDepth(node);
         int nodeswitchsize = branchvarssize;

         /* memory allocation */
         SCIPallocBufferArray(scip, &branchvars, branchvarssize);
         SCIPallocBufferArray(scip, &branchbounds, branchvarssize);
         SCIPallocBufferArray(scip, &boundtypes, branchvarssize);
         SCIPallocBufferArray(scip, &nodeswitches, nodeswitchsize);

         SCIPnodeGetAncestorBranchingPath(node, branchvars, branchbounds, boundtypes, &nbranchvars, branchvarssize,
                                          nodeswitches, &nnodes, nodeswitchsize);

         /* if the arrays were too small, we have to reallocate them and recall SCIPnodeGetAncestorBranchingPath */
         if( nbranchvars > branchvarssize || nnodes > nodeswitchsize )
         {
            branchvarssize = nbranchvars;
            nodeswitchsize = nnodes;

            /* memory reallocation */
            SCIPreallocBufferArray(scip, &branchvars, branchvarssize);
            SCIPreallocBufferArray(scip, &branchbounds, branchvarssize);
            SCIPreallocBufferArray(scip, &boundtypes, branchvarssize);
            SCIPreallocBufferArray(scip, &nodeswitches, nodeswitchsize);
            SCIPnodeGetAncestorBranchingPath(node, branchvars, branchbounds, boundtypes, &nbranchvars, branchvarssize,
                                             nodeswitches, &nnodes, nodeswitchsize);
            assert(nbranchvars == branchvarssize);
         }


         Vec<Assumption> assumptions { };
         /* we only want to create output, if branchings were performed */
         if( nbranchvars >= 1 )
         {

            /* print all nodes, starting from the root, which is last in the arrays */
            for( int j = nnodes - 1; j >= 0; --j )
            {
               int end;
               if( j == nnodes - 1 )
                  end = nbranchvars;
               else
                  end = nodeswitches[ j + 1 ];


               for( int i = nodeswitches[ j ]; i < end; ++i )
               {
                  //TODO: store this path and the farkas lemma
                  SCIP_Real scalar = 0;
                  SCIP_Real constant = 0;
                  SCIPvarGetOrigvarSum(&branchvars[ i ], &scalar, &constant);
                  assumptions.push_back({ SCIPvarGetIndex(( branchvars[ i ] )), branchbounds[ i ],
                                          boundtypes[ i ] == SCIP_BOUNDTYPE_LOWER });

                  SCIPinfoMessage(scip, nullptr, "%s %s %g -", SCIPvarGetName(branchvars[ i ]),
                                  boundtypes[ i ] == SCIP_BOUNDTYPE_LOWER ? ">=" : "<=", branchbounds[ i ]);


               }
            }
            SCIPinfoMessage(scip, nullptr, "\n-----\n");

         }
         for(int i=0; i< assumptions.size() ;i++)
         {
            Assumption a = assumptions[i];
            SCIPinfoMessage(scip, nullptr, "A%d %s %g 1 %d 1 { asm } -1\n", i,
                            a.is_lower ? "L" : "G", a.value, a.index);
         }

         /* free all local memory */
         SCIPfreeBufferArray(scip, &nodeswitches);
         SCIPfreeBufferArray(scip, &boundtypes);
         SCIPfreeBufferArray(scip, &branchbounds);
         SCIPfreeBufferArray(scip, &branchvars);
      }


   };

} // namespace exact

#endif
