/****************************************************************************
* Copyright (c) 2015, CEA
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*****************************************************************************/
//////////////////////////////////////////////////////////////////////////////
//
// File:        RK3_FT.h
// Directory:   $TRUST_ROOT/src/Front_tracking_discontinu
// Version:     /main/11
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RK3_FT_included
#define RK3_FT_included

#include <Deriv.h>
#include <RK3.h>
#include <Vect_Ref_Probleme_base.h>
#include <Probleme_Couple.h>


//////////////////////////////////////////////////////////////////////////////
//
// .DESCRIPTION
//     classe RK3
//     Cette classe represente un schema en temps de Runge Kutta d'ordre 3
//     (cas 7 de Williamson) s'ecrit :
//     q1=h f(x0)
//     x1=x0+b1 q1
//     q2=h f(x1) +a2 q1
//     x2=x1+b2 q2
//     q3=h f(x2)+a3 q2
//     x3=x2+b3 q3
//      avec a1=0, a2=-5/9, a3=-153/128
//                              b1=1/3, b2=15/16, b3=8/15
// .SECTION voir aussi
//     Schema_Temps_base
//////////////////////////////////////////////////////////////////////////////
class RK3_FT: public RK3
{

  Declare_instanciable(RK3_FT);
public :

  ////////////////////////////////
  //                            //
  // Caracteristiques du schema //
  //                            //
  ////////////////////////////////

  virtual int nb_valeurs_temporelles() const;
  virtual int nb_valeurs_futures() const;
  virtual double temps_futur(int i) const;
  virtual double temps_defaut() const;

  /////////////////////////////////////////
  //                                     //
  // Fin des caracteristiques du schema  //
  //                                     //
  /////////////////////////////////////////

  virtual int faire_un_pas_de_temps_eqn_base(Equation_base&);
  virtual bool iterateTimeStep(bool& converged);
  inline void completer();
  int faire_un_pas_de_temps_pb_couple(Probleme_Couple&);

};

inline void RK3_FT::completer()
{
  /*   Cerr << "*** On passe dans Euler_Explicite::completer() ***" << finl; */
}
#endif
