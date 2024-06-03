/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*                            MIP-DD                                         */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with MIP-DD; see the file LICENSE. If not visit scipopt.org.       */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _BUGGER_MISC_NUM_HPP_
#define _BUGGER_MISC_NUM_HPP_

#include "bugger/misc/MultiPrecision.hpp"


namespace bugger
{

template <typename T>
struct num_traits
{
   constexpr static bool is_rational =
       std::numeric_limits<T>::is_specialized == true &&
       std::numeric_limits<T>::is_integer == false &&
       std::numeric_limits<T>::is_exact == true &&
       std::numeric_limits<T>::min_exponent == 0 &&
       std::numeric_limits<T>::max_exponent == 0 &&
       std::numeric_limits<T>::min_exponent10 == 0 &&
       std::numeric_limits<T>::max_exponent10 == 0;

   constexpr static bool is_floating_point =
       std::numeric_limits<T>::is_specialized == true &&
       std::numeric_limits<T>::is_integer == false &&
       std::numeric_limits<T>::is_exact == false &&
       std::numeric_limits<T>::min_exponent != 0 &&
       std::numeric_limits<T>::max_exponent != 0 &&
       std::numeric_limits<T>::min_exponent10 != 0 &&
       std::numeric_limits<T>::max_exponent10 != 0;

   constexpr static bool is_integer =
       std::numeric_limits<T>::is_specialized == true &&
       std::numeric_limits<T>::is_integer == true &&
       std::numeric_limits<T>::is_exact == true &&
       std::numeric_limits<T>::min_exponent == 0 &&
       std::numeric_limits<T>::max_exponent == 0 &&
       std::numeric_limits<T>::min_exponent10 == 0 &&
       std::numeric_limits<T>::max_exponent10 == 0;
};

template <typename T>
class provides_numerator_and_denominator_overloads
{
   struct no
   {
   };

   template <typename T2>
   static decltype( numerator( std::declval<T2>() ) )
   test_numerator( int );

   template <typename T2>
   static decltype( numerator( std::declval<T2>() ) )
   test_denominator( int );

   template <typename T2>
   static no
   test_numerator( ... );

   template <typename T2>
   static no
   test_denominator( ... );

 public:
   constexpr static bool value =
       num_traits<decltype( test_numerator<T>( 0 ) )>::is_integer &&
       num_traits<decltype( test_denominator<T>( 0 ) )>::is_integer;
};

using std::min;
using std::max;
using std::abs;
using std::floor;
using std::ceil;
using std::round;
using std::copysign;
using std::exp;
using std::frexp;
using std::ldexp;
using std::log;
using std::log2;
using std::pow;
using std::sqrt;

template <typename REAL>
class Num
{
 public:
   Num()
       : zeta( REAL{ 0 } ), epsilon( REAL{ 1e-9 }), feastol( REAL{ 1e-6 } ),
         hugeval( REAL{ 1e8 } ) { }

   template <typename R1, typename R2>
   bool
   isZetaEq(const R1& a, const R2& b ) const
   {
      return abs( a - b ) <= zeta;
   }

   template <typename R1, typename R2>
   bool
   isEpsEq( const R1& a, const R2& b ) const
   {
      return abs( a - b ) <= epsilon;
   }

   template <typename R1, typename R2>
   bool
   isZetaGE(const R1& a, const R2& b ) const
   {
      return a - b >= -zeta;
   }

   template <typename R1, typename R2>
   bool
   isEpsGE(const R1& a, const R2& b ) const
   {
      return a - b >= -epsilon;
   }

   template <typename R1, typename R2>
   bool
   isZetaLE( const R1& a, const R2& b ) const
   {
      return a - b <= zeta;
   }

   template <typename R1, typename R2>
   bool
   isEpsLE( const R1& a, const R2& b ) const
   {
      return a - b <= epsilon;
   }


   template <typename R1, typename R2>
   bool
   isFeasLE( const R1& a, const R2& b ) const
   {
      return a - b <= feastol;
   }

   template <typename R1, typename R2>
   bool
   isZetaGT( const R1& a, const R2& b ) const
   {
      return a - b > zeta;
   }

   template <typename R1, typename R2>
   bool
   isEpsGT( const R1& a, const R2& b ) const
   {
      return a - b > epsilon;
   }

   template <typename R1, typename R2>
   bool
   isFeasGT( const R1& a, const R2& b ) const
   {
      return a - b > feastol;
   }


   template <typename R1, typename R2>
   bool
   isZetaLT(const R1& a, const R2& b ) const
   {
      return a - b < -zeta;
   }

   template <typename R1, typename R2>
   bool
   isEpsLT(const R1& a, const R2& b ) const
   {
      return a - b < -epsilon;
   }

   template <typename R1, typename R2>
   bool
   isFeasLT( const R1& a, const R2& b ) const
   {
      return a - b < -feastol;
   }

   template <typename R>
   REAL
   feasCeil( const R& a ) const
   {
      return ceil( REAL( a - feastol ) );
   }

   template <typename R>
   REAL
   epsCeil(const R& a ) const
   {
      return ceil( REAL( a - epsilon ) );
   }

   template <typename R>
   REAL
   zetaCeil(const R& a ) const
   {
      return ceil( REAL( a - zeta ) );
   }

   template <typename R>
   REAL
   feasFloor( const R& a ) const
   {
      return floor( REAL( a + feastol ) );
   }

   template <typename R>
   REAL
   zetaFloor(const R& a ) const
   {
      return floor( REAL( a + zeta ) );
   }

   template <typename R>
   REAL
   epsFloor(const R& a ) const
   {
      return floor( REAL( a + epsilon ) );
   }

   template <typename R>
   bool
   isZetaIntegral(const R& a ) const
   {
      return isZetaEq(a, round(a));
   }

   template <typename R>
   bool
   isEpsIntegral(const R& a ) const
   {
      return isEpsEq(a, round(a));
   }

   template <typename R>
   bool
   isFeasIntegral(const R& a ) const
   {
      return isFeasEq(a, round(a));
   }

   template <typename R>
   bool
   isEpsZero(const R& a ) const
   {
      return abs( a ) <= epsilon;
   }

   template <typename R>
   bool
   isZetaZero(const R& a ) const
   {
      return abs( a ) <= zeta;
   }


   template <typename R>
   bool
   isFeasZero( const R& a ) const
   {
      return abs( a ) <= feastol;
   }


   const REAL&
   getEpsilon() const
   {
      return epsilon;
   }

   const REAL&
   getFeasTol() const
   {
      return feastol;
   }

   const REAL&
   getZeta() const
   {
      return zeta;
   }

   const REAL&
   getHugeVal() const
   {
      return hugeval;
   }

   template <typename R>
   bool
   isHugeVal( const R& a ) const
   {
      return abs( a ) >= hugeval;
   }

   void
   setEpsilon( REAL value )
   {
      assert( value >= 0 );
      this->epsilon = value;
   }

   void
   setZeta( REAL value )
   {
      assert( value >= 0 );
      this->zeta = value;
   }

   void
   setFeasTol( REAL value )
   {
      assert( value >= 0 );
      this->feastol = value;
   }

   void
   setHugeVal( REAL value )
   {
      assert( value >= 0 );
      this->hugeval = value;
   }


 private:
   REAL epsilon;
   REAL feastol;
   REAL zeta;
   REAL hugeval;
};

} // namespace bugger

#endif
