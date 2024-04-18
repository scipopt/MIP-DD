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

#ifndef __BUGGER_INTERFACES_SCIPINTERFACE_HPP__
#define __BUGGER_INTERFACES_SCIPINTERFACE_HPP__

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/pub_cons.h"
#include "scip/scip_param.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


namespace bugger {

   class ScipParameters {

   public:

      int mode = -1;
      double limitspace = 1.0;
      bool set_dual_limit = true;
      bool set_prim_limit = true;
      bool set_best_limit = true;
      bool set_solu_limit = true;
      bool set_rest_limit = true;
      bool set_tota_limit = true;
      bool set_time_limit = false;
   };

   template <typename REAL>
   class ScipInterface : public SolverInterface<REAL> {

   public:

      enum Limit : char {
         DUAL = 1,
         PRIM = 2,
         BEST = 3,
         SOLU = 4,
         REST = 5,
         TOTA = 6,
         TIME = 7
      };

   private:

      const ScipParameters& parameters;
      const HashMap<String, char>& limits;
      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars;

   public:

      explicit ScipInterface(const Message& _msg, const ScipParameters& _parameters,
                             const HashMap<String, char>& _limits) : SolverInterface<REAL>(_msg), parameters(_parameters),
                             limits(_limits) {
         if( SCIPcreate(&scip) != SCIP_OKAY || SCIPincludeDefaultPlugins(scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      void
      print_header( ) const override
      {
         SCIPprintVersion(scip, nullptr);
         int length = SCIPgetNExternalCodes(scip);
         auto description = SCIPgetExternalCodeDescriptions(scip);
         auto names = SCIPgetExternalCodeNames(scip);
         for( int i = 0; i < length; ++i )
         {
            String n { names[i] };
            String d { description[i] };
            this->msg.info("\t{:20} {}\n", n,d);
         }
      }

      bool
      has_setting(const String& name) const override
      {
         return SCIPgetParam(scip, name.c_str()) != nullptr;
      }

      boost::optional<SolverSettings>
      parseSettings(const String& filename) const override
      {
         if( !filename.empty() )
         {
            SCIP_RETCODE retcode = SCIPreadParams(scip, filename.c_str());
            if( retcode != SCIP_OKAY )
               return boost::none;
         }

         Vec<std::pair<String, bool>> bool_settings;
         Vec<std::pair<String, int>> int_settings;
         Vec<std::pair<String, long>> long_settings;
         Vec<std::pair<String, double>> double_settings;
         Vec<std::pair<String, char>> char_settings;
         Vec<std::pair<String, String>> string_settings;
         Vec<std::pair<String, long long>> limit_settings;
         int nparams = SCIPgetNParams(scip);
         SCIP_PARAM** params = SCIPgetParams(scip);

         for( int i = 0; i < nparams; ++i )
         {
            SCIP_PARAM* param = params[ i ];
            String name { param->name };
            auto limit = limits.find(name);
            if( limit != limits.end() )
            {
               switch( limit->second )
               {
               case DUAL:
               case PRIM:
                  break;
               case BEST:
               case SOLU:
               case REST:
                  limit_settings.emplace_back( name, param->data.intparam.valueptr == nullptr
                                                   ? param->data.intparam.curvalue
                                                   : *param->data.intparam.valueptr );
                  break;
               case TOTA:
                  limit_settings.emplace_back( name, param->data.longintparam.valueptr == nullptr
                                                   ? param->data.longintparam.curvalue
                                                   : *param->data.longintparam.valueptr );
                  break;
               case TIME:
                  limit_settings.emplace_back( name, std::min(std::ceil(param->data.realparam.valueptr == nullptr
                                                                      ? param->data.realparam.curvalue
                                                                      : *param->data.realparam.valueptr), (SCIP_Real)LLONG_MAX) );
                  break;
               default:
                  SCIPerrorMessage("unknown limit type\n");
               }
            }
            else
            {
               switch( param->paramtype )
               {
               case SCIP_PARAMTYPE_BOOL:
                  bool_settings.emplace_back( name, param->data.boolparam.valueptr == nullptr
                                                  ? param->data.boolparam.curvalue
                                                  : *param->data.boolparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_INT:
                  int_settings.emplace_back( name, param->data.intparam.valueptr == nullptr
                                                 ? param->data.intparam.curvalue
                                                 : *param->data.intparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_LONGINT:
                  long_settings.emplace_back( name, param->data.longintparam.valueptr == nullptr
                                                  ? param->data.longintparam.curvalue
                                                  : *param->data.longintparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_REAL:
                  double_settings.emplace_back( name, param->data.realparam.valueptr == nullptr
                                                    ? param->data.realparam.curvalue
                                                    : *param->data.realparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_CHAR:
                  char_settings.emplace_back( name, param->data.charparam.valueptr == nullptr
                                                  ? param->data.charparam.curvalue
                                                  : *param->data.charparam.valueptr );
                  break;
               case SCIP_PARAMTYPE_STRING:
                  string_settings.emplace_back( name, param->data.stringparam.valueptr == nullptr
                                                    ? param->data.stringparam.curvalue
                                                    : *param->data.stringparam.valueptr );
                  break;
               default:
                  SCIPerrorMessage("unknown setting type\n");
               }
            }
         }

         return SolverSettings(bool_settings, int_settings, long_settings, double_settings,char_settings, string_settings, limit_settings);
      }

      void
      doSetUp(SolverSettings& settings, const Problem<double>& problem, const Solution<double>& solution) override {
         auto retcode = setup(settings, problem, solution);
         assert(retcode == SCIP_OKAY);
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override {

         char retcode = SCIP_ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;
         SCIPsetMessagehdlrQuiet(scip, this->msg.getVerbosityLevel() < VerbosityLevel::kDetailed);
         // optimize
         if( parameters.mode == -1 )
            retcode = SCIPsolve(scip);
         // count
         else
         {
            retcode = SCIPsetParamsCountsols(scip);
            if( retcode == SCIP_OKAY )
               retcode = SCIPcount(scip);
         }

         if( retcode == SCIP_OKAY )
         {
            // reset return code
            retcode = Retcode::OKAY;

            if( parameters.mode == -1 )
            {
               // retrieve enabled checks
               bool dual = true;
               bool primal = true;
               bool objective = true;

               for( int passcode: passcodes )
               {
                  switch( passcode )
                  {
                  case Retcode::DUALFAIL:
                     dual = false;
                     break;
                  case Retcode::PRIMALFAIL:
                     primal = false;
                     break;
                  case Retcode::OBJECTIVEFAIL:
                     objective = false;
                     break;
                  }
               }

               // declare primal solution
               Vec<Solution<double>> solution;
               SCIP_SOL** sols = SCIPgetSols(scip);
               int nsols = SCIPgetNSols(scip);

               // check dual by reference solution objective
               if( retcode == Retcode::OKAY && dual )
                  retcode = this->check_dual_bound( SCIPgetDualbound(scip), SCIPsumepsilon(scip), SCIPinfinity(scip) );

               // check primal by generated solution values
               if( retcode == Retcode::OKAY )
               {
                  if( nsols >= 1 )
                  {
                     solution.resize(primal ? nsols : objective ? 1 : 0);

                     for( int i = solution.size() - 1; i >= 0; --i )
                     {
                        solution[i].status = SolutionStatus::kFeasible;
                        solution[i].primal.resize(vars.size());

                        for( int col = 0; col < solution[i].primal.size(); ++col )
                           solution[i].primal[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : SCIPgetSolVal(scip, sols[i], vars[col]);
                     }

                     if( solution.size() >= 1 && SCIPhasPrimalRay(scip) )
                     {
                        solution[0].status = SolutionStatus::kUnbounded;
                        solution[0].ray.resize(vars.size());

                        for( int col = 0; col < solution[0].ray.size(); ++col )
                           solution[0].ray[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<double>::signaling_NaN() : SCIPgetPrimalRayVal(scip, vars[col]);
                     }

                     if( primal )
                        retcode = this->check_primal_solution( solution, SCIPsumepsilon(scip), SCIPinfinity(scip) );
                  }
                  else if( nsols != 0 && primal )
                     retcode = Retcode::PRIMALFAIL;
               }

               // check objective by best solution evaluation
               if( retcode == Retcode::OKAY && objective )
               {
                  // check solution objective instead of primal bound if no ray is provided
                  double bound = abs(SCIPgetPrimalbound(scip)) == SCIPinfinity(scip) && solution.size() >= 1 && solution[0].status == SolutionStatus::kFeasible ? SCIPgetSolOrigObj(scip, sols[0]) : SCIPgetPrimalbound(scip);

                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = this->check_objective_value( bound, solution[0], SCIPsumepsilon(scip), SCIPinfinity(scip) );
               }
            }
            else
            {
               // check count by primal solution existence
               if( retcode == Retcode::OKAY )
               {
                  long long int count;
                  unsigned int valid;

                  count = SCIPgetNCountedSols(scip, &valid);
                  retcode = this->check_count_number( SCIPgetDualbound(scip), SCIPgetPrimalbound(scip), (valid ? count : -1), SCIPinfinity(scip) );
               }
            }

            // translate solver status
            switch( SCIPgetStatus(scip) )
            {
               case SCIP_STATUS_UNKNOWN:
                  solverstatus = SolverStatus::kUnknown;
                  break;
               case SCIP_STATUS_TOTALNODELIMIT:
                  solverstatus = SolverStatus::kTotalNodeLimit;
                  break;
               case SCIP_STATUS_STALLNODELIMIT:
                  solverstatus = SolverStatus::kStallNodeLimit;
                  break;
               case SCIP_STATUS_NODELIMIT:
                  solverstatus = SolverStatus::kNodeLimit;
                  break;
               case SCIP_STATUS_TIMELIMIT:
                  solverstatus = SolverStatus::kTimeLimit;
                  break;
               case SCIP_STATUS_GAPLIMIT:
                  solverstatus = SolverStatus::kGapLimit;
                  break;
#if SCIP_VERSION_API >= 115
               case SCIP_STATUS_PRIMALLIMIT:
                  solverstatus = SolverStatus::kPrimalLimit;
                  break;
               case SCIP_STATUS_DUALLIMIT:
                  solverstatus = SolverStatus::kDualLimit;
                  break;
#endif
               case SCIP_STATUS_MEMLIMIT:
                  solverstatus = SolverStatus::kMemLimit;
                  break;
               case SCIP_STATUS_SOLLIMIT:
                  solverstatus = SolverStatus::kSolLimit;
                  break;
               case SCIP_STATUS_BESTSOLLIMIT:
                  solverstatus = SolverStatus::kBestSolLimit;
                  break;
               case SCIP_STATUS_RESTARTLIMIT:
                  solverstatus = SolverStatus::kRestartLimit;
                  break;
               case SCIP_STATUS_USERINTERRUPT:
                  solverstatus = SolverStatus::kInterrupt;
                  break;
#if SCIP_VERSION_API >= 22
               case SCIP_STATUS_TERMINATE:
                  solverstatus = SolverStatus::kTerminate;
                  break;
#endif
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
               retcode = Retcode::OKAY;
               break;
            }
         }
         // restrict limit settings
         if( retcode != Retcode::OKAY )
         {
            const auto& limitsettings = this->adjustment->getLimitSettings( );
            for( int index = 0; index < limitsettings.size( ); ++index )
            {
               if( limitsettings[index].second < 0 || limitsettings[index].second > 1 )
               {
                  double bound;
                  switch( limits.find(limitsettings[index].first)->second )
                  {
                  case BEST:
                     // incremented to continue after finding the last best solution
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * SCIPgetNBestSolsFound(scip) + 1.0, 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case SOLU:
                     // incremented to continue after finding the last solution
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * SCIPgetNSolsFound(scip) + 1.0, 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case REST:
                     // decremented from runs to restarts
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * (SCIPgetNRuns(scip) - 1.0), 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case TOTA:
                     // assumes last node is processed
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * SCIPgetNTotalNodes(scip), 1.0));
                     if( bound > LONG_MAX )
                        continue;
                     else
                        break;
                  case TIME:
                     // sensitive to processor speed variability
                     bound = std::ceil(std::max((1.0 + parameters.limitspace) * SCIPgetSolvingTime(scip), 1.0));
                     if( bound > LLONG_MAX )
                        continue;
                     else
                        break;
                  case DUAL:
                  case PRIM:
                  default:
                     SCIPerrorMessage("unknown limit type\n");
                  }
                  if( limitsettings[index].second < 0 || bound < limitsettings[index].second )
                  {
                     this->msg.info("\t\t{} = {}\n", limitsettings[index].first, (long long)bound);
                     this->adjustment->setLimitSettings(index, bound);
                  }
               }
            }
         }
         return { retcode, solverstatus };
      }

      long long
      getSolvingEffort( ) override {

         switch( SCIPgetStage(scip) )
         {
         case SCIP_STAGE_PRESOLVING:
         case SCIP_STAGE_PRESOLVED:
         case SCIP_STAGE_SOLVING:
         case SCIP_STAGE_SOLVED:
            return SCIPgetNLPIterations(scip);
         case SCIP_STAGE_INIT:
         case SCIP_STAGE_PROBLEM:
         case SCIP_STAGE_TRANSFORMING:
         case SCIP_STAGE_TRANSFORMED:
         case SCIP_STAGE_INITPRESOLVE:
         case SCIP_STAGE_EXITPRESOLVE:
         case SCIP_STAGE_INITSOLVE:
         case SCIP_STAGE_EXITSOLVE:
         case SCIP_STAGE_FREETRANS:
         case SCIP_STAGE_FREE:
         default:
            return -1;
         }
      }

      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<double>>>
      readInstance(const String& settings_filename, const String& problem_filename) override {

         auto settings = parseSettings(settings_filename);
         SCIP_RETCODE retcode = SCIPreadProb(scip, problem_filename.c_str(), nullptr);
         if( retcode != SCIP_OKAY )
            return { settings, boost::none };
         ProblemBuilder<SCIP_Real> builder;

         // set problem name
         builder.setProblemName(String(SCIPgetProbName(scip)));
         // set objective offset
         builder.setObjOffset(SCIPgetOrigObjoffset(scip));
         // set objective sense
         builder.setObjSense(SCIPgetObjsense(scip) == SCIP_OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = SCIPgetNVars(scip);
         int nrows = SCIPgetNConss(scip);
         int nnz = 0;
         SCIP_VAR **probvars = SCIPgetVars(scip);
         SCIP_CONS **probconss = SCIPgetConss(scip);
         for( int i = 0; i < nrows; ++i )
         {
            int nconsvars = 0;
            SCIP_Bool success = FALSE;
            SCIPgetConsNVars(scip, probconss[ i ], &nconsvars, &success);
            if( !success )
               return { settings, boost::none };
            nnz += nconsvars;
         }
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int i = 0; i < ncols; ++i )
         {
            SCIP_VAR *var = probvars[ i ];
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

         // set up rows
         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Real> consvals(ncols);
         Vec<int> indices(ncols);
         builder.setNumRows(nrows);
         for( int i = 0; i < nrows; ++i )
         {
            int nconsvars = 0;
            SCIP_Bool success = FALSE;
            SCIP_CONS *cons = probconss[ i ];
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
               assert(strcmp(SCIPvarGetName(consvars[ j ]), SCIPvarGetName(probvars[ indices[ j ] ])) == 0);
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

      bool
      writeInstance(const String& filename, const bool& writesettings) override {
         if( writesettings || limits.size() >= 1 )
            SCIPwriteParams(scip, (filename + ".set").c_str(), FALSE, TRUE);
         return SCIPwriteOrigProblem(scip, (filename + ".cip").c_str(), nullptr, FALSE) == SCIP_OKAY;
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
      setup(SolverSettings& settings, const Problem<double>& problem, const Solution<double>& solution) {

         this->adjustment = &settings;
         this->model = &problem;
         this->reference = &solution;
         bool solution_exists = this->reference->status == SolutionStatus::kFeasible;
         int ncols = this->model->getNCols( );
         int nrows = this->model->getNRows( );
         const Vec<String> &varNames = this->model->getVariableNames( );
         const Vec<String> &consNames = this->model->getConstraintNames( );
         const VariableDomains<double> &domains = this->model->getVariableDomains( );
         const Objective<double> &obj = this->model->getObjective( );
         const auto &consMatrix = this->model->getConstraintMatrix( );
         const auto &lhs_values = consMatrix.getLeftHandSides( );
         const auto &rhs_values = consMatrix.getRightHandSides( );
         const auto &rflags = this->model->getRowFlags( );

         set_parameters( );
         SCIP_CALL(SCIPcreateProbBasic(scip, this->model->getName( ).c_str( )));
         SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));
         SCIP_CALL(SCIPsetObjsense(scip, obj.sense ? SCIP_OBJSENSE_MINIMIZE : SCIP_OBJSENSE_MAXIMIZE));
         vars.resize(this->model->getNCols( ));
         if( solution_exists )
            this->value = obj.offset;
         else if( this->reference->status == SolutionStatus::kUnbounded )
            this->value = obj.sense ? -SCIPinfinity(scip) : SCIPinfinity(scip);
         else if( this->reference->status == SolutionStatus::kInfeasible )
            this->value = obj.sense ? SCIPinfinity(scip) : -SCIPinfinity(scip);

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
                  this->value += obj.coefficients[ col ] * this->reference->primal[ col ];
               SCIP_CALL(SCIPaddVar(scip, var));
               vars[ col ] = var;
               SCIP_CALL(SCIPreleaseVar(scip, &var));
            }
         }

         Vec<SCIP_VAR*> consvars(this->model->getNCols( ));
         Vec<SCIP_Real> consvals(this->model->getNCols( ));
         for( int row = 0; row < nrows; ++row )
         {
            if( this->model->getRowFlags( )[ row ].test(RowFlag::kRedundant) )
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
               assert(!this->model->getColFlags( )[ inds[ k ] ].test(ColFlag::kFixed));
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

         if( solution_exists && ( parameters.set_dual_limit || parameters.set_prim_limit ) )
         {
            for( const auto& pair : limits )
            {
               switch( pair.second )
               {
               case DUAL:
                  SCIP_CALL(SCIPsetRealParam(scip, pair.first.c_str(), this->relax( this->value, obj.sense, 2.0 * SCIPsumepsilon(scip), SCIPinfinity(scip) )));
                  break;
               case PRIM:
                  SCIP_CALL(SCIPsetRealParam(scip, pair.first.c_str(), this->value));
                  break;
               }
            }
         }

         return SCIP_OKAY;
      }

      void
      set_parameters( ) const {
         for( const auto& pair : this->adjustment->getBoolSettings( ) )
            SCIPsetBoolParam(scip, pair.first.c_str(), pair.second);
         for( const auto& pair : this->adjustment->getIntSettings( ) )
            SCIPsetIntParam(scip, pair.first.c_str(), pair.second);
         for( const auto& pair : this->adjustment->getLongSettings( ) )
            SCIPsetLongintParam(scip, pair.first.c_str(), pair.second);
         for( const auto& pair : this->adjustment->getDoubleSettings( ) )
            SCIPsetRealParam(scip, pair.first.c_str(), pair.second);
         for( const auto& pair : this->adjustment->getCharSettings( ) )
            SCIPsetCharParam(scip, pair.first.c_str(), pair.second);
         for( const auto& pair : this->adjustment->getStringSettings( ) )
            SCIPsetStringParam(scip, pair.first.c_str(), pair.second.c_str());
         for( const auto& pair : this->adjustment->getLimitSettings( ) )
         {
            switch( limits.find(pair.first)->second )
            {
            case BEST:
            case SOLU:
            case REST:
               SCIPsetIntParam(scip, pair.first.c_str(), pair.second);
               break;
            case TOTA:
               SCIPsetLongintParam(scip, pair.first.c_str(), pair.second);
               break;
            case TIME:
               SCIPsetRealParam(scip, pair.first.c_str(), pair.second);
               break;
            case DUAL:
            case PRIM:
            default:
               SCIPerrorMessage("unknown limit type\n");
            }
         }
      }
   };

   template <typename REAL>
   class ScipFactory : public SolverFactory<REAL> {

   private:

      ScipParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
         parameterset.addParameter("scip.mode", "solve scip mode (-1: optimize, 0: count)", parameters.mode, -1, 0);
         parameterset.addParameter("scip.limitspace", "relative margin when restricting limits or -1 for no restriction", parameters.limitspace, -1.0);
         parameterset.addParameter("scip.setduallimit", "terminate when dual bound is better than reference solution", parameters.set_dual_limit);
         parameterset.addParameter("scip.setprimlimit", "terminate when prim bound is as good as reference solution", parameters.set_prim_limit);
         parameterset.addParameter("scip.setbestlimit", "restrict best number of solutions automatically", parameters.set_best_limit);
         parameterset.addParameter("scip.setsolulimit", "restrict total number of solutions automatically", parameters.set_solu_limit);
         parameterset.addParameter("scip.setrestlimit", "restrict number of restarts automatically", parameters.set_rest_limit);
         parameterset.addParameter("scip.settotalimit", "restrict total number of nodes automatically", parameters.set_tota_limit);
         parameterset.addParameter("scip.settimelimit", "restrict time automatically (unreproducible)", parameters.set_time_limit);
         // run and stalling number of nodes, memory, and gap are unrestrictable because they are not monotonously increasing
      }

      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) override
      {
         auto scip = std::unique_ptr<SolverInterface<REAL>>( new ScipInterface<REAL>( msg, parameters, limits ) );
         if( initial )
         {
            String name;
            if( parameters.mode != -1 )
            {
               parameters.set_dual_limit = false;
               parameters.set_prim_limit = false;
               parameters.set_best_limit = false;
               parameters.set_solu_limit = false;
               parameters.set_rest_limit = false;
            }
            else
            {
               if( parameters.set_dual_limit )
               {
                  if( scip->has_setting(name = "limits/dual") || scip->has_setting(name = "limits/proofstop") )
                     limits[name] = ScipInterface<REAL>::DUAL;
                  else
                  {
                     msg.info("Dual limit disabled.\n");
                     parameters.set_dual_limit = false;
                  }
               }
               if( parameters.set_prim_limit )
               {
                  if( scip->has_setting(name = "limits/primal") || scip->has_setting(name = "limits/objectivestop") )
                     limits[name] = ScipInterface<REAL>::PRIM;
                  else
                  {
                     msg.info("Primal limit disabled.\n");
                     parameters.set_prim_limit = false;
                  }
               }
               if( parameters.limitspace < 0.0 )
               {
                  parameters.set_best_limit = false;
                  parameters.set_solu_limit = false;
                  parameters.set_rest_limit = false;
               }
               else
               {
                  if( parameters.set_best_limit )
                  {
                     if( scip->has_setting(name = "limits/bestsol") )
                        limits[name] = ScipInterface<REAL>::BEST;
                     else
                     {
                        msg.info("Bestsolution limit disabled.\n");
                        parameters.set_best_limit = false;
                     }
                  }
                  if( parameters.set_solu_limit )
                  {
                     if( scip->has_setting(name = "limits/solutions") )
                        limits[name] = ScipInterface<REAL>::SOLU;
                     else
                     {
                        msg.info("Solution limit disabled.\n");
                        parameters.set_solu_limit = false;
                     }
                  }
                  if( parameters.set_rest_limit )
                  {
                     if( scip->has_setting(name = "limits/restarts") )
                        limits[name] = ScipInterface<REAL>::REST;
                     else
                     {
                        msg.info("Restart limit disabled.\n");
                        parameters.set_rest_limit = false;
                     }
                  }
               }
            }
            if( parameters.limitspace < 0.0 )
            {
               parameters.set_tota_limit = false;
               parameters.set_time_limit = false;
            }
            else
            {
               if( parameters.set_tota_limit )
               {
                  if( scip->has_setting(name = "limits/totalnodes") )
                     limits[name] = ScipInterface<REAL>::TOTA;
                  else
                  {
                     msg.info("Totalnode limit disabled.\n");
                     parameters.set_tota_limit = false;
                  }
               }
               if( parameters.set_time_limit )
               {
                  if( scip->has_setting(name = "limits/time") )
                     limits[name] = ScipInterface<REAL>::TIME;
                  else
                  {
                     msg.info("Time limit disabled.\n");
                     parameters.set_time_limit = false;
                  }
               }
            }
            initial = false;
         }
         return scip;
      }
   };

} // namespace bugger

#endif
