/****************************************************************************
* Copyright (c) 2021, CEA
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
// File:        Source_Gravite_PF_VDF.h
// Directory:   $TRUST_ROOT/../Composants/TrioCFD/Multiphase/Phase_field/src/VDF
// Version:     /main/10
//
//////////////////////////////////////////////////////////////////////////////


#ifndef Source_Gravite_PF_VDF_included
#define Source_Gravite_PF_VDF_included

#include <Source_base.h>
#include <Ref_Zone_VDF.h>
#include <Ref_Zone_Cl_VDF.h>
#include <Ref_Probleme_base.h>


/*! @brief class  Source_Gravite_PF_VDF
 *
 *  Cette classe represente le terme source de gravite dans l equation Navier_Stokes_phase_field,
 *   pour une discretisation VDF.
 *
 *
 * @sa Source_base
 */
class Source_Gravite_PF_VDF : public Source_base
{

  Declare_instanciable(Source_Gravite_PF_VDF);

public:
  DoubleTab& calculer(DoubleTab& ) const override;
  inline void associer_pb(const Probleme_base& ) override;
  DoubleTab& ajouter(DoubleTab& ) const override;
  void mettre_a_jour(double temps) override { }

protected :
  void associer_zones(const Zone_dis& zone,const Zone_Cl_dis& ) override;
  REF(Zone_VDF) la_zone;
  REF(Zone_Cl_VDF) la_zone_Cl;
  REF(Probleme_base) le_probleme;
};

inline void Source_Gravite_PF_VDF::associer_pb(const Probleme_base& pb )
{
  le_probleme=pb;
}

#endif
