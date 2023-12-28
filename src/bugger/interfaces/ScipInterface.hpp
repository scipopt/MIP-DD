/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
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

#ifndef BUGGER_INTERFACES_SCIP_INTERFACE_HPP_
#define BUGGER_INTERFACES_SCIP_INTERFACE_HPP_

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"
#include "SolverStatus.hpp"

namespace bugger {

   #define SCIP_CALL_RETURN(x)                                        \
   do                                                                 \
   {                                                                  \
      SCIP_RETCODE _restat_;                                          \
      if( (_restat_ = (x)) != SCIP_OKAY )                             \
      {                                                               \
         SCIPerrorMessage("Error <%d> in function call\n", _restat_); \
         return SolverStatus::kError;                                 \
      }                                                               \
   }                                                                  \
   while( FALSE )


   class ScipInterface : public SolverInterface {

   private:
      const std::string& setting;
      SCIP* scip;
      double reference = INFINITY;
      Vec<SCIP_VAR*> vars;


   public:
      ScipInterface(const std::string& _setting) : setting(_setting), scip(nullptr) {
         if( SCIPcreate(&scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      void
      doSetUp(const Problem<double> &problem, bool solution_exits, const Solution<double> sol) override {
         auto result = setup(problem, solution_exits, sol);
         assert(result == SCIP_OKAY);
      }

      static boost::optional<Problem<double>>
      readProblem(const std::string &filename) {
         Problem<double> problem;
         SCIP *scip = NULL;

         SCIPcreate(&scip);
         SCIPincludeDefaultPlugins(scip);
         //TODO: this can be done easier
         auto retcode = SCIPreadProb(scip, filename.c_str( ), NULL);
         if( retcode != SCIP_OKAY )
            return problem;
         return buildProblem(scip);

      }

      void modify_parameters(int nbatches) override {
         SCIP_PARAM *batch;

         int *inds;
         int nparams = SCIPgetNParams(scip);
         SCIP_PARAM **params = SCIPgetParams(scip);
         int batchsize = 1;

         if( nbatches > 0 )
         {
            batchsize = nbatches - 1;

            for( int i = 0; i < nparams; ++i )
               if( isSettingAdmissible(params[ i ]))
                  ++batchsize;

            batchsize /= nbatches;
         }

         (SCIPallocBufferArray(scip, &inds, batchsize));
         (SCIPallocBufferArray(scip, &batch, batchsize));
         int nbatch = 0;

         for( int i = 0; i < nparams; ++i )
         {
            SCIP_PARAM *param;

            param = params[ i ];

            if( isSettingAdmissible(param))
            {
               inds[ nbatch ] = i;
               batch[ nbatch ].isfixed = param->isfixed;
               param->isfixed = FALSE;

               switch( SCIPparamGetType(param))
               {
                  case SCIP_PARAMTYPE_BOOL:
                  {
                     //TODO: that does not work
                     bool *ptrbool;
//                     ptrbool = ( param->data.boolparam.valueptr == nullptr ? &param->data.boolparam.curvalue
//                                                                           : param->data.boolparam.valueptr );
//                     batch[ nbatch ].data.boolparam.curvalue = *ptrbool;
//                     *ptrbool = param->data.boolparam.defaultvalue;
                     break;
                  }
                  case SCIP_PARAMTYPE_INT:
                  {
                     int *ptrint;
                     ptrint = ( param->data.intparam.valueptr == nullptr ? &param->data.intparam.curvalue
                                                                         : param->data.intparam.valueptr );
                     batch[ nbatch ].data.intparam.curvalue = *ptrint;
                     *ptrint = param->data.intparam.defaultvalue;
                     break;
                  }
                  case SCIP_PARAMTYPE_LONGINT:
                  {
                     SCIP_Longint *ptrlongint;
                     ptrlongint = ( param->data.longintparam.valueptr == nullptr ? &param->data.longintparam.curvalue
                                                                                 : param->data.longintparam.valueptr );
                     batch[ nbatch ].data.longintparam.curvalue = *ptrlongint;
                     *ptrlongint = param->data.longintparam.defaultvalue;
                     break;
                  }
                  case SCIP_PARAMTYPE_REAL:
                  {
                     SCIP_Real *ptrreal;
                     ptrreal = ( param->data.realparam.valueptr == nullptr ? &param->data.realparam.curvalue
                                                                           : param->data.realparam.valueptr );
                     batch[ nbatch ].data.realparam.curvalue = *ptrreal;
                     *ptrreal = param->data.realparam.defaultvalue;
                     break;
                  }
                  case SCIP_PARAMTYPE_CHAR:
                  {
                     char *ptrchar;
                     ptrchar = ( param->data.charparam.valueptr == nullptr ? &param->data.charparam.curvalue
                                                                           : param->data.charparam.valueptr );
                     batch[ nbatch ].data.charparam.curvalue = *ptrchar;
                     *ptrchar = param->data.charparam.defaultvalue;
                     break;
                  }
                  case SCIP_PARAMTYPE_STRING:
                  {
                     char **ptrstring;
                     ptrstring = ( param->data.stringparam.valueptr == nullptr ? &param->data.stringparam.curvalue
                                                                               : param->data.stringparam.valueptr );
                     batch[ nbatch ].data.stringparam.curvalue = *ptrstring;
                     (BMSduplicateMemoryArray(ptrstring, param->data.stringparam.defaultvalue,
                                              strlen(param->data.stringparam.defaultvalue) + 1));
                     break;
                  }
                  default:
                     SCIPerrorMessage("unknown parameter type\n");
               }

               ++nbatch;
            }

         }

      }


      BuggerStatus run(const Message &msg, SolverStatus originalStatus) override {

         //TODO: Expect failing assertion
         SolverStatus status = solve( );
         BuggerStatus result = BuggerStatus::kSuccess;
         if( status == SolverStatus::kError )
            result = BuggerStatus::kUnexpectedError;
         else if( SCIPisSumNegative(scip, SCIPgetObjsense(scip) * (reference - SCIPgetDualbound(scip))) )
            result = BuggerStatus::kFail;

         switch( result )
         {
            case BuggerStatus::kSuccess:
               msg.info("\tError could not be reproduced\n");
               break;
            case BuggerStatus::kFail:
               msg.info("\tError could be reproduced\n");
               break;
            case BuggerStatus::kUnexpectedError:
               msg.info("\tAn error was returned\n");
               break;
         }

         // TODO: what are passcodes doing?
//         for( int i = 0; i < presoldata->npasscodes; ++i )
//            if( retcode == presoldata->passcodes[i] )
//               return 0;

         return result;
      }

      ~ScipInterface( ) {
         if( scip != nullptr )
         {
//            SCIP_RETCODE retcode = SCIPfreeSol(scip, &solution);
//            UNUSED(retcode);
//            assert(retcode == SCIP_OKAY);
            auto retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }

   private:

      bool isSettingAdmissible(SCIP_PARAM *param) {
         /* keep reading and writing settings because input and output is not monitored */
         return ( param->isfixed || !SCIPparamIsDefault(param))
                && strncmp("display/", SCIPparamGetName(param), 8) != 0 &&
                strncmp("limits/", SCIPparamGetName(param), 7) != 0
                && strncmp("reading/", SCIPparamGetName(param), 8) != 0 &&
                strncmp("write/", SCIPparamGetName(param), 6) != 0;
      }

      SCIP_RETCODE
      setup(const Problem<double> &problem, bool solution_exits, const Solution<double> sol) {
         SCIP_CALL(SCIPincludeDefaultPlugins(scip));
         SCIP_CALL(SCIPreadParams(scip, setting.c_str( )));
         int ncols = problem.getNCols( );
         int nrows = problem.getNRows( );
         const Vec<String> &varNames = problem.getVariableNames( );
         const Vec<String> &consNames = problem.getConstraintNames( );
         const VariableDomains<double> &domains = problem.getVariableDomains( );
         const Objective<double> &obj = problem.getObjective( );
         const auto &consMatrix = problem.getConstraintMatrix( );
         const auto &lhs_values = consMatrix.getLeftHandSides( );
         const auto &rhs_values = consMatrix.getRightHandSides( );
         const auto &rflags = problem.getRowFlags( );

         SCIP_CALL(SCIPcreateProbBasic(scip, problem.getName( ).c_str( )));

         vars.resize(problem.getNCols( ));

         if( solution_exits )
            reference = 0;

         for( int col = 0; col < ncols; ++col )
         {
            SCIP_VAR *var;


            SCIP_Real lb = domains.flags[ col ].test(ColFlag::kLbInf)
                           ? -SCIPinfinity(scip)
                           : SCIP_Real(domains.lower_bounds[ col ]);
            SCIP_Real ub = domains.flags[ col ].test(ColFlag::kUbInf)
                           ? SCIPinfinity(scip)
                           : SCIP_Real(domains.upper_bounds[ col ]);
            assert(!domains.flags[ col ].test(ColFlag::kInactive) || ( lb == ub ));
            SCIP_VARTYPE type;
            if( domains.flags[ col ].test(ColFlag::kIntegral))
            {
               if( lb == 0 && ub == 1 )
                  type = SCIP_VARTYPE_BINARY;
               else
                  type = SCIP_VARTYPE_INTEGER;
            }
            else if( domains.flags[ col ].test(ColFlag::kImplInt))
               type = SCIP_VARTYPE_IMPLINT;
            else
               type = SCIP_VARTYPE_CONTINUOUS;

            SCIP_CALL(SCIPcreateVarBasic(
                  scip, &var, varNames[ col ].c_str( ), lb, ub,
                  SCIP_Real(obj.coefficients[ col ]), type));
            if( solution_exits )
               reference += obj.coefficients[ col ] * sol.primal[ col ];
            SCIP_CALL(SCIPaddVar(scip, var));
            vars[ col ] = var;

            SCIP_CALL(SCIPreleaseVar(scip, &var));
         }

         Vec<SCIP_VAR *> consvars;
         Vec<SCIP_Real> consvals;
         consvars.resize(problem.getNCols( ));
         consvals.resize(problem.getNCols( ));

         for( int row = 0; row < nrows; ++row )
         {
            if( problem.getRowFlags( )[ row ].test(RowFlag::kRedundant))
               continue;
            assert(!rflags[ row ].test(RowFlag::kLhsInf) || !rflags[ row ].test(RowFlag::kRhsInf));
            SCIP_CONS *cons;

            auto rowvec = consMatrix.getRowCoefficients(row);
            const double *vals = rowvec.getValues( );
            const int *inds = rowvec.getIndices( );
            SCIP_Real lhs = rflags[ row ].test(RowFlag::kLhsInf)
                            ? -SCIPinfinity(scip)
                            : SCIP_Real(lhs_values[ row ]);
            SCIP_Real rhs = rflags[ row ].test(RowFlag::kRhsInf)
                            ? SCIPinfinity(scip)
                            : SCIP_Real(rhs_values[ row ]);

            // the first length entries of consvars/-vals are the entries of the current constraint
            int length = 0;
            for( int k = 0; k != rowvec.getLength( ); ++k )
            {
               if( problem.getColFlags( )[ k ].test(ColFlag::kFixed) )
               {
                  double value = problem.getLowerBounds( )[ inds[ k ] ];
                  assert(value == problem.getUpperBounds( )[ inds[ k ] ]);
                  if( value != 0.0 )
                  {
                     // update lhs and rhs if fixed variable is still present
                     if( !rflags[ row ].test(RowFlag::kLhsInf) )
                        lhs -= ( vals[ k ] * value );
                     if( !rflags[ row ].test(RowFlag::kRhsInf) )
                        rhs -= ( vals[ k ] * value );
                  }
                  continue;
               }
               consvars[ length ] = vars[ inds[ k ] ];
               consvals[ length ] = SCIP_Real(vals[ k ]);
               ++length;
            }

            SCIP_CALL(SCIPcreateConsBasicLinear(
                  scip, &cons, consNames[ row ].c_str( ), length,
                  consvars.data( ), consvals.data( ), lhs, rhs));
            SCIP_CALL(SCIPaddCons(scip, cons));
            SCIP_CALL(SCIPreleaseCons(scip, &cons));
         }


         if( obj.offset != 0 )
            SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));

         return SCIP_OKAY;
      }

      SCIP_SOL *
      add_solution(Solution<double> sol) {
         SCIP_SOL *s;
         SCIP_RETCODE retcode;
         retcode = SCIPcreateSol(scip, &s, nullptr);
         assert(retcode == SCIP_OKAY);

         for( int i = 0; i < sol.primal.size( ); i++ )
         {
            retcode = SCIPsetSolVal(scip, s, vars[ i ], sol.primal[ i ]);
            assert(retcode == SCIP_OKAY);
         }
         return s;
      }

      SolverStatus solve( ) override {

         SCIPsetMessagehdlrQuiet(scip, true);

//         //TODO: Support initial solutions
//         for( int i = 0; i < nsols; ++i )
//         {
//            SCIP_SOL *sol;
//            sol = sols[ i ];
//            if( sol == solution )
//            {
//               reference = SCIPgetSolOrigObj(scip, sol);
//            }
//            else
//            {
//               SCIP_SOL *initsol = NULL;
//
//               SCIP_CALL_RETURN(SCIPtranslateSubSol(*test, scip, sol, NULL, SCIPgetOrigVars(scip), &initsol));
//
//               if( initsol == NULL )
//                  SCIP_CALL_ABORT(SCIP_INVALIDCALL);
//
//               SCIP_CALL_RETURN(SCIPaddSolFree(*test, &initsol, &valid));
//            }
//         }

         SCIP_CALL_RETURN(SCIPsolve(scip));

         switch( SCIPgetStatus(scip))
         {
            case SCIP_STATUS_UNKNOWN:
               return SolverStatus::kError;
            case SCIP_STATUS_USERINTERRUPT:
            case SCIP_STATUS_NODELIMIT:
            case SCIP_STATUS_TOTALNODELIMIT:
            case SCIP_STATUS_STALLNODELIMIT:
            case SCIP_STATUS_TIMELIMIT:
            case SCIP_STATUS_MEMLIMIT:
            case SCIP_STATUS_GAPLIMIT:
            case SCIP_STATUS_SOLLIMIT:
            case SCIP_STATUS_BESTSOLLIMIT:
            case SCIP_STATUS_RESTARTLIMIT:
#if SCIP_VERSION_MAJOR >= 6
            case SCIP_STATUS_TERMINATE:
#endif
               return SolverStatus::kLimit;
            case SCIP_STATUS_INFORUNBD:
               return SolverStatus::kInfeasibleOrUnbounded;
            case SCIP_STATUS_INFEASIBLE:
               return SolverStatus::kInfeasible;
            case SCIP_STATUS_UNBOUNDED:
               return SolverStatus::kUnbounded;
            case SCIP_STATUS_OPTIMAL:
               return SolverStatus::kOptimal;
         }


         return SolverStatus::kError;
      }

      static
      Problem<SCIP_Real> buildProblem(
            SCIP *scip               /**< SCIP data structure */
      ) {
         ProblemBuilder<SCIP_Real> builder;

         /* build problem from matrix */
         int nnz = SCIPgetNNZs(scip);
         int nvars = SCIPgetNVars(scip);
         int nrows = SCIPgetNConss(scip);
         builder.reserve(nnz, nrows, nvars);
         /* set up columns */
         builder.setNumCols(nvars);
         auto vars = SCIPgetVars(scip);
         for( int i = 0; i != nvars; ++i )
         {
            SCIP_VAR *var = vars[ i ];
            SCIP_Real lb = SCIPvarGetLbGlobal(var);
            SCIP_Real ub = SCIPvarGetUbGlobal(var);
            builder.setColLb(i, lb);
            builder.setColUb(i, ub);
            builder.setColLbInf(i, SCIPisInfinity(scip, -lb));
            builder.setColUbInf(i, SCIPisInfinity(scip, ub));
            builder.setColIntegral(i, SCIPvarIsIntegral(var));
            builder.setObj(i, SCIPvarGetObj(var));
         }

         /* set up rows */
         builder.setNumRows(nrows);
         auto conss = SCIPgetConss(scip);

//         TODO: iterate over the linear constraint handler and add the data.
//         for( int i = 0; i != nrows; ++i )
//         {
//            auto row = SCIPgetRowLinear(scip, conss[ i ]);
//            int *rowcols = SCIPgetRow(matrix, i);
//            SCIP_Real *rowvals = SCIPmatrixGetRowValPtr(matrix, i);
//            int rowlen = SCIProwGetNNonz(row);
//            builder.addRowEntries(i, rowlen, rowcols, rowvals);
//
//            SCIP_Real lhs = SCIProwGetLhs(row);
//            SCIP_Real rhs = SCIProwGetRhs(row);
//            builder.setRowLhs(i, lhs);
//            builder.setRowRhs(i, rhs);
//            builder.setRowLhsInf(i, SCIPisInfinity(scip, -lhs));
//            builder.setRowRhsInf(i, SCIPisInfinity(scip, rhs));
//         }

         //TODO: add objective offset.
         builder.setObjOffset(0);

         return builder.build( );
      }


   };

} // namespace bugger

#endif
