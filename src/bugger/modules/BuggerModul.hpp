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

#ifndef __BUGGER_MODULES_BUGGERMODUL_HPP__
#define __BUGGER_MODULES_BUGGERMODUL_HPP__

#include "bugger/data/BuggerParameters.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"
#include "bugger/interfaces/SolverInterface.hpp"

#ifdef BUGGER_TBB
#include "bugger/misc/tbb.hpp"
#else
#include <chrono>
#endif


namespace bugger {

   enum class ModulStatus : int {
      kDidNotRun = 0,

      kNotAdmissible = 1,

      kUnsuccesful = 2,

      kSuccessful = 3,

   };

   class BuggerModul {

   private:

      std::string name;
      double execTime;
      bool enabled;
      unsigned int ncalls;
      unsigned int nsuccessCall;

   protected:

      const Message& msg;
      const Num<double>& num;
      const BuggerParameters& parameters;
      std::shared_ptr<SolverFactory> factory;
      int nchgcoefs;
      int nfixedvars;
      int nchgsides;
      int naggrvars;
      int nchgsettings;
      int ndeletedrows;
      std::pair<char, SolverStatus> final_result;
      int nsolves;

   public:

      BuggerModul(const Message& _msg, const Num<double>& _num, const BuggerParameters& _parameters,
                  std::shared_ptr<SolverFactory>& _factory) : msg(_msg), num(_num), parameters(_parameters),
                  factory(_factory) {
         ncalls = 0;
         nsuccessCall = 0;
         name = "unnamed";
         execTime = 0.0;
         enabled = true;
         nchgcoefs = 0;
         nfixedvars = 0;
         nchgsides = 0;
         naggrvars = 0;
         nchgsettings = 0;
         ndeletedrows = 0;
         nsolves = 0;
         final_result = { SolverInterface::OKAY, SolverStatus::kUnknown };
      }

      virtual ~BuggerModul( ) = default;


      virtual bool
      initialize( ) {
         return false;
      }

      int
      getNSolves( ) {
         return nsolves;
      }

      virtual void
      addModuleParameters(ParameterSet& paramSet) {
      }

      void
      addParameters(ParameterSet& paramSet) {
         paramSet.addParameter(
               fmt::format("{}.enabled", this->name).c_str( ),
               fmt::format("enable module {}", this->name).c_str( ),
               this->enabled);

         addModuleParameters(paramSet);
      }

      ModulStatus
      run(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution, const Timer& timer) {
         final_result = { SolverInterface::OKAY, SolverStatus::kUnknown };
         if( !enabled )
            return ModulStatus::kDidNotRun;

         msg.info("module {} running\n", name);
#ifdef BUGGER_TBB
         auto start = tbb::tick_count::now( );
#else
         auto start = std::chrono::steady_clock::now();
#endif
         ModulStatus result = execute(settings, problem, solution);
#ifdef BUGGER_TBB
         if( result == ModulStatus::kSuccessful )
            nsuccessCall++;
         if ( result != ModulStatus::kDidNotRun && result != ModulStatus::kNotAdmissible )
            ncalls++;

         auto end = tbb::tick_count::now( );
         auto duration = end - start;
         execTime = execTime + duration.seconds( );
#else
         auto end = std::chrono::steady_clock::now();
         execTime = execTime + std::chrono::duration_cast<std::chrono::milliseconds>(
                                   end- start ).count()/1000;
#endif
         msg.info("module {} finished\n", name);
         return result;
      }

      void
      printStats(const Message& message) {
         double success = ncalls == 0 ? 0.0 : ( double(nsuccessCall) / double(ncalls)) * 100.0;
         int changes = nchgcoefs + nfixedvars + nchgsides + naggrvars + ndeletedrows + nchgsettings;
         message.info(" {:>18} {:>12} {:>12} {:>18.1f} {:>12} {:>18.3f}\n", name, ncalls, changes, success, nsolves, execTime);
      }


      bool
      isEnabled( ) const {
         return this->enabled;
      }

      const std::string&
      getName( ) const {
         return this->name;
      }

      std::pair<char, SolverStatus>
      getFinalResult( ) const {
         return final_result;
      }

      void
      setEnabled(bool value) {
         this->enabled = value;
      }

   protected:

      double get_linear_activity(SparseVectorView<double>& data, Solution<double>& solution) {
         StableSum<double> sum;
         for( int i = 0; i < data.getLength( ); ++i )
            sum.add(solution.primal[ data.getIndices( )[ i ] ] * data.getValues( )[ i ]);
         return sum.get( );
      }

      virtual ModulStatus
      execute(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution) = 0;

      void
      setName(const std::string& value) {
         this->name = value;
      }


      static bool
      is_time_exceeded(const Timer& timer, double tlim) {
         return tlim != std::numeric_limits<double>::max( ) &&
                timer.getTime( ) >= tlim;
      }

      BuggerStatus
      call_solver(const SolverSettings& settings, const Problem<double>& problem, const Solution<double>& solution) {
         ++nsolves;
         auto solver = factory->create_solver(msg);
         solver->doSetUp(settings, problem, solution);
         if( !parameters.debug_filename.empty( ) )
            solver->writeInstance(parameters.debug_filename, true);
         std::pair<char, SolverStatus> result = solver->solve(parameters.passcodes);
         if( result.first == SolverInterface::OKAY )
         {
            msg.info("\tOkay  - Status {}\n", result.second);
            return BuggerStatus::kOkay;
         }
         else if( result.first > SolverInterface::OKAY )
         {
            final_result = result;
            msg.info("\tBug {} - Status {}\n", (int) result.first, result.second);
            return BuggerStatus::kBug;
         }
         else
         {
            final_result = result;
            msg.info("\tError {}\n", (int) result.first);
            return BuggerStatus::kError;
         }
      }

      void apply_changes(Problem<double>& copy, const Vec<MatrixEntry<double>>& entries) const {
         MatrixBuffer<double> matrixBuffer{ };
         for( const auto &entry: entries )
            matrixBuffer.addEntry(entry.row, entry.col, entry.val);
         copy.getConstraintMatrix( ).changeCoefficients(matrixBuffer);
      }
   };

} // namespace bugger

#endif
