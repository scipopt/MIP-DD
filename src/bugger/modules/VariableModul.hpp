/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2023             Konrad-Zuse-Zentrum                        */
/*                     fuer Informationstechnik Berlin                       */
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

#ifndef _BUGGER_MODUL_VARIABLE_HPP_
#define _BUGGER_MODUL_VARIABLE_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/Status.hpp"

namespace bugger {

   class VariableModul : public BuggerModul {
   public:
      VariableModul( const Message& _msg ) : BuggerModul( ) {
         this->setName("variable");
         this->msg = _msg;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isVariableAdmissible(const Problem<double>& problem, int var) {
         if( problem.getColFlags()[var].test(ColFlag::kFixed) )
            return false;
         /* keep restricted variables because they might be already fixed */
         return problem.getColFlags()[var].test(ColFlag::kLbInf) || problem.getColFlags()[var].test(ColFlag::kUbInf) ||
            num.isLT(problem.getLowerBounds()[var], problem.getUpperBounds()[var]);
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double>& solution, bool solution_exists, const BuggerOptions &options, const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto copy = Problem<double>(problem);
         Vec<std::pair<int, double>> applied_reductions{};
         Vec<std::pair<int, double>> batches{};

         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int i = problem.getNCols() - 1; i >= 0; --i )
               if( isVariableAdmissible(problem, i))
                  ++batchsize;

            batchsize /= options.nbatches;
         }

         int nbatch = 0;

         for( int var = copy.getNCols() - 1; var >= 0; --var )
         {
            if( isVariableAdmissible(copy, var))
            {
               SCIP_Real fixedval;

               if( !solution_exists)
               {
                  if( copy.getColFlags()[var].test(ColFlag::kIntegral))
                     fixedval = MAX(MIN(0.0, num.epsFloor(copy.getUpperBounds()[var])),
                                    num.epsCeil(copy.getLowerBounds()[var]));
                  else
                     fixedval = MAX(MIN(0.0, copy.getUpperBounds()[var]), copy.getLowerBounds()[var]);
               }
               else
               {
                  if( copy.getColFlags()[var].test(ColFlag::kIntegral))
                     fixedval = num.round(solution.primal[var]);
                  else
                     fixedval = solution.primal[var];
               }

               copy.getLowerBounds()[var] = fixedval;
               copy.getUpperBounds()[var] = fixedval;
               ++nbatch;
            }

            if( nbatch >= 1 && ( nbatch >= batchsize || var <= 0 ))
            {
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg) != Status::kFail )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions ){
                     problem.getLowerBounds()[item.first] = item.second;
                     problem.getUpperBounds()[item.first] = item.second;
                  }
               }
               else
               {
                  //TODO: push back together
                  for( const auto &item: batches )
                     applied_reductions.push_back(item);
                  nfixedvars += nbatch;
                  result = ModulStatus::kSuccessful;
                  batches.clear();
               }
               nbatch = 0;
            }
         }
         problem = Problem<double>(copy);
         return result;
      }
   };


} // namespace bugger

#endif
