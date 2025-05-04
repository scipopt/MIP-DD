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

#ifndef __BUGGER_DATA_BUGGERRUN_HPP__
#define __BUGGER_DATA_BUGGERRUN_HPP__

#include "bugger/io/MpsParser.hpp"
#include "bugger/io/MpsWriter.hpp"
#include "bugger/io/SolParser.hpp"
#include "bugger/io/SolWriter.hpp"
#include "bugger/misc/OptionsParser.hpp"
#include "bugger/modifiers/SettingModifier.hpp"


namespace bugger
{
   template <typename REAL>
   class BuggerRun
   {
   private:

      const Message& msg;
      const Num<REAL>& num;
      BuggerParameters& parameters;
      const std::shared_ptr<SolverFactory<REAL>>& factory;
      const Vec<std::unique_ptr<BuggerModifier<REAL>>>& modifiers;
      Vec<ModifierStatus> results;

   public:

      explicit BuggerRun(const Message& _msg, const Num<REAL>& _num, BuggerParameters& _parameters, const std::shared_ptr<SolverFactory<REAL>>& _factory, const Vec<std::unique_ptr<BuggerModifier<REAL>>>& _modifiers)
            : msg(_msg), num(_num), parameters(_parameters), factory(_factory), modifiers(_modifiers), results(_modifiers.size()) { }

      bool
      is_time_exceeded(const Timer& timer) const
      {
         return timer.getTime() >= parameters.tlim;
      }

      void
      apply(const OptionsInfo& optionsInfo, SettingModifier<REAL>* const setting)
      {
         msg.info("\nMIP Solver:\n");
         factory->create_solver(msg)->print_header();
         msg.info("\n");
         if( setting->isEnabled() )
         {
            auto target_settings = factory->create_solver(msg)->parseSettings(optionsInfo.target_settings_file);
            if( target_settings )
               setting->target_settings = target_settings.get();
            else
            {
               msg.info("Targets parser of the solver on {} failed!\n", optionsInfo.target_settings_file);
               setting->setEnabled(false);
            }
         }
         SolutionStatus status { };
         if( boost::iequals(optionsInfo.solution_file, "infeasible") )
            status = SolutionStatus::kInfeasible;
         else if( boost::iequals(optionsInfo.solution_file, "unbounded") )
            status = SolutionStatus::kUnbounded;
         else if( !optionsInfo.solution_file.empty() && !boost::iequals(optionsInfo.solution_file, "unknown") )
            status = SolutionStatus::kFeasible;
         auto instance = factory->create_solver(msg)->readInstance(optionsInfo.settings_file, optionsInfo.problem_file, status == SolutionStatus::kFeasible && !boost::iequals(optionsInfo.solution_file, "feasible") ? optionsInfo.solution_file : "");
         if( !std::get<0>(instance) )
         {
            msg.info("Settings parser of the solver on {} failed!\n", optionsInfo.settings_file);
            setting->setEnabled(false);
            std::get<0>(instance) = SolverSettings { };
         }
         auto settings = std::get<0>(instance).get();
         if( !std::get<1>(instance) )
         {
            msg.info("Problem parser of the solver failed, general parser ");
            std::get<1>(instance) = MpsParser<REAL>::readProb(optionsInfo.problem_file);
            if( std::get<1>(instance) )
               msg.info("successful.\n");
            else
            {
               msg.info("on {} failed!\n", optionsInfo.problem_file);
               return;
            }
         }
         auto problem = std::get<1>(instance).get();
         if( !std::get<2>(instance) )
         {
            msg.info("Solution parser of the solver failed, general parser ");
            std::get<2>(instance) = SolParser<REAL>::readSol(optionsInfo.solution_file, problem.getVariableNames( ));
            if( std::get<2>(instance) )
               msg.info("successful.\n");
            else
            {
               msg.info("on {} failed!\n", optionsInfo.solution_file);
               return;
            }
         }
         auto solution = std::get<2>(instance).get();
         solution.status = status;
         (void)problem.checkFeasibility(solution, num, msg);
         long long last_effort = -1;
         std::pair<char, SolverStatus> last_result = { SolverRetcode::OKAY, SolverStatus::kUnknown };
         int last_round = -1;
         int last_modifier = -1;
         auto solver = factory->create_solver(msg);
         solver->doSetUp(settings, problem, solution);
         if( parameters.mode == MODE_REDUCE )
         {
            if( parameters.expenditure < 0 )
               parameters.expenditure = 0;
         }
         else
         {
            last_result = solver->solve(Vec<int>{ });
            last_effort = solver->getSolvingEffort( );
            msg.info("Original solve returned code {} with status {} and effort {}.\n", (int)last_result.first, last_result.second, last_effort);
            if( parameters.mode == MODE_REPRODUCE )
               return;
            if( parameters.expenditure < 0 && ( parameters.nbatches <= 0 || last_effort <= 0 || (parameters.expenditure = parameters.nbatches * last_effort) / last_effort != parameters.nbatches ) )
            {
               msg.info("Batch adaption disabled.\n");
               parameters.expenditure = 0;
            }
            msg.info("\n");
         }
         bool writesetting = setting->isEnabled();
         bool writesolution = false;
         for( const auto& modifier: modifiers )
         {
            if( modifier->isEnabled() && ( modifier->getName() == "fixing" || modifier->getName() == "objective" ) )
            {
               writesolution = true;
               break;
            }
         }
         int ending = optionsInfo.problem_file.rfind('.');
         if( optionsInfo.problem_file.substr(ending + 1) == "gz" ||
             optionsInfo.problem_file.substr(ending + 1) == "bz2" )
            ending = optionsInfo.problem_file.rfind('.', ending-1);
         std::string filename = optionsInfo.problem_file.substr(0, ending) + "_";

         double time = 0.0;
         {
            Timer timer(time);
            long long minadmissible = -1;
            long long maxadmissible = -1;

            for( int round = parameters.initround, stage = parameters.initstage, success = parameters.initstage; stage < parameters.maxstages; ++round )
            {
               //TODO: Clean matrix in each round
               //TODO: Simplify solver handling
               //TODO: Free solver afterwards
               solver = factory->create_solver(msg);
               solver->doSetUp(settings, problem, solution);
               auto successwrite = solver->writeInstance(filename + std::to_string(round), writesetting, writesolution);
               if( !std::get<0>(successwrite) )
                  msg.info("Settings writer of the solver on {} failed!\n", filename + std::to_string(round) + ".set");
               if( !std::get<1>(successwrite) )
                  MpsWriter<REAL>::writeProb(filename + std::to_string(round) + ".mps", problem);
               if( !std::get<2>(successwrite) )
                  SolWriter<REAL>::writeSol(filename + std::to_string(round) + ".sol", problem, solution);

               if( round >= parameters.maxrounds || is_time_exceeded(timer) )
                  break;

               // adapt batch number
               if( parameters.expenditure > 0 && last_effort >= 0 )
                  parameters.nbatches = last_effort >= 1 ? (parameters.expenditure - 1) / last_effort + 1 : 0;

               msg.info("Round {} Stage {} Batch {}\n", round + 1, stage + 1, parameters.nbatches);

               for( int modifier = 0; modifier <= stage && stage < parameters.maxstages; ++modifier )
               {
                  if( modifiers[ modifier ]->getLastAdmissible( ) > minadmissible )
                     results[ modifier ] = modifiers[ modifier ]->run(settings, problem, solution, timer);

                  if( results[ modifier ] == ModifierStatus::kSuccessful )
                  {
                     long long effort = modifiers[ modifier ]->getLastSolvingEffort( );
                     if( effort >= 0 )
                        last_effort = effort;
                     last_result = modifiers[ modifier ]->getLastResult( );
                     last_round = round;
                     last_modifier = modifier;
                     success = modifier;
                     minadmissible = -1;
                     maxadmissible = -1;
                  }
                  else
                  {
                     long long admissible = modifiers[ modifier ]->getLastAdmissible( );
                     if( maxadmissible < admissible )
                        maxadmissible = admissible;
                     if( success == modifier )
                     {
                        modifier = stage;
                        ++stage;
                        success = stage;

                        // refine batch adaption
                        if( stage >= parameters.maxstages && parameters.nbatches > 0 && maxadmissible > parameters.nbatches )
                        {
                           minadmissible = parameters.nbatches;
                           while( parameters.expenditure > 0 )
                           {
                              if( (parameters.expenditure * 2) / 2 == parameters.expenditure )
                              {
                                 parameters.expenditure *= 2;
                                 if( parameters.expenditure > last_effort )
                                    break;
                              }
                              else
                              {
                                 msg.info("Batch adaption disabled.\n");
                                 parameters.expenditure = 0;
                              }
                           }
                           if( parameters.expenditure > 0 && last_effort >= 1 )
                              parameters.nbatches = (parameters.expenditure - 1) / last_effort + 1;
                           else if( (parameters.nbatches * 2) / 2 == parameters.nbatches )
                              parameters.nbatches *= 2;
                           else
                              parameters.nbatches = 0;
                           msg.info("Refined Batch {}\n", parameters.nbatches);
                           modifier = -1;
                           stage = parameters.initstage;
                           success = parameters.initstage;
                        }
                     }
                  }
               }
            }

            assert( is_time_exceeded(timer) || evaluateResults( ) != ModifierStatus::kSuccessful );
         }

         printStats(time, last_result, last_round, last_modifier, last_effort);
      }

   private:

      ModifierStatus
      evaluateResults( )
      {
         int largestValue = static_cast<int>( ModifierStatus::kDidNotRun );

         for( int modifier = 0; modifier < parameters.maxstages; ++modifier )
            largestValue = max(largestValue, static_cast<int>( results[ modifier ] ));

         return static_cast<ModifierStatus>( largestValue );
      }

      void
      printStats(const double& time, const std::pair<char, SolverStatus>& last_result, int last_round, int last_modifier, long long last_effort)
      {
         msg.info("\n {:>18} {:>12} {:>12} {:>18} {:>12} {:>18} \n",
                  "modifiers", "nb calls", "changes", "success calls(%)", "solves", "execution time(s)");
         int nsolves = 0;
         for( const auto& modifier: modifiers )
         {
            modifier->printStats(msg);
            nsolves += modifier->getNSolves();
         }
         if( last_round == -1 )
         {
            assert(last_modifier == -1);
            if( parameters.mode == MODE_REDUCE )
            {
               assert(last_result.first == SolverRetcode::OKAY);
               assert(last_result.second == SolverStatus::kUnknown);
               assert(last_effort == -1);
            }
            msg.info("\nNo modifications applied by the bugger!");
         }
         else
         {
            assert(last_modifier != -1);
            msg.info("\nFinal solve returned code {} with status {} and effort {} in round {} by modifier {}.", (int)last_result.first, last_result.second, last_effort, last_round + 1, modifiers[ last_modifier ]->getName( ));
         }
         msg.info( "\nbugging took {:.3f} seconds with {} solver invocations", time, nsolves );
         if( parameters.mode != MODE_REDUCE )
            msg.info(" (excluding original solve)");
         msg.info("\n");
      }
   };

}

#endif //BUGGER_BUGGERRUN_HPP
