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

#ifndef _BUGGER_PRESOLVE_METHOD_HPP_
#define _BUGGER_PRESOLVE_METHOD_HPP_

#include "bugger/data/BuggerOptions.hpp"
#include "bugger/io/Message.hpp"
#include "bugger/misc/Num.hpp"
#include "bugger/misc/Vec.hpp"
#include "bugger/misc/fmt.hpp"
#include "bugger/misc/Timer.hpp"

#ifdef BUGGER_TBB

#include "bugger/misc/tbb.hpp"

#else
#include <chrono>
#endif

#include <bitset>


namespace bugger {

   enum class ModulStatus : int {
      kDidNotRun = 0,

      kNotAdmissible = 1,

      kUnsuccesful = 2,

      kSuccessful = 3,

   };

   class BuggerModul {
   public:
      BuggerModul( std::shared_ptr<SolverFactory>& _factory) {
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
         solver_factory = _factory;
      }

      virtual ~BuggerModul( ) = default;


      virtual bool
      initialize( ) {
         return false;
      }

      virtual void
      addModuleParameters(ParameterSet &paramSet) {
      }

      void
      addParameters(ParameterSet &paramSet) {
         paramSet.addParameter(
               fmt::format("{}.enabled", this->name).c_str( ),
               fmt::format("is presolver {} enabled", this->name).c_str( ),
               this->enabled);

         addModuleParameters(paramSet);
      }

      ModulStatus
      run(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution, const BuggerOptions &options,
          const Timer &timer) {
         if( !enabled )
            return ModulStatus::kDidNotRun;

         msg.info("module {} running\n", name);
#ifdef BUGGER_TBB
         auto start = tbb::tick_count::now( );
#else
         auto start = std::chrono::steady_clock::now();
#endif
         ModulStatus result = execute(problem, settings, solution, options, timer);
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
      printStats(const Message &message) {
         double success = ncalls == 0 ? 0.0 : ( double(nsuccessCall) / double(ncalls)) * 100.0;
         int changes = nchgcoefs + nfixedvars + nchgsides + naggrvars + ndeletedrows + nchgsettings;
         message.info(" {:>18} {:>12} {:>12} {:>18.1f} {:>18.3f}\n", name, ncalls, changes, success, execTime);
      }


      bool
      isEnabled( ) const {
         return this->enabled;
      }

      const std::string &
      getName( ) const {
         return this->name;
      }

      unsigned int
      getNCalls( ) const {
         return ncalls;
      }

      void
      setEnabled(bool value) {
         this->enabled = value;
      }

   protected:

      std::unique_ptr<SolverInterface>
      createSolver(){
         return solver_factory->create_solver();
      }

      double get_linear_activity(SparseVectorView<double> &data, Solution<double> &solution) {
         StableSum<double> sum;
         for( int i = 0; i < data.getLength( ); i++ )
            sum.add(solution.primal[ data.getIndices( )[ i ]] * data.getValues( )[ i ]);
         return sum.get( );
      }

      virtual ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution,
              const BuggerOptions &options, const Timer &timer) = 0;

      void
      setName(const std::string &value) {
         this->name = value;
      }


      static bool
      is_time_exceeded(const Timer &timer, double tlim) {
         return tlim != std::numeric_limits<double>::max( ) &&
                timer.getTime( ) >= tlim;
      }


      BuggerStatus
      call_solver(SolverInterface *solver, const Message &msg, const BuggerOptions &options) {

         std::pair<char, SolverStatus> result = solver->solve(options.passcodes);
         if( result.first == SolverInterface::OKAY )
         {
            msg.info("\tOkay  - Status {}\n", result.second);
            return BuggerStatus::kOkay;
         }
         else if( result.first > SolverInterface::OKAY )
         {
            msg.info("\tBug {} - Status {}\n", (int) result.first, result.second);
            return BuggerStatus::kBug;
         }
         else
         {
            msg.info("\tError {}\n", (int) result.first);
            return BuggerStatus::kError;
         }
      }

   private:
      std::string name;
      double execTime;
      bool enabled;
      unsigned int ncalls;
      unsigned int nsuccessCall;
   protected:
      int nchgcoefs;
      int nfixedvars;
      int nchgsides;
      int naggrvars;
      int nchgsettings;
      int ndeletedrows;
      Num<double> num;
      Message msg;
      std::shared_ptr<SolverFactory> solver_factory;
   };

} // namespace bugger

#endif
