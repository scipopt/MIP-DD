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

#ifndef __BUGGER_INTERFACES_SOLVERINTERFACE_HPP__
#define __BUGGER_INTERFACES_SOLVERINTERFACE_HPP__

#include "bugger/data/SolverSettings.hpp"
#include "bugger/data/Problem.hpp"
#include "bugger/data/Solution.hpp"
#include "bugger/misc/Timer.hpp"
#include "bugger/interfaces/SolverStatus.hpp"


namespace bugger
{
   enum SolverRetcode : char
   {
      OKAY              = 0,
      DUALFAIL          = 1,
      PRIMALFAIL        = 2,
      OBJECTIVEFAIL     = 3,
      COMPLETIONFAIL    = 4,
      CERTIFICATIONFAIL = 5
   };

   /**
    * API to access the solver
    * Optional methods are marked with **optional**. They should be implemented to enable further functionality.
    * @tparam REAL arithmetic type of problem, solution, and modifications
    */
   template <typename REAL>
   class SolverInterface
   {
   protected:

      const Message& msg;
      SolverSettings* adjustment = nullptr;
      const Problem<REAL>* model = nullptr;
      const Solution<REAL>* reference = nullptr;
      REAL value { std::numeric_limits<REAL>::signaling_NaN() };

   public:

      SolverInterface(const Message& _msg) : msg(_msg) { }

      /** **optional**
       * prints the header of the used solver
       * _if not implemented, then the solver specification will not be contained in the log_
       */
      virtual
      void
      print_header( ) const { }

      /** **optional**
       * detects setting with given name
       * _if not implemented, then it can not be used for automatic limit settings_
       * @param name
       * @return whether setting is available
       */
      virtual
      bool
      has_setting(const String& name) const
      {
         return false;
      }

      /** **optional**
       * parse Settings
       * _if returned boost::none, then modifier Setting will be deactivated_
       * @param filename
       */
      virtual
      boost::optional<SolverSettings>
      parseSettings(const String& filename) const
      {
         return boost::none;
      }

      /**
       * loads settings, problem, and solution
       * @param settings
       * @param problem
       * @param solution
       */
      virtual
      void
      doSetUp(SolverSettings& settings, const Problem<REAL>& problem, const Solution<REAL>& solution) = 0;

      /**
       * solves the instance
       * @param passcodes suppress certain codes (referring to the returned values) -> these are not considered as bugs
       * @return a pair<char, SolverStatus>: Negative values in the char are reserved for solver internal errors while the remaining ones are declared in SolverInterface::Retcode. The SolverStatus primarily serves to be printed in the log holding the solution status of the solve, for example infeasible, unbounded, optimal, or specific limits reached.
       */
      virtual
      std::pair<char, SolverStatus>
      solve(const Vec<int>& passcodes) = 0;

      /** **optional**
       * provides measure for the solving effort to adapt batch number
       * _if returned -1 initially, then automatic batch adaption will be deactivated_
       * @return a long long int: Non-negative value proportional to effort of the solve or -1 if unknown
       */
      virtual
      long long
      getSolvingEffort( ) const
      {
         return -1;
      }

      /** **optional**
       * read setting-problem-solution tuple from files
       * _if returned boost::none for setting, modifier Setting will be deactivated_
       * _if returned boost::none for problem or solution, then internal parsers give a try_
       * @param settings_filename
       * @param problem_filename
       * @param solution_filename
       * @return optionalized tuple of setting, problem, and solution
       */
      virtual
      std::tuple<boost::optional<SolverSettings>, boost::optional<Problem<REAL>>, boost::optional<Solution<REAL>>>
      readInstance(const String& settings_filename, const String& problem_filename, const String& solution_filename)
      {
         return { boost::none, boost::none, boost::none };
      }

      /** **optional**
       * write stored setting-problem-solution tuple to files
       * _if returned false for setting, no settings will be written which is deprecated_
       * _if returned false for problem or solution, then internal writers giva a try_
       * @param filename
       * @param writesettings
       * @param writesolution
       * @return boolean tuple whether setting, problem, and solution are not required or written successfully
       */
      virtual
      std::tuple<bool, bool, bool>
      writeInstance(const String& filename, const bool& writesettings, const bool& writesolution) const
      {
         return { !writesettings, false, !writesolution };
      }

      virtual
      ~SolverInterface() = default;

   protected:

      REAL
      relax(const REAL& bound, const bool& increase, const REAL& tolerance, const REAL& infinity) const
      {
         assert(tolerance > 0);
         assert(tolerance * 2 < 1);
         assert(infinity > 1);

         if( bound <= -infinity )
            return -infinity;
         else if( bound >= infinity )
            return infinity;
         else if( abs(bound) < 1 )
            return bound + (increase ? tolerance : -tolerance);
         else if( (abs(bound) + 1) * tolerance > 1 )
            return bound + (increase ? REAL(1 - tolerance) : REAL(tolerance - 1));
         else if( bound < 0 )
            return bound * (1 + (increase ? -tolerance : tolerance));
         else
            return bound * (1 + (increase ? tolerance : -tolerance));
      }

      char
      check_dual_bound(const REAL& dual, const REAL& tolerance, const REAL& infinity) const
      {
         if( abs(dual) > infinity )
         {
            msg.detailed( "\tDual beyond infinity ({:<3} > {:<3})\n", abs(dual), infinity );
            return DUALFAIL;
         }

         switch( reference->status )
         {
         case SolutionStatus::kUnknown:
            return OKAY;
         case SolutionStatus::kFeasible:
            if( reference->primal.size() == model->getNCols() )
               break;
            if( (model->getObjective().sense ? dual : -dual) == infinity )
            {
               msg.detailed( "\tDual against feasibility ({:<3})\n", dual );
               return DUALFAIL;
            }
            return OKAY;
         case SolutionStatus::kInfeasible:
         case SolutionStatus::kUnbounded:
            break;
         }

         if( model->getObjective().sense )
         {
            if( dual > relax( value, true, tolerance, infinity ) )
            {
               msg.detailed( "\tDual above reference ({:<3} > {:<3})\n", dual, value );
               return DUALFAIL;
            }
         }
         else
         {
            if( dual < relax( value, false, tolerance, infinity ) )
            {
               msg.detailed( "\tDual below reference ({:<3} < {:<3})\n", dual, value );
               return DUALFAIL;
            }
         }

         return OKAY;
      }

      char
      check_primal_solution(const Vec<Solution<REAL>>& solution, const REAL& tolerance, const REAL& infinity) const
      {
         for( int i = solution.size() - 1; i >= 0; --i )
         {
            if( solution[i].status == SolutionStatus::kUnknown )
               continue;

            if( solution[i].status != SolutionStatus::kInfeasible )
            {
               assert(solution[i].primal.size() == model->getNCols());

               for( int col = 0; col < model->getNCols(); ++col )
               {
                  if( model->getColFlags()[col].test( ColFlag::kFixed ) )
                     continue;

                  if( solution[i].primal[col] < relax( model->getColFlags()[col].test( ColFlag::kLbInf ) ? -infinity : model->getLowerBounds()[col], false, tolerance, infinity )
                   || solution[i].primal[col] > relax( model->getColFlags()[col].test( ColFlag::kUbInf ) ?  infinity : model->getUpperBounds()[col], true,  tolerance, infinity )
                   || ( model->getColFlags()[col].test( ColFlag::kIntegral ) && abs(solution[i].primal[col] - round(solution[i].primal[col])) > tolerance ) )
                  {
                     msg.detailed( "\tColumn {:<3} outside domain (value {:<3}) in solution {:<3}\n", model->getVariableNames()[col], solution[i].primal[col], i );
                     return PRIMALFAIL;
                  }
               }

               for( int row = 0; row < model->getNRows(); ++row )
               {
                  if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                     continue;

                  REAL activity { model->getPrimalActivity(solution[i], row) };

                  if( ( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < relax( model->getConstraintMatrix().getLeftHandSides()[row],  false, tolerance, infinity ) )
                   || ( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) && activity > relax( model->getConstraintMatrix().getRightHandSides()[row], true,  tolerance, infinity ) ) )
                  {
                     msg.detailed( "\tRow {:<3} outside range (activity {:<3}) in solution {:<3}\n", model->getConstraintNames()[row], activity, i );
                     return PRIMALFAIL;
                  }
               }
            }

            if( solution[i].status == SolutionStatus::kUnbounded )
            {
               assert(solution[i].ray.size() == model->getNCols());

               REAL scale { };

               for( int col = 0; col < model->getNCols(); ++col )
                  if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                     scale = max(scale, abs(solution[i].ray[col]));

               scale *= tolerance;

               for( int col = 0; col < model->getNCols(); ++col )
               {
                  if( !model->getColFlags()[col].test( ColFlag::kFixed )
                      && ( ( !model->getColFlags()[col].test( ColFlag::kLbInf ) && solution[i].ray[col] < -scale )
                        || ( !model->getColFlags()[col].test( ColFlag::kUbInf ) && solution[i].ray[col] >  scale ) ) )
                  {
                     msg.detailed( "\tColumn {:<3} escaped domain (rayval {:<3}) in solution {:<3}\n", model->getVariableNames()[col], solution[i].ray[col], i );
                     return PRIMALFAIL;
                  }
               }

               for( int row = 0; row < model->getNRows(); ++row )
               {
                  if( model->getRowFlags()[row].test( RowFlag::kRedundant ) )
                     continue;

                  REAL activity { model->getRayActivity(solution[i], row) };

                  if( ( !model->getRowFlags()[row].test( RowFlag::kLhsInf ) && activity < -scale )
                   || ( !model->getRowFlags()[row].test( RowFlag::kRhsInf ) && activity >  scale ) )
                  {
                     msg.detailed( "\tRow {:<3} escaped range (rayact {:<3}) in solution {:<3}\n", model->getConstraintNames()[row], activity, i );
                     return PRIMALFAIL;
                  }
               }
            }
         }

         return OKAY;
      }

      char
      check_objective_value(const REAL& primal, const Solution<REAL>& solution, const REAL& tolerance, const REAL& infinity) const
      {
         if( abs(primal) > infinity )
         {
            msg.detailed( "\tPrimal beyond infinity ({:<3} > {:<3})\n", abs(primal), infinity );
            return OBJECTIVEFAIL;
         }

         if( solution.status == SolutionStatus::kUnknown )
            return OKAY;

         if( solution.status == SolutionStatus::kUnbounded )
         {
            assert(solution.ray.size() == model->getNCols());

            REAL slope { model->getRayObjective(solution) };
            REAL scale { };

            for( int col = 0; col < model->getNCols(); ++col )
               if( !model->getColFlags()[col].test( ColFlag::kFixed ) )
                  scale = max(scale, abs(solution.ray[col]));

            scale *= tolerance;

            if( (model->getObjective().sense ? -slope : slope) > scale )
               return OKAY;
         }

         REAL result;

         if( solution.status == SolutionStatus::kInfeasible )
            result = model->getObjective().sense ? infinity : -infinity;
         else
         {
            assert(solution.primal.size() == model->getNCols());

            result = model->getPrimalObjective(solution);
         }

         if( model->getObjective().sense )
         {
            if( primal < relax( result, false, tolerance, infinity ) )
            {
               msg.detailed( "\tPrimal below reference ({:<3} < {:<3})\n", primal, result );
               return OBJECTIVEFAIL;
            }
         }
         else
         {
            if( primal > relax( result, true, tolerance, infinity ) )
            {
               msg.detailed( "\tPrimal above reference ({:<3} > {:<3})\n", primal, result );
               return OBJECTIVEFAIL;
            }
         }

         return OKAY;
      }

      char
      check_count_number(const REAL& dual, const REAL& primal, const long long& count, const REAL& infinity) const
      {
         assert(infinity > 1);

         if( abs(dual) > infinity || (model->getObjective().sense ? primal : -primal) != infinity || count < -1 )
         {
            msg.detailed( "\tResult not consistent (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
            return OBJECTIVEFAIL;
         }

         switch( reference->status )
         {
         case SolutionStatus::kUnknown:
            break;
         case SolutionStatus::kInfeasible:
            if( count == 0 )
               break;
            msg.detailed( "\tInfeasibility not respected (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
            return PRIMALFAIL;
         case SolutionStatus::kFeasible:
            if( reference->primal.size() == model->getNCols() && abs(value) >= infinity )
               break;
         case SolutionStatus::kUnbounded:
            if( count >= 1 || (model->getObjective().sense ? dual : -dual) != infinity )
               break;
            msg.detailed( "\tFeasibility not respected (dual {:<3}, primal {:<3}, count {:<3}, infinity {:<3})\n", dual, primal, count, infinity );
            return DUALFAIL;
         }

         return OKAY;
      }
   };

   template <typename REAL>
   class SolverFactory
   {
   public:

      virtual
      void
      addParameters(ParameterSet& parameterset) = 0;

      virtual
      std::unique_ptr<SolverInterface<REAL>>
      create_solver(const Message& msg) = 0;

      virtual
      ~SolverFactory() = default;
   };

} // namespace bugger

#endif
