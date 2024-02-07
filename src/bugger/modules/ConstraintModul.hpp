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

#ifndef BUGGER_MODUL_CONSTRAINT_HPP_
#define BUGGER_MODUL_CONSTRAINT_HPP_

#include "bugger/modules/BuggerModul.hpp"

namespace bugger {

   class ConstraintModul : public BuggerModul {
   public:
      explicit ConstraintModul( const Message &_msg, const Num<double> &_num, std::shared_ptr<SolverFactory>& factory) : BuggerModul(factory) {
         this->setName("constraint");
         this->msg = _msg;
         this->num = _num;

      }

      bool
      initialize( ) override {
         return false;
      }

      bool isConstraintAdmissible(const Problem<double>& problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant) )
            return false;
         return true;
      }

      ModulStatus
      execute(Problem<double> &problem, SolverSettings& settings, Solution<double> &solution,
              const BuggerOptions &options, const Timer &timer) override {

         if( solution.status == SolutionStatus::kInfeasible )
            return ModulStatus::kNotAdmissible;

         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isConstraintAdmissible(problem, i) )
                  ++batchsize;
            if( batchsize == options.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= options.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         Vec<int> applied_reductions { };
         Vec<int> batches { };
         batches.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isConstraintAdmissible(copy, row) )
            {
               admissible = true;
               assert(!copy.getRowFlags( )[ row ].test(RowFlag::kRedundant));
               copy.getRowFlags( )[ row ].set(RowFlag::kRedundant);
               batches.push_back(row);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               auto solver = createSolver();
               solver->doSetUp(settings, copy, solution);
               if( !options.debug_filename.empty( ))
                  solver->writeInstance(options.debug_filename, false);
               if( call_solver(solver.get( ), msg, options) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                  {
                     assert(!copy.getRowFlags( )[ item ].test(RowFlag::kRedundant));
                     copy.getRowFlags( )[ item ].set(RowFlag::kRedundant);
                  }
               }
               else
                  applied_reductions.insert(applied_reductions.end(), batches.begin(), batches.end());
               batches.clear();
            }
         }

         if( !admissible )
            return ModulStatus::kNotAdmissible;
         if( applied_reductions.empty() )
            return ModulStatus::kUnsuccesful;
         problem = copy;
         ndeletedrows += applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
