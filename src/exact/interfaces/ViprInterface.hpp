/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    PaPILO --- Parallel Presolve for Integer and Linear Optimization       */
/*                                                                           */
/* Copyright (C) 2020-2023 Konrad-Zuse-Zentrum                               */
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

#ifndef VIPR_INTERFACES_SCIP_INTERFACE_HPP_
#define VIPR_INTERFACES_SCIP_INTERFACE_HPP_


#include "exact/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "exact/data/Problem.hpp"

namespace exact {

   class ViprInterface {

   private:
      std::ofstream proof_out;
      int counter;

   public:

      ViprInterface(const Problem<exact::Rational> &problem) {
         const auto &problem_name = problem.getName( );
         int length = problem_name.length( );
         int ending = 4;
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB
         if( problem_name.substr(length - 3) == ".gz" )
            ending = 7;
#endif
#ifdef BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2
         if( problem_name.substr(length - 4) == ".bz2" )
            ending = 8;
#endif
         proof_out = std::ofstream(problem_name.substr(0, length - ending) + ".vipr");
         counter = 0;
      }

      void
      init_vipr_certificate(const Problem<exact::Rational> &problem) {
         int nvars = problem.getNCols( );
         int nboundconss = 0;
         int non_zero_obj = 0;
         int nintvars = 0;
         auto domains = problem.getVariableDomains( );

         for( int var = 0; var < nvars; var++ )
         {
            if( !domains.flags[ var ].test(ColFlag::kLbInf))
               nboundconss++;
            if( !domains.flags[ var ].test(ColFlag::kUbInf))
               nboundconss++;
            if( domains.flags[ var ].test(ColFlag::kIntegral))
               nintvars++;
            if( problem.getObjective( ).coefficients[ var ] != 0 )
               non_zero_obj++;
         }

         proof_out << "VER 1.0\n";
         proof_out << "VAR " << nvars << "\n";
         for( int var = 0; var < nvars; var++ )
         {
            const char *varname = problem.getVariableNames( )[ var ].c_str( );
            //TODO: SCIPvarSetCertificateIndex(vars[ var ], var);
            //      SCIPvarSetCertificateIndex(SCIPvarGetTransVar(vars[ var ]), var);
            if( strstr(varname, " ") != NULL || strstr(varname, "\t") != NULL || strstr(varname, "\n") != NULL
                || strstr(varname, "\v") != NULL || strstr(varname, "\f") != NULL || strstr(varname, "\r") != NULL )
            {
               SCIPerrorMessage(
                     "Variable name <%s> cannot be printed to certificate file because it contains whitespace.\n",
                     varname);
               return;
            }
            proof_out << varname << "\n";
         }
         proof_out << "INT " << nintvars << "\n";
         for( int var = 0; var < nvars; var++ )
            if( domains.flags[ var ].test(ColFlag::kIntegral))
               proof_out << var << "\n";

         proof_out << "OBJ min\n" << non_zero_obj;

         for( int var = 0; var < nvars; var++ )
            if( problem.getObjective( ).coefficients[ var ] != 0 )
               proof_out << " " << var << " " << problem.getObjective( ).coefficients[ var ];
         proof_out << "\n";
         int ncertcons = 0;
         for( int cons = 0; cons < problem.getNRows( ); cons++ )
         {
            if( !problem.getRowFlags( )[ cons ].test(RowFlag::kLhsInf))
               ncertcons++;
            if( !problem.getRowFlags( )[ cons ].test(RowFlag::kRhsInf))
               ncertcons++;
         }

         proof_out << "CON " << ncertcons + nboundconss << " " << nboundconss << "\n";

         for( int var = 0; var < nvars; var++ )
         {
            if( !domains.flags[ var ].test(ColFlag::kLbInf))
            {
               proof_out << "B" << counter << " G "
                         << domains.lower_bounds[ var ] << " " << 1 << " " << var << " " << 1 << "\n";
               counter++;
            }
            if( !domains.flags[ var ].test(ColFlag::kUbInf))
            {
               proof_out << "B" << counter << " L "
                         << domains.upper_bounds[ var ] << " " << 1 << " " << var << " " << 1 << "\n";
               counter++;
            }
         }
         for( int cons = 0; cons < problem.getNRows( ); cons++ )
         {
            auto cons_data = problem.getConstraintMatrix( ).getRowCoefficients(cons);
            const int *indices = cons_data.getIndices( );
            const Rational *values = cons_data.getValues( );
            if( !problem.getRowFlags( )[ cons ].test(RowFlag::kLhsInf))
            {
               proof_out << "C" << counter << " G " << problem.getConstraintMatrix( ).getLeftHandSides( )[ cons ]
                         << " " << cons_data.getLength( );
               for( int i = 0; i < cons_data.getLength( ); i++ )
                  proof_out << " " << indices[ i ] << " " << values[ i ];
               proof_out << "\n";
               counter++;
            }
            if( !problem.getRowFlags( )[ cons ].test(RowFlag::kRhsInf))
            {
               proof_out << "C" << counter << " G " << problem.getConstraintMatrix( ).getRightHandSides( )[ cons ]
                         << " " << cons_data.getLength( );
               for( int i = 0; i < cons_data.getLength( ); i++ )
                  proof_out << " " << indices[ i ] << " " << values[ i ];
               proof_out << "\n";
               counter++;
            }
         }
      }

      void write_rtp_range(const Rational &lower, const Rational &upper) {
         proof_out << "RTP range " << lower << " " << upper << "\n";
      }

      void write_assumption(const int variable_index, const Rational &value, bool less) {
         proof_out << "A" << counter << " ";
         if(less)
            proof_out << "L" << " ";
         else
            proof_out << "G" << " ";
         proof_out << value << " " <<  1 << " "  << variable_index << " 1 { asm } -1 \n";
      }

      void
      write_solutions(Vec<std::pair<Vec<Rational>, Rational>> &solutions, bool optimum_found, Rational optimal_value) {
         proof_out << "SOL " << solutions.size( ) << "\n";

         for( const auto &solution: solutions )
         {
            int nonzeros = 0;
            for( const auto &val: solution.first )
               if( val != 0 )
                  nonzeros++;
            if( optimum_found && optimal_value == solution.second )
               proof_out << "best " << nonzeros;
            else
               proof_out << "feas " << nonzeros;
            for( int i = 0; i < solution.first.size( ); i++ )
               if( solution.first[ i ] != 0 )
                  proof_out << " " << i << " " << solution.first[ i ];
            proof_out << "\n";
         }
      }

      void write_derivations( ) {
         proof_out << "DER " << 0 << "\n";
      }
   };

} // namespace exact

#endif
