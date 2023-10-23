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

#ifndef _BUGGER_INTERFACES_SCIP_INTERFACE_HPP_
#define _BUGGER_INTERFACES_SCIP_INTERFACE_HPP_

#define UNUSED(expr) do { (void)(expr); } while (0)

#include "bugger/misc/Vec.hpp"
#include <cassert>
#include <stdexcept>

#include "bugger/data/Problem.hpp"
#include "bugger/data/ProblemBuilder.hpp"
#include "scip/cons_linear.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_paramset.h"
#include "Status.hpp"

namespace bugger {

   #define SCIP_CALL_RETURN(x)                                        \
   do                                                                 \
   {                                                                  \
      SCIP_RETCODE _restat_;                                          \
      if( (_restat_ = (x)) != SCIP_OKAY )                             \
      {                                                               \
         SCIPerrorMessage("Error <%d> in function call\n", _restat_); \
         return Status::kErrorDuringSCIP;                             \
      }                                                               \
   }                                                                  \
   while( FALSE )


   class ScipInterface {
   private:
      SCIP* scip;
      SCIP_Sol* solution;
      Vec<SCIP_VAR*> vars;


   public:
      ScipInterface( ) : scip(nullptr) {
         if( SCIPcreate(&scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }


      SCIP_RETCODE
      doSetUp( const Problem<double>& problem )
      {
         SCIP_CALL(SCIPincludeDefaultPlugins(scip));
         int ncols = problem.getNCols();
         int nrows = problem.getNRows();
         const Vec<String>& varNames = problem.getVariableNames();
         const Vec<String>& consNames = problem.getConstraintNames();
         const VariableDomains<double>& domains = problem.getVariableDomains();
         const Objective<double>& obj = problem.getObjective();
         const auto& consMatrix = problem.getConstraintMatrix();
         const auto& lhs_values = consMatrix.getLeftHandSides();
         const auto& rhs_values = consMatrix.getRightHandSides();
         const auto& rflags = problem.getRowFlags();

         SCIP_CALL( SCIPcreateProbBasic( scip, problem.getName().c_str() ) );

         vars.resize( problem.getNCols() );

         for( int i = 0; i < ncols; ++i )
         {
            SCIP_VAR* var;
            assert( !domains.flags[i].test( ColFlag::kInactive ) );

            SCIP_Real lb = domains.flags[i].test( ColFlag::kLbInf )
                           ? -SCIPinfinity( scip )
                           : SCIP_Real( domains.lower_bounds[i] );
            SCIP_Real ub = domains.flags[i].test( ColFlag::kUbInf )
                           ? SCIPinfinity( scip )
                           : SCIP_Real( domains.upper_bounds[i] );
            SCIP_VARTYPE type;
            if( domains.flags[i].test( ColFlag::kIntegral ) )
            {
               if( lb == 0  && ub ==  1 )
                  type = SCIP_VARTYPE_BINARY;
               else
                  type = SCIP_VARTYPE_INTEGER;
            }
            else if( domains.flags[i].test( ColFlag::kImplInt ) )
               type = SCIP_VARTYPE_IMPLINT;
            else
               type = SCIP_VARTYPE_CONTINUOUS;

            SCIP_CALL( SCIPcreateVarBasic(
                  scip, &var, varNames[i].c_str(), lb, ub,
                  SCIP_Real( obj.coefficients[i] ), type ) );
            SCIP_CALL( SCIPaddVar( scip, var ) );
            vars[i] = var;

            SCIP_CALL( SCIPreleaseVar( scip, &var ) );
         }

         Vec<SCIP_VAR*> consvars;
         Vec<SCIP_Real> consvals;
         consvars.resize( problem.getNCols() );
         consvals.resize( problem.getNCols() );

         for( int i = 0; i < nrows; ++i )
         {
            SCIP_CONS* cons;

            auto rowvec = consMatrix.getRowCoefficients( i );
            const double* vals = rowvec.getValues();
            const int* inds = rowvec.getIndices();
            SCIP_Real lhs = rflags[i].test( RowFlag::kLhsInf )
                            ? -SCIPinfinity( scip )
                            : SCIP_Real( lhs_values[i] );
            SCIP_Real rhs = rflags[i].test( RowFlag::kRhsInf )
                            ? SCIPinfinity( scip )
                            : SCIP_Real( rhs_values[i] );

            for( int k = 0; k != rowvec.getLength(); ++k )
            {
               consvars[k] = vars[inds[k]];
               consvals[k] = SCIP_Real( vals[k] );
            }

            SCIP_CALL( SCIPcreateConsBasicLinear(
                  scip, &cons, consNames[i].c_str(), rowvec.getLength(),
                  consvars.data(), consvals.data(), lhs, rhs ) );
            SCIP_CALL( SCIPaddCons( scip, cons ) );
            SCIP_CALL( SCIPreleaseCons( scip, &cons ) );
         }


         if( obj.offset !=  0 )
            SCIP_CALL( SCIPaddOrigObjoffset( scip, SCIP_Real( obj.offset ) ) );

         return SCIP_OKAY;
      }

      SCIP* getSCIP(){ return scip; }


   /** tests the given SCIP instance in a copy and reports detected bug; if a primal bug solution is provided, the
    *  resulting dual bound is also checked; on UNIX platforms aborts are caught, hence assertions can be enabled here
    */
      Status runSCIP() {
         SCIP *test = nullptr;
         SCIP_HASHMAP *varmap = nullptr;
         SCIP_HASHMAP *consmap = nullptr;
         int i;

         //TODO: overload with command line paramter
#ifdef CATCH_ASSERT_BUG
         if( fork() == 0 )
      exit(trySCIP(iscip.getSCIP(), iscip.get_solution(), &test, &varmap, &consmap));
   else
   {
      int status;

      wait(&status);

      if( WIFEXITED(status) )
         retcode = WEXITSTATUS(status);
      else if( WIFSIGNALED(status) )
      {
         retcode = WTERMSIG(status);

         if( retcode == SIGINT )
            retcode = 0;
      }
   }
#else
         Status retcode = trySCIP( &test, &varmap, &consmap);
#endif

         if( test != nullptr )
            SCIPfree(&test);
         if( consmap != nullptr )
            SCIPhashmapFree(&consmap);
         if( varmap != nullptr )
            SCIPhashmapFree(&varmap);
         SCIPinfoMessage(scip, nullptr, "\n");

         // TODO: what are passcodes doing?
//         for( i = 0; i < presoldata->npasscodes; ++i )
//            if( retcode == presoldata->passcodes[i] )
//               return 0;

         return retcode;
      }

      /** creates a SCIP instance test, variable map varmap, and constraint map consmap, copies setting, problem, and
       *  solutions apart from the primal bug solution, tries to solve, and reports detected bug
       */
      Status trySCIP(SCIP **test, SCIP_HASHMAP **varmap, SCIP_HASHMAP **consmap) {
         SCIP_Real reference = SCIPgetObjsense(scip) * SCIPinfinity(scip);
         SCIP_Bool valid = FALSE;
         SCIP_SOL **sols;
         int nsols;
         int i;

         sols = SCIPgetSols(scip);
         nsols = SCIPgetNSols(scip);
         SCIP_CALL_RETURN(SCIPhashmapCreate(varmap, SCIPblkmem(scip), SCIPgetNVars(scip)));
         SCIP_CALL_RETURN(SCIPhashmapCreate(consmap, SCIPblkmem(scip), SCIPgetNConss(scip)));
         SCIP_CALL_RETURN(SCIPcreate(test));
#ifndef SCIP_DEBUG
         SCIPsetMessagehdlrQuiet(*test, TRUE);
#endif
         SCIP_CALL_RETURN(SCIPincludeDefaultPlugins(*test));
         SCIP_CALL_RETURN(SCIPcopyParamSettings(scip, *test));
         SCIP_CALL_RETURN(SCIPcopyOrigProb(scip, *test, *varmap, *consmap, SCIPgetProbName(scip)));
         SCIP_CALL_RETURN(SCIPcopyOrigVars(scip, *test, *varmap, *consmap, NULL, NULL, 0));
         SCIP_CALL_RETURN(SCIPcopyOrigConss(scip, *test, *varmap, *consmap, FALSE, &valid));
         //TODO: fix this
//         (*test)->stat->subscipdepth = 0;

         if( !valid )
            SCIP_CALL_ABORT(SCIP_INVALIDDATA);

         for( i = 0; i < nsols; ++i )
         {
            SCIP_SOL *sol;

            sol = sols[ i ];


            if( sol == solution )
            {
               //TODO: parse Solution<REAL> solution to SCIP
//               reference = SCIPgetSolOrigObj(scip, sol);
            }
            else
            {
               SCIP_SOL *initsol = NULL;

               SCIP_CALL_RETURN(SCIPtranslateSubSol(*test, scip, sol, NULL, SCIPgetOrigVars(scip), &initsol));

               if( initsol == NULL )
                  SCIP_CALL_ABORT(SCIP_INVALIDCALL);

               SCIP_CALL_RETURN(SCIPaddSolFree(*test, &initsol, &valid));
            }
         }

         SCIP_CALL_RETURN(SCIPsolve(*test));

         if( SCIPisSumNegative(scip, SCIPgetObjsense(*test) * ( reference - SCIPgetDualbound(*test))))
            return Status::kFail;

         return Status::kSuccess;
      }



      ~ScipInterface( ) {
         if( scip != nullptr )
         {
            SCIP_RETCODE retcode = SCIPfree(&scip);
            UNUSED(retcode);
            assert(retcode == SCIP_OKAY);
         }
      }

   };

} // namespace bugger

#endif
