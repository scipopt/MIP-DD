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
      SCIP *scip;
      bool exists_sol;
      SCIP_Sol* solution;

   public:
      ScipInterface( ) : scip(nullptr) {
         if( SCIPcreate(&scip) != SCIP_OKAY )
            throw std::runtime_error("could not create SCIP");
      }

      SCIP* getSCIP(){ return scip; }
      SCIP_Sol* get_solution(){return solution;}
      bool exists_solution(){return exists_sol;}


      void
      parse(const std::string& filename)
      {
         assert(!filename.empty());
         SCIP_CALL_ABORT(SCIPincludeDefaultPlugins(scip));
         SCIP_CALL_ABORT(SCIPreadProb(scip, filename.c_str(), nullptr));
      }

      void
      read_parameters(const std::string& scip_settings_file)
      {
         if(!scip_settings_file.empty())
            SCIP_CALL_ABORT( SCIPreadParams( scip, scip_settings_file.c_str() ) );
      }

      void
      read_solution(const std::string& solution_file)
      {
         if(!solution_file.empty())
         {
            SCIP_CALL_ABORT(SCIPreadSol(scip, solution_file.c_str( )));
            exists_sol = true;
            solution = SCIPgetBestSol(scip);
         }
      }

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
               reference = SCIPgetSolOrigObj(scip, sol);
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
