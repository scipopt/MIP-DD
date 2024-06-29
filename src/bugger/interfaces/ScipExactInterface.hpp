/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef __BUGGER_INTERFACES_SCIPEXACTINTERFACE_HPP__
#define __BUGGER_INTERFACES_SCIPEXACTINTERFACE_HPP__

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "boost/detail/atomic_count.hpp"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "scip/certificate.h"
#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "bugger/data/SolverSettings.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"


namespace bugger
{
   template <typename T>
   class Countable
   {
      static boost::detail::atomic_count cs_count_;

   protected:

      ~Countable( )
      {
         --cs_count_;
      }

   public:

      Countable( )
      {
         ++cs_count_;
      }

      Countable(Countable const&)
      {
         ++cs_count_;
      }

      static unsigned
      count( )
      {
         return cs_count_;
      }
   };

   template <typename T>
   boost::detail::atomic_count Countable<T>::cs_count_(0);

   enum ScipExactLimit : char
   {
      DUAL = 1,
      PRIM = 2,
      BEST = 3,
      SOLU = 4,
      REST = 5,
      TOTA = 6,
      TIME = 7
   };

   class ScipExactParameters
   {
   public:

      static const String VERB;
      static const String EXAC;
      static const String CERT;

      int arithmetic = 0;
      int mode = -1;
      bool certificate = false;
      double limitspace = 1.0;
      bool set_dual_limit = true;
      bool set_prim_limit = true;
      bool set_best_limit = true;
      bool set_solu_limit = true;
      bool set_rest_limit = true;
      bool set_tota_limit = true;
      bool set_time_limit = false;
   };

   const String ScipExactParameters::VERB { "display/verblevel" };
   const String ScipExactParameters::EXAC { "exact/enabled" };
   const String ScipExactParameters::CERT { "certificate/filename" };

   template <typename REAL>
   class ScipExactInterface : public SolverInterface<REAL>, public Countable<ScipExactInterface<REAL>>
   {
   private:

      const ScipExactParameters& parameters;
      const HashMap<String, char>& limits;
      SCIP* scip = nullptr;
      Vec<SCIP_VAR*> vars { };

   public:

      explicit ScipExactInterface(const Message& _msg, const ScipExactParameters& _parameters,
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
            if( name == ScipExactParameters::VERB
             || name == ScipExactParameters::EXAC
             || name == ScipExactParameters::CERT )
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
            SCIPsetIntParam(scip, ScipExactParameters::VERB.c_str(), 5);

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
                        translateSolution(sols[i], i == 0 && SCIPhasPrimalRay(scip), *this->model, solution[i]);

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
            case SCIP_STATUS_USERINTERRUPT:
               solverstatus = SolverStatus::kInterrupt;
               break;
#if SCIP_APIVERSION >= 22
            case SCIP_STATUS_TERMINATE:
               solverstatus = SolverStatus::kTerminate;
               break;
#endif
            case SCIP_STATUS_NODELIMIT:
               solverstatus = SolverStatus::kNodeLimit;
               break;
            case SCIP_STATUS_TOTALNODELIMIT:
               solverstatus = SolverStatus::kTotalNodeLimit;
               break;
            case SCIP_STATUS_STALLNODELIMIT:
               solverstatus = SolverStatus::kStallNodeLimit;
               break;
            case SCIP_STATUS_TIMELIMIT:
               solverstatus = SolverStatus::kTimeLimit;
               break;
            case SCIP_STATUS_MEMLIMIT:
               solverstatus = SolverStatus::kMemLimit;
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
            case SCIP_STATUS_SOLLIMIT:
               solverstatus = SolverStatus::kSolLimit;
               break;
            case SCIP_STATUS_BESTSOLLIMIT:
               solverstatus = SolverStatus::kBestSolLimit;
               break;
            case SCIP_STATUS_RESTARTLIMIT:
               solverstatus = SolverStatus::kRestartLimit;
               break;
            case SCIP_STATUS_OPTIMAL:
               solverstatus = SolverStatus::kOptimal;
               break;
            case SCIP_STATUS_INFEASIBLE:
               solverstatus = SolverStatus::kInfeasible;
               break;
            case SCIP_STATUS_UNBOUNDED:
               solverstatus = SolverStatus::kUnbounded;
               break;
            case SCIP_STATUS_INFORUNBD:
               solverstatus = SolverStatus::kInfeasibleOrUnbounded;
               break;
            }

            // only do certificate stuff if problem was not solved in presolving
            if( retcode == SolverRetcode::OKAY && parameters.certificate && SCIPgetNNodes(scip) > 0 )
            {
               SCIPfreeTransform(scip);

               if( system(("viprcomp bugger" + std::to_string(this->count()) + " >/dev/null").c_str()) )
                  retcode = SolverRetcode::COMPLETIONFAIL;
               else if( system(("viprchk bugger" + std::to_string(this->count()) + "_complete.vipr >/dev/null").c_str()) )
                  retcode = SolverRetcode::CERTIFICATIONFAIL;
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
            switch( SCIPgetStage(scip) )
            {
            case SCIP_STAGE_TRANSFORMING:
            case SCIP_STAGE_TRANSFORMED:
            case SCIP_STAGE_INITPRESOLVE:
            case SCIP_STAGE_PRESOLVING:
            case SCIP_STAGE_EXITPRESOLVE:
            case SCIP_STAGE_PRESOLVED:
            case SCIP_STAGE_INITSOLVE:
            case SCIP_STAGE_SOLVING:
            case SCIP_STAGE_SOLVED:
            case SCIP_STAGE_EXITSOLVE:
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
               break;
            case SCIP_STAGE_INIT:
            case SCIP_STAGE_PROBLEM:
            case SCIP_STAGE_FREETRANS:
            case SCIP_STAGE_FREE:
            default:
               break;
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

      std::tuple<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>, boost::optional<Solution<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename, const String& solution_filename) override
      {
         auto parsed_settings = parseSettings(settings_filename);
         if( !parsed_settings )
            return { boost::none, boost::none, boost::none };
         SolverSettings settings { parsed_settings.get() };
         if( SCIPreadProb(scip, problem_filename.c_str(), nullptr) != SCIP_OKAY )
            return { settings, boost::none, boost::none };
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
         vars.clear();
         vars.insert(vars.end(), SCIPgetVars(scip), SCIPgetVars(scip) + ncols);
         SCIP_CONS** conss = SCIPgetConss(scip);
         for( int row = 0; row < nrows; ++row )
         {
            int nrowcols = 0;
            SCIP_Bool success = FALSE;
            SCIPgetConsNVars(scip, conss[row], &nrowcols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            nnz += nrowcols;
         }
         builder.reserve(nnz, nrows, ncols);

         // set up columns
         builder.setNumCols(ncols);
         for( int col = 0; col < ncols; ++col )
         {
            SCIP_VAR* var = vars[col];
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
            SCIP_CONS* cons = conss[row];
            SCIP_Bool success = FALSE;
            SCIP_Real lhs = SCIPconsGetLhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            SCIP_Real rhs = SCIPconsGetRhs(scip, cons, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            int nrowcols = 0;
            SCIPgetConsNVars(scip, cons, &nrowcols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            SCIPgetConsVars(scip, cons, consvars.data(), ncols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
            SCIPgetConsVals(scip, cons, consvals.data(), ncols, &success);
            if( !success )
               return { settings, boost::none, boost::none };
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

         Problem<REAL> problem { builder.build() };
         Solution<REAL> solution { };
         if( !solution_filename.empty() )
         {
            SCIP_SOL* sol = nullptr;
            SCIP_Bool error = TRUE;
            bool success = true;
            if( success && SCIPcreateSol(scip, &sol, nullptr) != SCIP_OKAY )
            {
               sol = nullptr;
               success = false;
            }
            if( success && ( SCIPreadSolFile(scip, solution_filename.c_str(), sol, FALSE, NULL, &error) != SCIP_OKAY || error ) )
               success = false;
            if( success )
               translateSolution(sol, FALSE, problem, solution);
            if( sol != nullptr )
               SCIPfreeSol(scip, &sol);
            if( !success )
               return { settings, problem, boost::none };
         }

         return { settings, problem, solution };
      }

      std::tuple<bool, bool, bool>
      writeInstance(const String& filename, const bool& writesettings, const bool& writesolution) const override
      {
         bool successsettings = ( !writesettings && limits.size() == 0 ) || SCIPwriteParams(scip, (filename + ".set").c_str(), FALSE, TRUE) == SCIP_OKAY;
         bool successproblem = SCIPwriteOrigProblem(scip, (filename + ".cip").c_str(), nullptr, FALSE) == SCIP_OKAY;
         bool successsolution = true;

         if( writesolution && this->reference->status == SolutionStatus::kFeasible )
         {
            SCIP_SOL* sol = nullptr;
            FILE* file = nullptr;
            if( successsolution && SCIPcreateSol(scip, &sol, nullptr) != SCIP_OKAY )
            {
               sol = nullptr;
               successsolution = false;
            }
            for( int col = 0; successsolution && col < this->reference->primal.size(); ++col )
            {
               if( !this->model->getColFlags()[col].test( ColFlag::kFixed ) && SCIPsetSolVal(scip, sol, vars[col], SCIP_Real(this->reference->primal[col])) != SCIP_OKAY )
                  successsolution = false;
            }
            if( successsolution && ( (file = fopen((filename + ".sol").c_str(), "w")) == nullptr || SCIPprintSol(scip, sol, file, FALSE) != SCIP_OKAY ) )
               successsolution = false;
            if( file != nullptr )
               fclose(file);
            if( sol != nullptr )
               SCIPfreeSol(scip, &sol);
         }

         return { successsettings, successproblem, successsolution };
      }

      ~ScipExactInterface( ) override
      {
         if( scip != nullptr )
         {
            switch( SCIPgetStage(scip) )
            {
            case SCIP_STAGE_INITSOLVE:
            case SCIP_STAGE_EXITSOLVE:
            case SCIP_STAGE_FREETRANS:
            case SCIP_STAGE_FREE:
               // avoid errors in stages above
               scip = nullptr;
            case SCIP_STAGE_INIT:
            case SCIP_STAGE_PROBLEM:
            case SCIP_STAGE_TRANSFORMING:
            case SCIP_STAGE_TRANSFORMED:
            case SCIP_STAGE_INITPRESOLVE:
            case SCIP_STAGE_PRESOLVING:
            case SCIP_STAGE_PRESOLVED:
            case SCIP_STAGE_EXITPRESOLVE:
            case SCIP_STAGE_SOLVING:
            case SCIP_STAGE_SOLVED:
            default:
               if( scip == nullptr || SCIPfree(&scip) != SCIP_OKAY )
                  this->msg.warn("could not free SCIP\n");
               break;
            }
         }
      }

   private:

      void
      translateSolution(SCIP_SOL* const sol, const SCIP_Bool ray, const Problem<REAL>& problem, Solution<REAL>& solution) const
      {
         solution.status = ray ? SolutionStatus::kUnbounded : SolutionStatus::kFeasible;
         solution.primal.resize(problem.getNCols());
         for( int col = 0; col < solution.primal.size(); ++col )
            solution.primal[col] = problem.getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(SCIPgetSolVal(scip, sol, vars[col]));
         if( ray )
         {
            solution.ray.resize(problem.getNCols());
            for( int col = 0; col < solution.ray.size(); ++col )
               solution.ray[col] = problem.getColFlags()[col].test( ColFlag::kFixed ) ? std::numeric_limits<REAL>::signaling_NaN() : REAL(SCIPgetPrimalRayVal(scip, vars[col]));
         }
      }

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
            this->value = this->model->getPrimalObjective(solution);
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
               SCIP_CALL(SCIPaddVarExactData(scip, var, NULL, NULL, NULL));
               vars[col] = var;
               SCIP_CALL(SCIPaddVar(scip, var));
               SCIP_CALL(SCIPreleaseVar(scip, &var));
            }
         }

         Vec<SCIP_VAR*> consvars(ncols);
         Vec<SCIP_Rational*> consvals(ncols);
         SCIP_Rational* lhs;
         SCIP_Rational* rhs;
         for( int col = 0; col < ncols; ++col )
            RatCreate(&consvals[col]);
         RatCreate(&lhs);
         RatCreate(&rhs);
         for( int row = 0; row < nrows; ++row )
         {
            if( rflags[row].test(RowFlag::kRedundant) )
               continue;
            assert(!rflags[row].test(RowFlag::kLhsInf) || !rflags[row].test(RowFlag::kRhsInf));
            const auto& rowvec = consMatrix.getRowCoefficients(row);
            const auto& rowinds = rowvec.getIndices( );
            const auto& rowvals = rowvec.getValues( );
            int nrowcols = rowvec.getLength( );
            SCIP_CONS* cons;
            RatSetReal(lhs, rflags[ row ].test(RowFlag::kLhsInf) ? -SCIPinfinity(scip) : SCIP_Real(lhs_values[ row ]));
            RatSetReal(rhs, rflags[ row ].test(RowFlag::kRhsInf) ? SCIPinfinity(scip) : SCIP_Real(rhs_values[ row ]));
            for( int i = 0; i < nrowcols; ++i )
            {
               assert(!this->model->getColFlags( )[rowinds[i]].test(ColFlag::kFixed));
               assert(rowvals[i] != 0);
               consvars[i] = vars[rowinds[i]];
               RatSetReal(consvals[i], SCIP_Real(rowvals[i]));
            }
            SCIP_CALL(SCIPcreateConsBasicExactLinear(scip, &cons, consNames[row].c_str(), nrowcols, consvars.data( ),
                                                     consvals.data( ), lhs, rhs));
            SCIP_CALL(SCIPaddCons(scip, cons));
            SCIP_CALL(SCIPreleaseCons(scip, &cons));
         }
         RatFree(&rhs);
         RatFree(&lhs);
         for( int col = ncols - 1; col >= 0; --col )
            RatFree(&consvals[col]);

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
      set_parameters( ) const
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
         switch( parameters.arithmetic )
         {
         case 0:
            SCIPsetBoolParam(scip, ScipExactParameters::EXAC.c_str(), FALSE);
            break;
         case 1:
            SCIPsetBoolParam(scip, ScipExactParameters::EXAC.c_str(), TRUE);
            break;
         default:
            SCIPerrorMessage("unknown solver arithmetic\n");
         }
         SCIPsetStringParam(scip, ScipExactParameters::CERT.c_str(), parameters.certificate ? ("bugger" + std::to_string(this->count())).c_str() : "");
      }
   };

   template <typename REAL>
   class ScipExactFactory : public SolverFactory<REAL>
   {
   private:

      ScipExactParameters parameters { };
      HashMap<String, char> limits { };
      bool initial = true;

   public:

      void
      addParameters(ParameterSet& parameterset) override
      {
         parameterset.addParameter("scip.arithmetic", "arithmetic scip type (0: double, 1: rational)", parameters.arithmetic, 0, 1);
         parameterset.addParameter("scip.mode", "solve scip mode (-1: optimize, 0: count)", parameters.mode, -1, 0);
         parameterset.addParameter("scip.certificate", "check vipr certificate", parameters.certificate);
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
            scip = std::unique_ptr<SolverInterface<REAL>>( new ScipExactInterface<REAL>( msg, parameters, limits ) );
            break;
         default:
            throw std::runtime_error("unknown solver arithmetic");
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
      return std::shared_ptr<SolverFactory<REAL>>( new ScipExactFactory<REAL>( ) );
   }

} // namespace bugger

#endif
