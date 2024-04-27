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

   template <typename REAL>
   class SideModul : public BuggerModul<REAL> {

   public:

      explicit SideModul(const Message& _msg, const Num<REAL>& _num, const BuggerParameters& _parameters,
                         std::shared_ptr<SolverFactory<REAL>>& _factory)
                         : BuggerModul<REAL>(_msg, _num, _parameters, _factory) {
         this->setName("side");
      }

   private:

      bool
      isSideAdmissable(const Problem<REAL>& problem, const int& row) const {
         return !problem.getRowFlags( )[ row ].test(RowFlag::kRedundant)
           && ( problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf)
             || problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf)
             || !this->num.isZetaEq(problem.getConstraintMatrix( ).getLeftHandSides( )[ row ], problem.getConstraintMatrix( ).getRightHandSides( )[ row ]) );
      }

      ModulStatus
      execute(SolverSettings& settings, Problem<REAL>& problem, Solution<REAL>& solution) override {

         if( solution.status == SolutionStatus::kUnbounded )
            return ModulStatus::kNotAdmissible;

         long long batchsize = 1;

         if( this->parameters.nbatches > 0 )
         {
            batchsize = this->parameters.nbatches - 1;
            for( int row = problem.getNRows( ) - 1; row >= 0; --row )
               if( isSideAdmissable(problem, row) )
                  ++batchsize;
            if( batchsize == this->parameters.nbatches - 1 )
               return ModulStatus::kNotAdmissible;
            batchsize /= this->parameters.nbatches;
         }

         bool admissible = false;
         auto copy = Problem<REAL>(problem);
         ConstraintMatrix<REAL>& matrix = copy.getConstraintMatrix( );
         Vec<std::pair<int, REAL>> applied_reductions { };
         Vec<std::pair<int, REAL>> batches { };
         batches.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isSideAdmissable(copy, row) )
            {
               admissible = true;
               auto data = matrix.getRowCoefficients(row);
               bool integral = true;
               REAL fixedval;
               for( int index = 0; index < data.getLength( ); ++index )
               {
                  if( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kFixed) && ( !copy.getColFlags( )[ data.getIndices( )[ index ] ].test(ColFlag::kIntegral) || !this->num.isEpsIntegral(data.getValues( )[ index ]) ) )
                  {
                     integral = false;
                     break;
                  }
               }
               if( solution.status == SolutionStatus::kFeasible )
               {
                  fixedval = this->get_linear_activity(data, solution);
                  if( integral )
                     fixedval = this->num.round(fixedval);
               }
               else
               {
                  fixedval = 0.0;
                  if( integral )
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = this->num.min(fixedval, this->num.epsFloor(matrix.getRightHandSides( )[ row ]));
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = this->num.max(fixedval, this->num.epsCeil(matrix.getLeftHandSides( )[ row ]));
                  }
                  else
                  {
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                        fixedval = this->num.min(fixedval, matrix.getRightHandSides( )[ row ]);
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                        fixedval = this->num.max(fixedval, matrix.getLeftHandSides( )[ row ]);
                  }
               }
               matrix.modifyLeftHandSide( row, this->num, fixedval );
               matrix.modifyRightHandSide( row, this->num, fixedval );
               batches.emplace_back(row, fixedval);
            }

            if( !batches.empty() && ( batches.size() >= batchsize || row <= 0 ) )
            {
               if( this->call_solver(settings, copy, solution) == BuggerStatus::kOkay )
               {
                  copy = Problem<REAL>(problem);
                  for( const auto &item: applied_reductions )
                  {
                     matrix.modifyLeftHandSide( item.first, this->num, item.second );
                     matrix.modifyRightHandSide( item.first, this->num, item.second );
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
         this->nchgsides += 2 * applied_reductions.size();
         return ModulStatus::kSuccessful;
      }
   };

} // namespace bugger

#endif
