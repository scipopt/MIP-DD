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

#ifndef BUGGER_MODUL_COEFFICIENT_HPP_
#define BUGGER_MODUL_COEFFICIENT_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/BuggerStatus.hpp"

namespace bugger {

   class CoefficientModul : public BuggerModul {
   public:
      CoefficientModul(const Message &_msg, const Num<double> &_num) : BuggerModul( ) {
         this->setName("coefficient");
         this->msg = _msg;
         this->num = _num;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isCoefficientAdmissible(const Problem<double> problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant))
            return false;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         const VariableDomains<double> &domains = problem.getVariableDomains( );
         for( int i = 0; i < data.getLength( ); ++i )
         {
            int var = data.getIndices( )[ i ];
            if( is_lb_ge_than_ub(domains, var))
               return true;
         }
         return false;
      }

      bool is_lb_ge_than_ub(const VariableDomains<double> &domains, int var) const {
         return domains.flags[ var ].test(ColFlag::kUbInf) || domains.flags[ var ].test(ColFlag::kLbInf)
                || num.isZetaGE(domains.lower_bounds[ var ], domains.upper_bounds[ var ]);
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         auto copy = Problem<double>(problem);
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, double>> applied_reductions_lhs { };
         Vec<std::pair<int, double>> applied_reductions_rhs { };
         MatrixBuffer<double> batches_coeff { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isCoefficientAdmissible(problem, i) )
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         batches_lhs.reserve(batchsize);
         batches_rhs.reserve(batchsize);

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isCoefficientAdmissible(copy, row) )
            {
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);

               for( int index = data.getLength( ) - 1; index >= 0; --index )
               {
                  int var = data.getIndices( )[ index ];
                  if( is_lb_ge_than_ub(copy.getVariableDomains( ), var) )
                  {
                     double fixedval;

                     if( !solution_exists )
                     {
                        if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral) )
                           fixedval = num.max(num.min(0.0, num.zetaFloor(copy.getUpperBounds( )[ var ])), num.zetaCeil(copy.getLowerBounds( )[ var ]));
                        else
                           fixedval = num.max(num.min(0.0, copy.getUpperBounds( )[ var ]), copy.getLowerBounds( )[ var ]);
                     }
                     else
                     {
                        if( copy.getColFlags( )[ var ].test(ColFlag::kIntegral) )
                           fixedval = num.round(solution.primal[ var ]);
                        else
                           fixedval = solution.primal[ var ];
                     }

                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kLhsInf) )
                     {
                        copy.getConstraintMatrix( ).modifyLeftHandSide( row, num, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] - data.getValues( )[ index ] * fixedval );
                     }
                     if( !copy.getRowFlags( )[ row ].test(RowFlag::kRhsInf) )
                     {
                        copy.getConstraintMatrix( ).modifyRightHandSide( row, num, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] - data.getValues( )[ index ] * fixedval );
                     }

                     batches_coeff.addEntry(row, var, 0.0);
                  }
               }

               batches_lhs.push_back({ row, copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] });
               batches_rhs.push_back({ row, copy.getConstraintMatrix( ).getRightHandSides( )[ row ] });
            }

            if( !batches_lhs.empty() && ( batches_lhs.size() >= batchsize || row <= 0 ) )
            {
               copy.getConstraintMatrix( ).changeCoefficients(batches_coeff);
               auto solver = createSolver();
               solver->parseParameters();
               solver->doSetUp(copy, solution_exists, solution);
               if( solver->run(msg, originalSolverStatus) == BuggerStatus::kSuccess )
               {
                  copy = Problem<double>(problem);
                  copy.getConstraintMatrix( ).changeCoefficients(applied_entries);
                  for( const auto &item: applied_reductions_lhs )
                     copy.getConstraintMatrix( ).modifyLeftHandSide( item.first, num, item.second );
                  for( const auto &item: applied_reductions_rhs )
                     copy.getConstraintMatrix( ).modifyRightHandSide( item.first, num, item.second );
               }
               else
               {
                  SmallVec<int, 32> buffer;
                  const MatrixEntry<double> *iter = batches_coeff.template begin<true>(buffer);
                  while( iter != batches_coeff.end( ) )
                  {
                     applied_entries.addEntry(iter->row, iter->col, iter->val);
                     iter = batches_coeff.template next<true>( buffer );
                  }
                  applied_reductions_lhs.insert(applied_reductions_lhs.end(), batches_lhs.begin(), batches_lhs.end());
                  applied_reductions_rhs.insert(applied_reductions_rhs.end(), batches_rhs.begin(), batches_rhs.end());
               }
               batches_coeff.clear();
               batches_lhs.clear();
               batches_rhs.clear();
            }
         }

         if( applied_reductions_lhs.empty() )
         {
            return ModulStatus::kUnsuccesful;
         }
         else
         {
            problem = copy;
            nchgcoefs += applied_entries.getNnz();
            nchgsides += applied_reductions_lhs.size()+applied_reductions_rhs.size();
            return ModulStatus::kSuccessful;
         }
      }
   };

} // namespace bugger

#endif
