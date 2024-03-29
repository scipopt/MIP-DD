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

#ifndef __BUGGER_MODULE_SIDE_HPP__
#define __BUGGER_MODULE_SIDE_HPP__

#include "bugger/modules/BuggerModul.hpp"


namespace bugger {

   class SideModul : public BuggerModul {

   public:

      explicit SideModul(const Message& _msg, const Num<double>& _num, const BuggerParameters& _parameters,
                         std::shared_ptr<SolverFactory>& _factory) : BuggerModul(_msg, _num, _parameters, _factory) {
         this->setName("side");
      }

   private:

      bool
      isSideAdmissable(const Problem<double>& problem, const int& row) const {
         return !problem.getRowFlags( )[ row ].test(RowFlag::kRedundant)
           && ( problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
             || problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
             || !num.isZetaEq(problem.getConstraintMatrix( ).getLeftHandSides( )[ row ], problem.getConstraintMatrix( ).getRightHandSides( )[ row ]) );
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<double>& problem, Solution<double>& solution) override {

         if( solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         int batchsize = 1;

         if( parameters.nbatches > 0 )
         {
            batchsize = parameters.nbatches - 1;
            for( int row = problem.getNRows( ) - 1; row >= 0; --row )
               if( isSideAdmissable(problem, row) )
                  ++batchsize;
            if( batchsize == parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<double>(problem);
         ConstraintMatrix<double>& matrix = copy.getConstraintMatrix( );
         Vec<std::pair<int, double>> applied_reductions { };
         Vec<std::pair<int, double>> batches { };
         batches.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isSideAdmissable(copy, row) )
            {
               admissible = true;
               auto data = matrix.getRowCoefficients(row);
               bool integral = true;
               double fixedval;
               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kIntegral) || !num.isEpsIntegral(data.getValues( )[ index ]) ) )
                  {
                     integral = false;
                     break;
                  }
               }
               if( solution.status == SolutionStatus::kFeasible )
               {
                  fixedval = get_linear_activity(data, solution);
                  if( integral )
                     fixedval = num.round(fixedval);
               }
               else
               {
                  fixedval = 0.0;
                  if( integral )
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = num.min(fixedval, num.epsFloor(matrix.getRightHandSides( )[ row ]));
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = num.max(fixedval, num.epsCeil(matrix.getLeftHandSides( )[ row ]));
                  }
                  else
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = num.min(fixedval, matrix.getRightHandSides( )[ row ]);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = num.max(fixedval, matrix.getLeftHandSides( )[ row ]);
                  }
               }
               matrix.modifyLeftHandSide( row, num, fixedval );
               matrix.modifyRightHandSide( row, num, fixedval );
               batches.emplace_back(row, fixedval);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               if( call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<double>(problem);
                  for( const auto &item: applied_reductions )
                  {
                     matrix.modifyLeftHandSide( item.first, num, item.second );
                     matrix.modifyRightHandSide( item.first, num, item.second );
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
         nchgsides += 2 * applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
