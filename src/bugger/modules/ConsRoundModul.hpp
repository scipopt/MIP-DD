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

#ifndef BUGGER_MODUL_CONSROUND_HPP_
#define BUGGER_MODUL_CONSROUND_HPP_

#include "bugger/modules/BuggerModul.hpp"
#include "bugger/interfaces/Status.hpp"

namespace bugger {

   class ConsRoundModul : public BuggerModul {
   public:
      ConsRoundModul( ) : BuggerModul( ) {
         this->setName("consround");
      }

      bool
      initialize( ) override {
         return false;
      }

      SCIP_Bool SCIPisConsroundAdmissible(Problem<double> &problem, int row) {
         if( problem.getRowFlags( )[ row ].test(RowFlag::kLhsInf) ||
             problem.getRowFlags( )[ row ].test(RowFlag::kRhsInf))
            return true;

         double lhs = problem.getConstraintMatrix( ).getLeftHandSides( )[ row ];
         double rhs = problem.getConstraintMatrix( ).getRightHandSides( )[ row ];
         if( !num.isEq(lhs, rhs) && ( !num.isIntegral(lhs) || !num.isIntegral(rhs)))
            return true;
         auto data = problem.getConstraintMatrix( ).getRowCoefficients(row);
         for( int i = 0; i < data.getLength( ); ++i )
            if( num.isIntegral(data.getValues( )[ i ]))
               return TRUE;
         /* leave sparkling or fixed constraints */
         return FALSE;
      }


      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;
         auto copy = Problem<double>(problem);
         MatrixBuffer<double> applied_entries { };
         Vec<std::pair<int, int >> applied_reductions_lhs { };
         Vec<std::pair<int, int>> applied_reductions_rhs { };
         Vec<std::pair<int, double>> batches_lhs { };
         Vec<std::pair<int, double>> batches_rhs { };
         MatrixBuffer<double> batches_coeff { };

         int batchsize = 1;
         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;
            for( int i = 0; i < problem.getNRows( ); ++i )
               if( SCIPisConsroundAdmissible(problem, i))
                  ++batchsize;
            batchsize /= options.nbatches;
         }


         int nbatch = 0;
         for( int row = 0; row < copy.getNRows( ); ++row )
         {
            if( SCIPisConsroundAdmissible(copy, row))
            {
               auto data = copy.getConstraintMatrix( ).getRowCoefficients(row);

               for( int j = 0; j < data.getLength( ); ++j )
                  if( !num.isIntegral(data.getValues( )[ j ]))
                     batches_coeff.addEntry( row, data.getIndices( )[ j ], num.round(data.getValues( )[ j ]) );

               double lhs = copy.getConstraintMatrix( ).getLeftHandSides( )[ row ];
               double rhs = copy.getConstraintMatrix( ).getRightHandSides( )[ row ];
               if( solution_exists )
               {
                  if( !num.isIntegral(lhs))
                  {
                     double new_val = MIN(num.round(lhs), num.epsFloor(get_linear_activity(data, solution)));
                     batches_lhs.push_back({ row, new_val });
                     copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] = new_val;
                  }
                  if( !num.isIntegral(rhs))
                  {
                     double new_val = MIN(num.round(rhs), num.epsCeil(get_linear_activity(data, solution)));
                     batches_rhs.push_back({ row, new_val });
                     copy.getConstraintMatrix( ).getRightHandSides( )[ row ] = new_val;
                  }
               }
               else
               {
                  if( !num.isIntegral(lhs))
                  {
                     batches_lhs.push_back({ row, num.round(lhs) });
                     copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] = num.round(lhs);
                  }
                  if( !num.isIntegral(rhs))
                  {
                     batches_rhs.push_back({ row, num.round(rhs) });
                     copy.getConstraintMatrix( ).getLeftHandSides( )[ row ] = num.round(rhs);
                  }
               }
               ++nbatch;
            }
            //TODO: check if the change is working
            if(!batches_coeff.empty())
               copy.getConstraintMatrix().changeCoefficients(batches_coeff);

            if( nbatch >= 1 && ( nbatch >= batchsize || row >= copy.getNRows( ) - 1 ))
            {
               ScipInterface scipInterface { };
               //TODO pass settings to SCIP
               scipInterface.doSetUp(copy);
               if( scipInterface.runSCIP( ) != Status::kSuccess )
               {
                  copy = Problem<double>(problem);
                  SmallVec<int, 32> buffer;
                  if(!applied_entries.empty())
                     copy.getConstraintMatrix().changeCoefficients(applied_entries);
                  for( const auto &item: applied_reductions_lhs )
                     copy.getConstraintMatrix( ).getLeftHandSides( )[ item.first ] = item.second;
                  for( const auto &item: applied_reductions_rhs )
                     copy.getConstraintMatrix( ).getRightHandSides( )[ item.first ] = item.second;
               }
            }
            else
            {

               for( const auto &item: batches_lhs )
                  applied_reductions_lhs.emplace_back(item);
               for( const auto &item: batches_rhs )
                  applied_reductions_rhs.emplace_back(item);
               nchgsides += batches_lhs.size() + batches_rhs.size();
               SmallVec<int, 32> buffer;
               const MatrixEntry<double> *iter = batches_coeff.template begin<true>(buffer);
               while( iter != applied_entries.end( ))
               {
                  applied_entries.addEntry(iter->row, iter->col, iter->val);
                  nchgcoefs++;
               }
               batches_rhs.clear( );
               batches_lhs.clear( );
               batches_coeff.clear( );
               result = ModulStatus::kSuccessful;
            }
            nbatch = 0;
         }
         return result;
      }
   };


} // namespace bugger

#endif
