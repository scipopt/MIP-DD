/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*               This file is part of the program and library                */
/*    BUGGER                                                                 */
/*                                                                           */
/* Copyright (C) 2024             Zuse Institute Berlin                           */
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

#ifndef _BUGGER_MISC_NUM_HPP_
#define _BUGGER_MISC_NUM_HPP_

#include "bugger/misc/ParameterSet.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

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

template <typename Rational,
          typename std::enable_if<
              num_traits<Rational>::is_rational &&
                  provides_numerator_and_denominator_overloads<Rational>::value,
              int>::type = 0>
Rational
floor( const Rational& x, ... )
{
   if( x >= 0 )
      return numerator( x ) / denominator( x );

   if( numerator( x ) < 0 )
      return -1 + ( numerator( x ) + 1 ) / denominator( x );

   return -1 + ( numerator( x ) - 1 ) / denominator( x );
}

template <typename Rational,
          typename std::enable_if<
              num_traits<Rational>::is_rational &&
                  provides_numerator_and_denominator_overloads<Rational>::value,
              int>::type = 0>
Rational
ceil( const Rational& x, ... )
{
   if( x <= 0 )
      return numerator( x ) / denominator( x );

   if( numerator( x ) < 0 )
      return 1 + ( numerator( x ) + 1 ) / denominator( x );

   return 1 + ( numerator( x ) - 1 ) / denominator( x );
}

using std::abs;
using std::ceil;
using std::copysign;
using std::exp;
using std::floor;
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
       : zeta( REAL{ 0 } ), epsilon( REAL{ 1e-9}), feastol( REAL{ 1e-6 } ),
         hugeval( REAL{ 1e8 } )
   {
   }

   template <typename R>
   static constexpr REAL
   round( const R& x )
   {
      return floor( REAL( x + REAL( 0.5 ) ) );
   }

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

   template <typename R1, typename R2>
   static REAL
   max( const R1& a, const R2& b )
   {
      return a > b ? REAL( a ) : REAL( b );
   }

   template <typename R1, typename R2>
   static REAL
   min( const R1& a, const R2& b )
   {
      return a < b ? REAL( a ) : REAL( b );
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
