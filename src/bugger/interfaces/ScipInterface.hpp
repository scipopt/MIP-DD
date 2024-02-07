/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
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

#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/pub_cons.h"
#include "scip/scip_param.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/misc/Vec.hpp"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"

namespace bugger {

   class ScipInterface : public SolverInterface {

   private:

      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars;

   public:

      explicit ScipInterface( ) {
         if( SCIPcreate(&scip) != SCIP_OKAY || SCIPincludeDefaultPlugins(scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      boost::optional<SolverSettings>
      parseSettings(const std::string &filename) override
      {
         if( !filename.empty() )
         {
            SCIP_RETCODE retcode = SCIPreadParams(scip, filename.c_str());
            if( retcode != SCIP_OKAY )
               return boost::none;
         }

         Vec<std::pair<std::string, bool>> bool_settings;
         Vec<std::pair<std::string, int>> int_settings;
         Vec<std::pair<std::string, long>> long_settings;
         Vec<std::pair<std::string, double>> double_settings;
         Vec<std::pair<std::string, char>> char_settings;
         Vec<std::pair<std::string, std::string>> string_settings;
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

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings);
      }

      void
      doSetUp(const SolverSettings &settings, const Problem<double> &problem, const Solution<double> &solution) override {
         auto retcode = setup(settings, problem, solution);
         assert(retcode == SCIP_OKAY);
      }

      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<double>>>
      readInstance(const std::string &settings_filename, const std::string &problem_filename) override {

         auto settings = parseSettings(settings_filename);
         SCIP_RETCODE retcode = SCIPreadProb(scip, problem_filename.c_str(), nullptr);
         if( retcode != SCIP_OKAY )
            return { settings, boost::none };
         ProblemBuilder<SCIP_Real> builder;

         /* set problem name */
         builder.setProblemName(std::string(SCIPgetProbName(scip)));
         /* set objective offset */
         builder.setObjOffset(SCIPgetOrigObjoffset(scip));
         /* set objective sense */
         builder.setObjSense(SCIPgetObjsense(scip) == SCIP_OBJSENSE_MINIMIZE);

         /* reserve problem memory */
         int ncols = SCIPgetNVars(scip);
         int nrows = SCIPgetNConss(scip);
         int nnz = 0;
         SCIP_VAR **vars = SCIPgetVars(scip);
         SCIP_CONS **conss = SCIPgetConss(scip);
         for( int i = 0; i < nrows; ++i )
         {
            int nconsvars = 0;
            SCIP_Bool success = FALSE;
            SCIPgetConsNVars(scip, conss[ i ], &nconsvars, &success);
            if( !success )
               return { settings, boost::none };
            nnz += nconsvars;
         }
         builder.reserve(nnz, nrows, ncols);

         /* set up columns */
         builder.setNumCols(ncols);
         for( int i = 0; i < ncols; ++i )
         {
            SCIP_VAR *var = vars[ i ];
            SCIP_Real lb = SCIPvarGetLbGlobal(var);
            SCIP_Real ub = SCIPvarGetUbGlobal(var);
            SCIP_VARTYPE vartype = SCIPvarGetType(var);
            builder.setColLb(i, lb);
            builder.setColUb(i, ub);
            builder.setColLbInf(i, SCIPisInfinity(scip, -lb));
            builder.setColUbInf(i, SCIPisInfinity(scip, ub));
            builder.setColIntegral(i, vartype == SCIP_VARTYPE_BINARY || vartype == SCIP_VARTYPE_INTEGER);
            builder.setColImplInt(i, vartype != SCIP_VARTYPE_CONTINUOUS);
            builder.setColName(i, SCIPvarGetName(var));
            builder.setObj(i, SCIPvarGetObj(var));
         }

         /* set up rows */
         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Real> consvals(ncols);
         Vec<int> indices(ncols);
         builder.setNumRows(nrows);
         for( int i = 0; i < nrows; ++i )
         {
            int nconsvars = 0;
            SCIP_Bool success = FALSE;
            SCIP_CONS *cons = conss[ i ];
            SCIPgetConsNVars(scip, cons, &nconsvars, &success);
            SCIPgetConsVars(scip, cons, consvars.data(), ncols, &success);
            if( !success )
               return { settings, boost::none };
            SCIPgetConsVals(scip, cons, consvals.data(), ncols, &success);
            if( !success )
               return { settings, boost::none };
            for( int j = 0; j < nconsvars; ++j )
            {
               indices[ j ] = SCIPvarGetProbindex(consvars[ j ]);
               assert(strcmp(SCIPvarGetName(consvars[ j ]), SCIPvarGetName(vars[ indices[ j ] ])) == 0);
            }
            builder.addRowEntries(i, nconsvars, indices.data(), consvals.data());
            SCIP_Real lhs = SCIPconsGetLhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none };
            SCIP_Real rhs = SCIPconsGetRhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none };
            builder.setRowLhs(i, lhs);
            builder.setRowRhs(i, rhs);
            builder.setRowLhsInf(i, SCIPisInfinity(scip, -lhs));
            builder.setRowRhsInf(i, SCIPisInfinity(scip, rhs));
            builder.setRowName(i, SCIPconsGetName(cons));
         }

         return { settings, builder.build() };
      }

      void
      writeInstance(const std::string &filename, const bool &writesettings) override {
         if( writesettings )
            SCIPwriteParams(scip, (filename + ".set").c_str(), FALSE, TRUE);
         SCIPwriteOrigProblem(scip, (filename + ".cip").c_str(), nullptr, FALSE);
      };

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
      setup(const SolverSettings &settings, const Problem<double> &problem, const Solution<double> &solution) {

         model = &problem;
         reference = &solution;
         bool solution_exists = reference->status == SolutionStatus::kFeasible;
         int ncols = model->getNCols( );
         int nrows = model->getNRows( );
         const Vec<String> &varNames = model->getVariableNames( );
         const Vec<String> &consNames = model->getConstraintNames( );
         const VariableDomains<double> &domains = model->getVariableDomains( );
         const Objective<double> &obj = model->getObjective( );
         const auto &consMatrix = model->getConstraintMatrix( );
         const auto &lhs_values = consMatrix.getLeftHandSides( );
         const auto &rhs_values = consMatrix.getRightHandSides( );
         const auto &rflags = model->getRowFlags( );

         set_parameters(settings);
         SCIP_CALL(SCIPcreateProbBasic(scip, model->getName( ).c_str( )));
         SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));
         SCIP_CALL(SCIPsetObjsense(scip, obj.sense ? SCIP_OBJSENSE_MINIMIZE : SCIP_OBJSENSE_MAXIMIZE));
         vars.resize(model->getNCols( ));
         if( solution_exists )
            value = obj.offset;
         else if( reference->status == SolutionStatus::kUnbounded )
            value = obj.sense ? -SCIPinfinity(scip) : SCIPinfinity(scip);
         else if( reference->status == SolutionStatus::kInfeasible )
            value = obj.sense ? SCIPinfinity(scip) : -SCIPinfinity(scip);

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
               if( domains.flags[ col ].test(ColFlag::kIntegral) )
               {
                  if( lb == 0 && ub == 1 )
                     type = SCIP_VARTYPE_BINARY;
                  else
                     type = SCIP_VARTYPE_INTEGER;
               }
               else if( domains.flags[ col ].test(ColFlag::kImplInt) )
                  type = SCIP_VARTYPE_IMPLINT;
               else
                  type = SCIP_VARTYPE_CONTINUOUS;
               SCIP_CALL(SCIPcreateVarBasic(
                     scip, &var, varNames[ col ].c_str( ), lb, ub,
                     SCIP_Real(obj.coefficients[ col ]), type));
               if( solution_exists )
                  value += obj.coefficients[ col ] * reference->primal[ col ];
               SCIP_CALL(SCIPaddVar(scip, var));
               vars[ col ] = var;
               SCIP_CALL(SCIPreleaseVar(scip, &var));
            }
         }

         Vec<SCIP_VAR*> consvars(model->getNCols( ));
         Vec<SCIP_Real> consvals(model->getNCols( ));
         for( int row = 0; row < nrows; ++row )
         {
            if( model->getRowFlags( )[ row ].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[ row ].test(RowFlag::kLhsInf) || !rflags[ row ].test(RowFlag::kRhsInf));

            auto rowvec = consMatrix.getRowCoefficients(row);
            const double *vals = rowvec.getValues( );
            const int *inds = rowvec.getIndices( );
            SCIP_CONS *cons;

            // the first length entries of consvars/-vals are the entries of the current constraint
            int length = 0;
            for( int k = 0; k != rowvec.getLength( ); ++k )
            {
               assert(!model->getColFlags( )[ inds[ k ] ].test(ColFlag::kFixed));
               assert(vals[ k ] != 0.0);
               consvars[ length ] = vars[ inds[ k ] ];
               consvals[ length ] = SCIP_Real(vals[ k ]);
               ++length;
            }

            SCIP_CALL(SCIPcreateConsBasicLinear(
                  scip, &cons, consNames[ row ].c_str( ), length,
                  consvars.data( ), consvals.data( ),
                  rflags[ row ].test(RowFlag::kLhsInf) ? -SCIPinfinity(scip) : SCIP_Real(lhs_values[ row ]),
                  rflags[ row ].test(RowFlag::kRhsInf) ? SCIPinfinity(scip) : SCIP_Real(rhs_values[ row ])));
            SCIP_CALL(SCIPaddCons(scip, cons));
            SCIP_CALL(SCIPreleaseCons(scip, &cons));
         }

//#if SCIP_VERSION >= 900
////         TODO: test this
//         if( solution_exists )
//            SCIPsetRealParam(scip, "limits/objectivestop", value);
//#endif
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

      std::pair<char, SolverStatus> solve( const Vec<int>& passcodes ) override {

         SolverStatus solverstatus = SolverStatus::kUndefinedError;
         SCIPsetMessagehdlrQuiet(scip, true);
         char retcode = SCIPsolve(scip);
         if( retcode == SCIP_OKAY )
         {
            // reset return code
            retcode = OKAY;

            // initialize primal solution
            Solution<double> solution;

            // check dual by reference solution objective
            if( retcode == OKAY )
               retcode = check_dual_bound( SCIPgetDualbound(scip), SCIPsumepsilon(scip), SCIPinfinity(scip) );

            // check primal by generated solution values
            if( retcode == OKAY )
            {
               SCIP_SOL** sols = SCIPgetSols(scip);
               int nsols = SCIPgetNSols(scip);

               if( nsols >= 1 )
               {
                  solution.status = SolutionStatus::kFeasible;
                  solution.primal.resize(vars.size());

                  for( int i = nsols - 1; i >= 0 && retcode == OKAY; --i )
                  {
                     for( int col = 0; col < solution.primal.size(); ++col )
                        solution.primal[col] = model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : SCIPgetSolVal(scip, sols[i], vars[col]);

                     if( i <= 0 && SCIPhasPrimalRay(scip) )
                     {
                        solution.status = SolutionStatus::kUnbounded;
                        solution.ray.resize(vars.size());

                        for( int col = 0; col < solution.ray.size(); ++col )
                           solution.ray[col] = model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : SCIPgetPrimalRayVal(scip, vars[col]);
                     }

                     retcode = check_primal_solution( solution, SCIPsumepsilon(scip), SCIPinfinity(scip) );
                  }
               }
               else
               {
                  solution.status = SolutionStatus::kInfeasible;
                  retcode = check_primal_solution( solution, SCIPsumepsilon(scip), SCIPinfinity(scip) );
               }
            }

            // check objective by best solution evaluation
            if( retcode == OKAY )
               retcode = check_objective_value( SCIPgetPrimalbound(scip), solution, SCIPsumepsilon(scip), SCIPinfinity(scip) );

            // translate solver status
            switch( SCIPgetStatus(scip) )
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
            // shift retcodes so that all errors have negative values
            --retcode;
         }
         // progess certain passcodes as OKAY based on the user preferences
         for( int passcode: passcodes )
         {
            if( passcode == retcode )
            {
               retcode = OKAY;
               break;
            }
         }
         return { retcode, solverstatus };
      }
   };

   class ScipFactory : public SolverFactory
   {

   public:
      std::unique_ptr<SolverInterface>
      create_solver(  ) const override
      {
         return std::unique_ptr<SolverInterface>( new ScipInterface() );
      }

   };

} // namespace bugger

#endif
