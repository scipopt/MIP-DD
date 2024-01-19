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
#define CATCH_ASSERTIONS

#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scip_param.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"
#include "bugger/interfaces/SolverResult.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "SolverResult.hpp"



namespace bugger {

   class ScipInterface : public SolverInterface {

   private:
      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars;
      double reference = std::numeric_limits<double>::signaling_NaN();

   public:
      explicit ScipInterface( ) {
         if( SCIPcreate(&scip) != SCIP_OKAY || SCIPincludeDefaultPlugins(scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      void
      doSetUp(const Problem<double> &problem, SolverSettings settings, bool solution_exits, const Solution<double> sol) override {
         auto result = setup(problem, solution_exits, sol, settings);
         assert(result == SCIP_OKAY);
      }

      void writeSettings(std::string filename, const SolverSettings& solver_settings ) override {
         set_parameters(solver_settings);
         SCIPwriteParams(scip, filename.c_str(), 0, 1);
      };

      SolverSettings
      parseSettings(const std::string& settings) override
      {
         Vec<std::pair<std::string, bool>> bool_settings;
         Vec<std::pair<std::string, int>> int_settings;
         Vec<std::pair<std::string, long>> long_settings;
         Vec<std::pair<std::string, double>> double_settings;
         Vec<std::pair<std::string, char>> char_settings;
         Vec<std::pair<std::string, std::string>> string_settings;

         if(!settings.empty())
            SCIPreadParams(scip, settings.c_str( ));
         int nparams = SCIPgetNParams(scip);
         SCIP_PARAM **params = SCIPgetParams(scip);

         for( int i = 0; i < nparams; ++i )
         {
            SCIP_PARAM *param;

            param = params[ i ];
            param->isfixed = FALSE;
            switch( SCIPparamGetType(param))
            {
               case SCIP_PARAMTYPE_BOOL:
               {
                  bool bool_val = ( param->data.boolparam.valueptr == nullptr ? param->data.boolparam.curvalue
                                                                              : *param->data.boolparam.valueptr );
                  bool_settings.emplace_back(param->name, bool_val);
                  break;
               }
               case SCIP_PARAMTYPE_INT:
               {
                  int int_value = ( param->data.intparam.valueptr == nullptr ? param->data.intparam.curvalue
                                                                             : *param->data.intparam.valueptr );
                  int_settings.emplace_back( param->name, int_value);
                  break;
               }
               case SCIP_PARAMTYPE_LONGINT:
               {
                  long long_val = ( param->data.longintparam.valueptr == nullptr ? param->data.longintparam.curvalue
                                                                                 : *param->data.longintparam.valueptr );
                  long_settings.emplace_back(param->name, long_val);
                  break;
               }
               case SCIP_PARAMTYPE_REAL:
               {
                  double real_val = ( param->data.realparam.valueptr == nullptr ? param->data.realparam.curvalue
                                                                                : *param->data.realparam.valueptr );
                  double_settings.emplace_back(param->name, real_val);
                  break;
               }
               case SCIP_PARAMTYPE_CHAR:
               {

                  char char_val = ( param->data.charparam.valueptr == nullptr ? param->data.charparam.curvalue
                                                                              : *param->data.charparam.valueptr );
                  char_settings.emplace_back(param->name, char_val);

                  break;
               }
               case SCIP_PARAMTYPE_STRING:
               {
                  std::string string_val = ( param->data.stringparam.valueptr == nullptr ? param->data.stringparam.curvalue
                                                                                         : *param->data.stringparam.valueptr );
                  string_settings.emplace_back(param->name, string_val);
                  break;
               }
               default:
                  SCIPerrorMessage("unknown parameter type\n");
            }
         }

         return {bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings};
      }

      ~ScipInterface( ) override {
         if( scip != nullptr )
         {
            auto retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }

   private:

      SCIP_RETCODE
      setup(const Problem<double> &problem, bool solution_exits, const Solution<double> sol, SolverSettings settings) {

         set_parameters(settings);

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
         SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));
         SCIP_CALL(SCIPsetObjsense(scip, obj.sense ? SCIP_OBJSENSE_MINIMIZE : SCIP_OBJSENSE_MAXIMIZE));
         vars.resize(problem.getNCols( ));

         if( solution_exits )
            reference = obj.offset;
         for( int col = 0; col < ncols; ++col )
         {
            if( domains.flags[ col ].test(ColFlag::kFixed) )
               vars[ col ] = nullptr;
            else
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
               // update lhs and rhs if fixed variable is present
               if( problem.getColFlags( )[ inds[ k ] ].test(ColFlag::kFixed) )
               {
                  if( !rflags[ row ].test(RowFlag::kLhsInf) )
                     lhs -= vals[ k ] * problem.getLowerBounds( )[ inds[ k ] ];
                  if( !rflags[ row ].test(RowFlag::kRhsInf) )
                     rhs -= vals[ k ] * problem.getLowerBounds( )[ inds[ k ] ];
               }
               else
               {
                  consvars[ length ] = vars[ inds[ k ] ];
                  consvals[ length ] = SCIP_Real(vals[ k ]);
                  ++length;
               }
            }

            SCIP_CALL(SCIPcreateConsBasicLinear(
                  scip, &cons, consNames[ row ].c_str( ), length,
                  consvars.data( ), consvals.data( ), lhs, rhs));
            SCIP_CALL(SCIPaddCons(scip, cons));
            SCIP_CALL(SCIPreleaseCons(scip, &cons));
         }

         return SCIP_OKAY;
      }

      void set_parameters(const SolverSettings &settings) const {
         for(const auto& pair : settings.getBoolSettings())
            SCIPsetBoolParam(scip, pair.first.c_str(), pair.second);
         for(const auto& pair : settings.getIntSettings())
            SCIPsetIntParam(scip, pair.first.c_str(), pair.second);
         for(const auto& pair : settings.getLongSettings())
            SCIPsetLongintParam(scip, pair.first.c_str(), pair.second);
         for(const auto& pair : settings.getDoubleSettings())
            SCIPsetRealParam(scip, pair.first.c_str(), pair.second);
         for(const auto& pair : settings.getCharSettings())
            SCIPsetCharParam(scip, pair.first.c_str(), pair.second);
         for(const auto& pair : settings.getStringSettings())
            SCIPsetStringParam(scip, pair.first.c_str(), pair.second.c_str());
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

   private:

      static
      Problem<SCIP_Real> buildProblem(
            SCIP *scip               /**< SCIP data structure */
      ) {
         SCIP_MATRIX* matrix;
         ProblemBuilder<SCIP_Real> builder;

         /* build problem from matrix */
         int nnz = SCIPgetNNZs(scip);
         int nvars = SCIPgetNVars(scip);
         int nrows = SCIPgetNConss(scip);
         builder.reserve(nnz, nrows, nvars);
         builder.setProblemName(SCIPgetProbName(scip));
         builder.setObjOffset(SCIPgetOrigObjoffset(scip));
         builder.setObjSense(SCIPgetObjsense(scip) == SCIP_OBJSENSE_MINIMIZE);

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
         (void)SCIPmatrixCreate(scip, &matrix, FALSE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
         nrows = SCIPmatrixGetNRows(matrix);
         builder.setNumRows(nrows);
         for( int i = 0; i != nrows; ++i )
         {
            int *rowcols = SCIPmatrixGetRowIdxPtr(matrix, i);
            SCIP_Real *rowvals = SCIPmatrixGetRowValPtr(matrix, i);
            int rowlen = SCIPmatrixGetRowNNonzs(matrix, i);
            builder.addRowEntries(i, rowlen, rowcols, rowvals);
            SCIP_Real lhs = SCIPmatrixGetRowLhs(matrix, i);
            SCIP_Real rhs = SCIPmatrixGetRowRhs(matrix, i);
            builder.setRowLhs(i, lhs);
            builder.setRowRhs(i, rhs);
            builder.setRowLhsInf(i, SCIPisInfinity(scip, -lhs));
            builder.setRowRhsInf(i, SCIPisInfinity(scip, rhs));
         }
         SCIPmatrixFree(scip, &matrix);

         return builder.build( );
      }

      std::pair<char, SolverStatus> solve( ) override {

         SolverStatus solverstatus = SolverStatus::kUnknown;
         SCIPsetMessagehdlrQuiet(scip, true);
         char retcode = SCIPsolve(scip);
         if( retcode == SCIP_OKAY )
         {
            if( SCIPisSumNegative(scip, SCIPgetObjsense(scip) * ( reference - SCIPgetDualbound(scip))))
               retcode = BUG_REPRODUCED;
            else
               retcode = OKAY;

            switch( SCIPgetStatus(scip))
            {
               case SCIP_STATUS_UNKNOWN:
                  solverstatus = SolverStatus::kUnknown;
                  break;
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
                  solverstatus = SolverStatus::kLimit;
                  break;
               case SCIP_STATUS_INFORUNBD:
                  solverstatus = SolverStatus::kInfeasibleOrUnbounded;
                  break;
               case SCIP_STATUS_INFEASIBLE:
                  solverstatus = SolverStatus::kInfeasible;
                  break;
               case SCIP_STATUS_UNBOUNDED:
                  solverstatus = SolverStatus::kUnbounded;
                  break;
               case SCIP_STATUS_OPTIMAL:
                  solverstatus = SolverStatus::kOptimal;
                  break;
            }
         }
         else
         {
            solverstatus = SolverStatus::kUndefinedError;
            Vec<char> passcodes{};
            // progess certain passcodes as OKAY based on the user preferences
            for(char passcode: passcodes)
               if( passcode == retcode )
                  return {0, solverstatus};
            // shift retcodes so that all errors have negative values
            retcode--;
         }
         return { retcode, solverstatus };
      }

   };

} // namespace bugger

#endif
