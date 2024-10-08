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

#ifndef __BUGGER_MODIFIERS_BUGGERMODIFIER_HPP__
#define __BUGGER_MODIFIERS_BUGGERMODIFIER_HPP__

#include "bugger/data/BuggerParameters.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"

#ifdef BUGGER_TBB
#include "bugger/misc/tbb.hpp"
#else
#include <chrono>
#endif


namespace bugger
{
   enum class ModifierStatus : int
   {

      kDidNotRun = 0,

      kNotAdmissible = 1,

      kUnsuccesful = 2,

      kSuccessful = 3,

   };

   template <typename REAL>
   class BuggerModifier
   {
   private:

      String name { };
      double execTime = 0.0;
      bool enabled = true;
      unsigned int ncalls = 0;
      unsigned int nsuccessCall = 0;

   protected:

      const Message& msg;
      const Num<REAL>& num;
      const BuggerParameters& parameters;
      std::shared_ptr<SolverFactory<REAL>> factory;
      int nchgcoefs = 0;
      int nfixedvars = 0;
      int nchgsides = 0;
      int naggrvars = 0;
      int nchgsettings = 0;
      int ndeletedrows = 0;
      int nsolves = 0;
      std::pair<char, SolverStatus> last_result { SolverRetcode::OKAY, SolverStatus::kUnknown };
      long long last_effort = -1;

   public:

      BuggerModifier(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                  std::shared_ptr<SolverFactory<REAL>>& _factory) : msg(_msg), num(_num), parameters(_parameters),
                  factory(_factory) { }

      virtual ~BuggerModifier( ) = default;

      virtual bool
      initialize( )
      {
         return false;
      }

      int
      getNSolves( )
      {
         return nsolves;
      }

      virtual void
      addModifierParameters(ParameterSet& paramSet) { };

      void
      addParameters(ParameterSet& paramSet)
      {
         paramSet.addParameter(
               fmt::format("{}.enabled", this->name).c_str( ),
               fmt::format("enable modifier {}", this->name).c_str( ),
               this->enabled);

         addModifierParameters(paramSet);
      }

      ModifierStatus
      run(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution, const Timer& timer)
      {
         last_result = { SolverRetcode::OKAY, SolverStatus::kUnknown };
         last_effort = -1;
         if( !enabled )
            return ModifierStatus::kDidNotRun;

         msg.info("modifier {} running\n", name);
#ifdef BUGGER_TBB
         auto start = tbb::tick_count::now( );
#else
         auto start = std::chrono::steady_clock::now();
#endif
         ModifierStatus result = execute(settings, problem, solution);
         if( result == ModifierStatus::kSuccessful )
            nsuccessCall++;
         if ( result != ModifierStatus::kDidNotRun && result != ModifierStatus::kNotAdmissible )
            ncalls++;
#ifdef BUGGER_TBB
         auto end = tbb::tick_count::now( );
         auto duration = end - start;
         execTime = execTime + duration.seconds( );
#else
         auto end = std::chrono::steady_clock::now( );
         execTime = execTime + std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count( ) / 1000.0;
#endif
         msg.info("modifier {} finished\n", name);
         return result;
      }

      void
      printStats(const Message& message)
      {
         double success = ncalls == 0 ? 0.0 : ( (double)nsuccessCall / (double)ncalls ) * 100.0;
         int changes = nchgcoefs + nfixedvars + nchgsides + naggrvars + ndeletedrows + nchgsettings;
         message.info(" {:>18} {:>12} {:>12} {:>18.1f} {:>12} {:>18.3f}\n", name, ncalls, changes, success, nsolves, execTime);
      }

      void
      setName(const String& value)
      {
         this->name = value;
      }

      const String&
      getName( ) const
      {
         return this->name;
      }

      void
      setEnabled(bool value)
      {
         this->enabled = value;
      }

      bool
      isEnabled( ) const
      {
         return this->enabled;
      }

      std::pair<char, SolverStatus>
      getLastResult( ) const
      {
         return last_result;
      }

      long long
      getLastSolvingEffort( ) const
      {
         return last_effort;
      }

   protected:

      virtual ModifierStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) = 0;

      static bool
      is_time_exceeded(const Timer& timer, double tlim)
      {
         return timer.getTime( ) >= tlim;
      }

      BuggerStatus
      call_solver(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution)
      {
         ++nsolves;
         auto solver = factory->create_solver(msg);
         solver->doSetUp(settings, problem, solution);
         if( !parameters.debug_filename.empty( ) )
            solver->writeInstance(parameters.debug_filename, true, true);
         std::pair<char, SolverStatus> result = solver->solve(parameters.passcodes);
         if( !SolverStatusCheck::is_value(result.second) )
         {
            msg.error("Error: Solver returned unknown status {}\n", (int)result.second);
            result.second = SolverStatus::kUndefinedError;
            return BuggerStatus::kError;
         }
         long long effort = solver->getSolvingEffort( );
         if( result.first == SolverRetcode::OKAY )
         {
            msg.info("\tOkay    - Status {:<23} - Effort{:>20}\n", result.second, effort);
            return BuggerStatus::kOkay;
         }
         else
         {
            if( effort >= 0 )
               last_effort = effort;
            last_result = result;
            if( result.first > SolverRetcode::OKAY )
            {
               msg.info("\tBug{:>4} - Status {:<23} - Effort{:>20}\n", (int)result.first, result.second, effort);
               return BuggerStatus::kBug;
            }
            else
            {
               msg.info("\tErr{:>4} - Status {:<23} - Effort{:>20}\n", (int)result.first, result.second, effort);
               return BuggerStatus::kError;
            }
         }
      }

      void apply_changes(Problem<REAL>& copy, const Vec<MatrixEntry<REAL>>& entries) const
      {
         MatrixBuffer<REAL> matrixBuffer { };
         for( const auto &entry: entries )
            matrixBuffer.addEntry(entry.row, entry.col, entry.val);
         copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer);
      }
   };

} // namespace bugger

#endif
