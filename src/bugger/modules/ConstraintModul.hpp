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

#ifndef BUGGER_MODUL_CONSTRAINT_HPP_
#define BUGGER_MODUL_CONSTRAINT_HPP_

#include "bugger/modules/BuggerModul.hpp"

#if BUGGER_HAVE_SCIP

#include "scip/var.h"
#include "scip/scip_sol.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/def.h"

#endif
namespace bugger {

   class ConstraintModul : public BuggerModul {
   public:
      ConstraintModul(const Message &_msg) : BuggerModul( ) {
         this->setName("constraint");
         this->msg = _msg;
      }

      bool
      initialize( ) override {
         return false;
      }

      bool isConstraintAdmissible(const Problem<double> problem, int row) {
         if( problem.getConstraintMatrix( ).getRowFlags( )[ row ].test(RowFlag::kRedundant))
            return false;
         return true;
      }

      ModulStatus
      execute(Problem<double> &problem, Solution<double> &solution, bool solution_exists, const BuggerOptions &options,
              const Timer &timer) override {

         ModulStatus result = ModulStatus::kUnsuccesful;

         auto copy = Problem<double>(problem);
         Vec<int> applied_redundant_rows { };
         Vec<int> batches { };
         int batchsize = 1;

         if( options.nbatches > 0 )
         {
            batchsize = options.nbatches - 1;

            for( int i = problem.getNRows( ) - 1; i >= 0; --i )
               if( isConstraintAdmissible(problem, i))
                  ++batchsize;
            batchsize /= options.nbatches;
         }

         int nbatch = 0;

         for( int row = copy.getNRows( ) - 1; row >= 0; --row )
         {
            if( isConstraintAdmissible(copy, row))
            {
               assert(!copy.getRowFlags( )[ row ].test(RowFlag::kRedundant));
               copy.getRowFlags( )[ row ].set(RowFlag::kRedundant);
               assert(copy.getRowFlags( )[ row ].test(RowFlag::kRedundant));

               batches.push_back(row);

               if( nbatch >= 1 && ( nbatch >= batchsize || row <= 0 ))
               {
                  ScipInterface scipInterface { };
                  //TODO pass settings to SCIP
                  scipInterface.doSetUp(copy);
                  if( scipInterface.run(msg) != Status::kSuccess )
                  {
                     copy = Problem<double>(problem);
                     for( const auto &item: applied_redundant_rows )
                     {
                        assert(!copy.getRowFlags( )[ item ].test(RowFlag::kRedundant));
                        copy.getRowFlags( )[ item ].set(RowFlag::kRedundant);
                     }
                  }
                  else
                  {
                     //TODO: push back together
                     for( const auto &item: batches )
                        applied_redundant_rows.push_back(item);
                     ndeletedrows += nbatch;
                     batches.clear( );
                     result = ModulStatus::kSuccessful;
                  }
                  nbatch = 0;
               }
               ++nbatch;
            }

         }

         problem = Problem<double>(copy);
         //TODO: remove the constraints from the problem might be ideal at least at the end
         return result;
      }
   };


} // namespace bugger

#endif
