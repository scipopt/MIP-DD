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

#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


namespace bugger
{
   enum ScipLimit : char
   {
      DUAL = 1,
      PRIM = 2,
      BEST = 3,
      SOLU = 4,
      REST = 5,
      TOTA = 6,
      TIME = 7
   };

   class ScipParameters
   {
   public:

      static const String VERB;

      int arithmetic = 0;
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

   const String ScipParameters::VERB { "display/verblevel" };

   template <typename REAL>
   class ScipInterface : public SolverInterface<REAL>
   {
   private:

      const ScipParameters& parameters;
      const HashMap<String, char>& limits;
      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars { };

   public:

      explicit ScipInterface(const Message& _msg, const ScipParameters& _parameters,
                             const HashMap<String, char>& _limits) : SolverInterface<REAL>(_msg),
                             parameters(_parameters), limits(_limits)
      {
         if( SCIPcreate(&scip) != SCIP_OKAY || SCIPincludeDefaultPlugins(scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      void
      print_header( ) const override
      {
         SCIPprintVersion(scip, nullptr);

         auto names = SCIPgetExternalCodeNames(scip);
         auto description = SCIPgetExternalCodeDescriptions(scip);
         int length = SCIPgetNExternalCodes(scip);

         for( int i = 0; i < length; ++i )
            this->msg.info("\t{:20} {}\n", names[i], description[i]);
      }

      bool
      has_setting(const String& name) const override
      {
         return SCIPgetParam(scip, name.c_str()) != nullptr;
      }

      boost::optional<SolverSettings>
      parseSettings(const String& filename) const override
      {
         bool success = filename.empty() || SCIPreadParams(scip, filename.c_str()) == SCIP_OKAY;

         if( !success )
            return boost::none;

         Vec<std::pair<String, bool>> bool_settings;
         Vec<std::pair<String, int>> int_settings;
         Vec<std::pair<String, long>> long_settings;
         Vec<std::pair<String, double>> double_settings;
         Vec<std::pair<String, char>> char_settings;
         Vec<std::pair<String, String>> string_settings;
         Vec<std::pair<String, long long>> limit_settings;
         SCIP_PARAM** params = SCIPgetParams(scip);
         int nparams = SCIPgetNParams(scip);

         for( int i = 0; i < nparams; ++i )
         {
            SCIP_PARAM* param = params[ i ];
            String name { param->name };
            if( name == ScipParameters::VERB )
               continue;
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
                  limit_settings.emplace_back( name, min(ceil(param->data.realparam.valueptr == nullptr
                                                              ? param->data.realparam.curvalue
                                                              : *param->data.realparam.valueptr), SCIP_Real(LLONG_MAX)) );
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
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) override
      {
         auto retcode = setup(settings, problem, solution);
         UNUSED(retcode);
         assert(retcode == SCIP_OKAY);
      }

      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) override
      {
         char retcode = SCIP_ERROR;
         SolverStatus solverstatus = SolverStatus::kUndefinedError;

         if( this->msg.getVerbosityLevel() < VerbosityLevel::kDetailed )
            SCIPsetMessagehdlrQuiet(scip, TRUE);
         else
            SCIPsetIntParam(scip, ScipParameters::VERB.c_str(), 5);

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
            retcode = SolverRetcode::OKAY;

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
                  case SolverRetcode::DUALFAIL:
                     dual = false;
                     break;
                  case SolverRetcode::PRIMALFAIL:
                     primal = false;
                     break;
                  case SolverRetcode::OBJECTIVEFAIL:
                     objective = false;
                     break;
                  }
               }

               // declare primal solution
               Vec<Solution<REAL>> solution;
               SCIP_SOL** sols = SCIPgetSols(scip);
               int nsols = SCIPgetNSols(scip);

               // check dual by reference solution objective
               if( retcode == SolverRetcode::OKAY && dual )
                  retcode = this->check_dual_bound( REAL(SCIPgetDualbound(scip)), REAL(SCIPsumepsilon(scip)), REAL(SCIPinfinity(scip)) );

               // check primal by generated solution values
               if( retcode == SolverRetcode::OKAY )
               {
                  if( nsols >= 1 )
                  {
                     solution.resize(primal ? nsols : objective ? 1 : 0);

                     for( int i = solution.size() - 1; i >= 0; --i )
                     {
                        solution[i].status = SolutionStatus::kFeasible;
                        solution[i].primal.resize(vars.size());

                        for( int col = 0; col < solution[i].primal.size(); ++col )
                           solution[i].primal[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(SCIPgetSolVal(scip, sols[i], vars[col]));
                     }

                     if( solution.size() >= 1 && SCIPhasPrimalRay(scip) )
                     {
                        solution[0].status = SolutionStatus::kUnbounded;
                        solution[0].ray.resize(vars.size());

                        for( int col = 0; col < solution[0].ray.size(); ++col )
                           solution[0].ray[col] = this->model->getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(SCIPgetPrimalRayVal(scip, vars[col]));
                     }

                     if( primal )
                        retcode = this->check_primal_solution( solution, REAL(SCIPsumepsilon(scip)), REAL(SCIPinfinity(scip)) );
                  }
                  else if( nsols != 0 && primal )
                     retcode = SolverRetcode::PRIMALFAIL;
               }

               // check objective by best solution evaluation
               if( retcode == SolverRetcode::OKAY && objective )
               {
                  // check solution objective instead of primal bound if no ray is provided
                  REAL bound = abs(SCIPgetPrimalbound(scip)) == SCIPinfinity(scip) && solution.size() >= 1 && solution[0].status == SolutionStatus::kFeasible ? SCIPgetSolOrigObj(scip, sols[0]) : SCIPgetPrimalbound(scip);

                  if( solution.size() == 0 )
                     solution.emplace_back(SolutionStatus::kInfeasible);

                  retcode = this->check_objective_value( bound, solution[0], REAL(SCIPsumepsilon(scip)), REAL(SCIPinfinity(scip)) );
               }
            }
            else
            {
               // check count by primal solution existence
               if( retcode == SolverRetcode::OKAY )
               {
                  SCIP_Bool valid;
                  long long count = SCIPgetNCountedSols(scip, &valid);

                  retcode = this->check_count_number( REAL(SCIPgetDualbound(scip)), REAL(SCIPgetPrimalbound(scip)), valid ? count : -1, REAL(SCIPinfinity(scip)) );
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
#if SCIP_APIVERSION >= 115
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
#if SCIP_APIVERSION >= 22
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
         // interpret certain passcodes as OKAY based on the user preferences
         for( int passcode: passcodes )
         {
            if( passcode == retcode )
            {
               retcode = SolverRetcode::OKAY;
               break;
            }
         }
         // restrict limit settings
         if( retcode != SolverRetcode::OKAY )
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
                     bound = ceil(max((1.0 + parameters.limitspace) * SCIPgetNBestSolsFound(scip) + 1.0, 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case SOLU:
                     // incremented to continue after finding the last solution
                     bound = ceil(max((1.0 + parameters.limitspace) * SCIPgetNSolsFound(scip) + 1.0, 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case REST:
                     // decremented from runs to restarts
                     bound = ceil(max((1.0 + parameters.limitspace) * (SCIPgetNRuns(scip) - 1.0), 1.0));
                     if( bound > INT_MAX )
                        continue;
                     else
                        break;
                  case TOTA:
                     // assumes last node is processed
                     bound = ceil(max((1.0 + parameters.limitspace) * SCIPgetNTotalNodes(scip), 1.0));
                     if( bound > LONG_MAX )
                        continue;
                     else
                        break;
                  case TIME:
                     // sensitive to processor speed variability
                     bound = ceil(max((1.0 + parameters.limitspace) * SCIPgetSolvingTime(scip), 1.0));
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
      getSolvingEffort( ) const override
      {
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

      std::pair<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename) override
      {
         auto settings = parseSettings(settings_filename);
         if( SCIPreadProb(scip, problem_filename.c_str(), nullptr) != SCIP_OKAY )
            return { settings, boost::none };
         ProblemBuilder<REAL> builder;

         // set problem name
         builder.setProblemName(SCIPgetProbName(scip));
         // set objective offset
         builder.setObjOffset(SCIPgetOrigObjoffset(scip));
         // set objective sense
         builder.setObjSense(SCIPgetObjsense(scip) == SCIP_OBJSENSE_MINIMIZE);

         // reserve problem memory
         int ncols = SCIPgetNVars(scip);
         int nrows = SCIPgetNConss(scip);
         int nnz = 0;
         SCIP_VAR** probvars = SCIPgetVars(scip);
         SCIP_CONS** probconss = SCIPgetConss(scip);
         for( int row = 0; row < nrows; ++row )
         {
            int nrowcols = 0;
            SCIP_Bool success = FALSE;
            SCIPgetConsNVars(scip, probconss[row], &nrowcols, &success);
            if( !success )
               return { settings, boost::none };
            nnz += nrowcols;
         }
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int col = 0; col < ncols; ++col )
         {
            SCIP_VAR* var = probvars[col];
            SCIP_Real lb = SCIPvarGetLbGlobal(var);
            SCIP_Real ub = SCIPvarGetUbGlobal(var);
            SCIP_VARTYPE vartype = SCIPvarGetType(var);
            builder.setColLb(col, REAL(lb));
            builder.setColUb(col, REAL(ub));
            builder.setColLbInf(col, SCIPisInfinity(scip, -lb));
            builder.setColUbInf(col, SCIPisInfinity(scip, ub));
            builder.setColIntegral(col, vartype == SCIP_VARTYPE_BINARY || vartype == SCIP_VARTYPE_INTEGER);
            builder.setColImplInt(col, vartype != SCIP_VARTYPE_CONTINUOUS);
            builder.setObj(col, REAL(SCIPvarGetObj(var)));
            builder.setColName(col, SCIPvarGetName(var));
         }

         // set up rows
         builder.setNumRows(nrows);
         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Real> consvals(ncols);
         Vec<int> rowinds(ncols);
         Vec<REAL> rowvals(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            SCIP_CONS* cons = probconss[row];
            SCIP_Bool success = FALSE;
            SCIP_Real lhs = SCIPconsGetLhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none };
            SCIP_Real rhs = SCIPconsGetRhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none };
            int nrowcols = 0;
            SCIPgetConsNVars(scip, cons, &nrowcols, &success);
            if( !success )
               return { settings, boost::none };
            SCIPgetConsVars(scip, cons, consvars.data(), ncols, &success);
            if( !success )
               return { settings, boost::none };
            SCIPgetConsVals(scip, cons, consvals.data(), ncols, &success);
            if( !success )
               return { settings, boost::none };
            for( int i = 0; i < nrowcols; ++i )
            {
               rowinds[i] = SCIPvarGetProbindex(consvars[i]);
               rowvals[i] = REAL(consvals[i]);
            }
            builder.setRowLhs(row, REAL(lhs));
            builder.setRowRhs(row, REAL(rhs));
            builder.setRowLhsInf(row, SCIPisInfinity(scip, -lhs));
            builder.setRowRhsInf(row, SCIPisInfinity(scip, rhs));
            builder.addRowEntries(row, nrowcols, rowinds.data(), rowvals.data());
            builder.setRowName(row, SCIPconsGetName(cons));
         }

         return { settings, builder.build() };
      }

      bool
      writeInstance(const String& filename, const bool& writesettings) const override
      {
         if( writesettings || limits.size() >= 1 )
            SCIPwriteParams(scip, (filename + ".set").c_str(), FALSE, TRUE);
         return SCIPwriteOrigProblem(scip, (filename + ".cip").c_str(), nullptr, FALSE) == SCIP_OKAY;
      }

      ~ScipInterface( ) override
      {
         if( scip != nullptr )
         {
            auto retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }

   private:

      SCIP_RETCODE
      setup(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution)
      {
         this->adjustment = &settings;
         this->model = &problem;
         this->reference = &solution;
         bool solution_exists = this->reference->status == SolutionStatus::kFeasible;
         int ncols = this->model->getNCols( );
         int nrows = this->model->getNRows( );
         const auto& varNames = this->model->getVariableNames( );
         const auto& consNames = this->model->getConstraintNames( );
         const auto& domains = this->model->getVariableDomains( );
         const auto& obj = this->model->getObjective( );
         const auto& consMatrix = this->model->getConstraintMatrix( );
         const auto& lhs_values = consMatrix.getLeftHandSides( );
         const auto& rhs_values = consMatrix.getRightHandSides( );
         const auto& rflags = this->model->getRowFlags( );

         set_parameters( );
         SCIP_CALL(SCIPcreateProbBasic(scip, this->model->getName( ).c_str()));
         SCIP_CALL(SCIPaddOrigObjoffset(scip, SCIP_Real(obj.offset)));
         SCIP_CALL(SCIPsetObjsense(scip, obj.sense ? SCIP_OBJSENSE_MINIMIZE : SCIP_OBJSENSE_MAXIMIZE));
         vars.resize(ncols);
         if( solution_exists )
            this->value = this->get_primal_objective(solution);
         else if( this->reference->status == SolutionStatus::kUnbounded )
            this->value = obj.sense ? -SCIPinfinity(scip) : SCIPinfinity(scip);
         else if( this->reference->status == SolutionStatus::kInfeasible )
            this->value = obj.sense ? SCIPinfinity(scip) : -SCIPinfinity(scip);

         for( int col = 0; col < ncols; ++col )
         {
            if( domains.flags[col].test(ColFlag::kFixed) )
               vars[col] = nullptr;
            else
            {
               SCIP_VAR* var;
               SCIP_Real lb = domains.flags[col].test(ColFlag::kLbInf)
                              ? -SCIPinfinity(scip)
                              : SCIP_Real(domains.lower_bounds[col]);
               SCIP_Real ub = domains.flags[col].test(ColFlag::kUbInf)
                              ? SCIPinfinity(scip)
                              : SCIP_Real(domains.upper_bounds[col]);
               assert(!domains.flags[col].test(ColFlag::kInactive) || lb == ub);
               SCIP_VARTYPE type;
               if( domains.flags[col].test(ColFlag::kIntegral) )
               {
                  if( lb >= 0 && ub <= 1 )
                     type = SCIP_VARTYPE_BINARY;
                  else
                     type = SCIP_VARTYPE_INTEGER;
               }
               else if( domains.flags[col].test(ColFlag::kImplInt) )
                  type = SCIP_VARTYPE_IMPLINT;
               else
                  type = SCIP_VARTYPE_CONTINUOUS;
               SCIP_CALL(SCIPcreateVarBasic(scip, &var, varNames[col].c_str(), lb, ub,
                                            SCIP_Real(obj.coefficients[col]), type));
               vars[col] = var;
               SCIP_CALL(SCIPaddVar(scip, var));
               SCIP_CALL(SCIPreleaseVar(scip, &var));
            }
         }

         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Real> consvals(ncols);
         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[row].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[row].test(RowFlag::kLhsInf) || !rflags[row].test(RowFlag::kRhsInf));
            const auto& rowvec = consMatrix.getRowCoefficients(row);
            const auto& rowinds = rowvec.getIndices( );
            const auto& rowvals = rowvec.getValues( );
            int nrowcols = rowvec.getLength( );
            SCIP_Real lhs = rflags[row].test(RowFlag::kLhsInf)
                            ? -SCIPinfinity(scip)
                            : SCIP_Real(lhs_values[row]);
            SCIP_Real rhs = rflags[row].test(RowFlag::kRhsInf)
                            ? SCIPinfinity(scip)
                            : SCIP_Real(rhs_values[row]);
            SCIP_CONS* cons;
            for( int i = 0; i < nrowcols; ++i )
            {
               assert(!this->model->getColFlags( )[rowinds[i]].test(ColFlag::kFixed));
               assert(rowvals[i] != 0);
               consvars[i] = vars[rowinds[i]];
               consvals[i] = SCIP_Real(rowvals[i]);
            }
            SCIP_CALL(SCIPcreateConsBasicLinear(scip, &cons, consNames[row].c_str(), nrowcols, consvars.data( ),
                                                consvals.data( ), lhs, rhs));
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
                  SCIP_CALL(SCIPsetRealParam(scip, pair.first.c_str(), SCIP_Real(this->relax( this->value, obj.sense, 2 * SCIPsumepsilon(scip), SCIPinfinity(scip) ))));
                  break;
               case PRIM:
                  SCIP_CALL(SCIPsetRealParam(scip, pair.first.c_str(), SCIP_Real(this->value)));
                  break;
               }
            }
         }

         return SCIP_OKAY;
      }

      void
      set_parameters( )
      {
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
   class ScipFactory : public SolverFactory<REAL>
   {
   private:

      ScipParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
         parameterset.addParameter("scip.arithmetic", "arithmetic scip type (0: double)", parameters.arithmetic, 0, 0);
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
         std::unique_ptr<SolverInterface<REAL>> scip;
         switch( parameters.arithmetic )
         {
         case 0:
            scip = std::unique_ptr<SolverInterface<REAL>>( new ScipInterface<REAL>( msg, parameters, limits ) );
            break;
         default:
            msg.error("unknown solver arithmetic\n");
            return nullptr;
         }
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
                     limits[name] = DUAL;
                  else
                  {
                     msg.info("Dual limit disabled.\n");
                     parameters.set_dual_limit = false;
                  }
               }
               if( parameters.set_prim_limit )
               {
                  if( scip->has_setting(name = "limits/primal") || scip->has_setting(name = "limits/objectivestop") )
                     limits[name] = PRIM;
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
                        limits[name] = BEST;
                     else
                     {
                        msg.info("Bestsolution limit disabled.\n");
                        parameters.set_best_limit = false;
                     }
                  }
                  if( parameters.set_solu_limit )
                  {
                     if( scip->has_setting(name = "limits/solutions") )
                        limits[name] = SOLU;
                     else
                     {
                        msg.info("Solution limit disabled.\n");
                        parameters.set_solu_limit = false;
                     }
                  }
                  if( parameters.set_rest_limit )
                  {
                     if( scip->has_setting(name = "limits/restarts") )
                        limits[name] = REST;
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
                     limits[name] = TOTA;
                  else
                  {
                     msg.info("Totalnode limit disabled.\n");
                     parameters.set_tota_limit = false;
                  }
               }
               if( parameters.set_time_limit )
               {
                  if( scip->has_setting(name = "limits/time") )
                     limits[name] = TIME;
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

   template <typename REAL>
   std::shared_ptr<SolverFactory<REAL>>
   load_solver_factory( )
   {
      return std::shared_ptr<SolverFactory<REAL>>( new ScipFactory<REAL>( ) );
   }

} // namespace bugger

#endif
