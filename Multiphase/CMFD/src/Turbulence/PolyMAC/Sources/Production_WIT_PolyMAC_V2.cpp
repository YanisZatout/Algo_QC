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
// File:        Production_WIT_PolyMAC_V2.cpp
// Directory:   $TRUST_ROOT/src/ThHyd/Multiphase/PolyMAC_V2
// Version:     /main/13
//
//////////////////////////////////////////////////////////////////////////////

#include <Production_WIT_PolyMAC_V2.h>

#include <Zone_PolyMAC_V2.h>
#include <Champ_P0_PolyMAC_V2.h>
#include <Matrix_tools.h>
#include <Pb_Multiphase.h>
#include <grad_Champ_Face_PolyMAC_V2.h>
#include <Champ_Uniforme.h>
#include <Flux_interfacial_base.h>
#include <Milieu_composite.h>
#include <Operateur_Diff.h>
#include <Op_Diff_Turbulent_PolyMAC_V2_Face.h>
#include <Navier_Stokes_std.h>
#include <Viscosite_turbulente_base.h>
#include <Viscosite_turbulente_multiple.h>
#include <ConstDoubleTab_parts.h>


Implemente_instanciable(Production_WIT_PolyMAC_V2,"Production_WIT_P0_PolyMAC_V2", Source_base);

Sortie& Production_WIT_PolyMAC_V2::printOn(Sortie& os) const
{
  return os;
}

Entree& Production_WIT_PolyMAC_V2::readOn(Entree& is)
{
  Param param(que_suis_je());
  param.ajouter("Reb_c", &Reb_c_);
  param.ajouter("g", &g_);
  param.lire_avec_accolades_depuis(is);

  Pb_Multiphase *pbm = sub_type(Pb_Multiphase, equation().probleme()) ? &ref_cast(Pb_Multiphase, equation().probleme()) : NULL;

  if (!pbm || pbm->nb_phases() == 1) Process::exit(que_suis_je() + " : not needed for single-phase flow!");
  for (int n = 0; n < pbm->nb_phases(); n++) //recherche de n_l, n_g : phase {liquide,gaz}_continu en priorite
    if (pbm->nom_phase(n).debute_par("liquide") && (n_l < 0 || pbm->nom_phase(n).finit_par("continu")))  n_l = n;
  if (n_l < 0) Process::exit(que_suis_je() + " : liquid phase not found!");

  return is;
}

void Production_WIT_PolyMAC_V2::dimensionner_blocs(matrices_t matrices, const tabs_t& semi_impl) const
{
// empty : no derivative to add in the blocks
}

void Production_WIT_PolyMAC_V2::ajouter_blocs(matrices_t matrices, DoubleTab& secmem, const tabs_t& semi_impl) const
{
  const Zone_PolyMAC_V2&                      zone = ref_cast(Zone_PolyMAC_V2, equation().zone_dis().valeur());
  const DoubleTab&                      tab_rho = equation().probleme().get_champ("masse_volumique").passe();
  const DoubleTab&                      tab_alp = equation().probleme().get_champ("alpha").passe();
  const DoubleTab&                          vit = equation().probleme().get_champ("vitesse").passe();
  const DoubleTab&                         diam = equation().probleme().get_champ("diametre_bulles").valeurs();
  const DoubleTab&                           nu = equation().probleme().get_champ("viscosite_cinematique").passe();

  const DoubleVect& pe = zone.porosite_elem(), &ve = zone.volumes();

  int Nk = equation().inconnue().valeurs().dimension(1), N = ref_cast(Pb_Multiphase, equation().probleme()).nb_phases(), ne = zone.nb_elem(), nf_tot = zone.nb_faces_tot(), D = dimension ;
  if (Nk!=1) Process::exit("WIT is only in the liquid phase");
  if (D!=3) Process::exit("WIT is only coded for 3 dimensions");

  // On calcule le second membre aux elements (implicite uniquement pour le moment)
  for(int e = 0 ; e < ne ; e++) for (int k = 0 ; k<N ; k++) if (k!=n_l)
        {
          double u_r = 0;
          for (int d = 0; d < D; d++) u_r += (vit(nf_tot + D*e+d, k) - vit(nf_tot + D*e+d, n_l))*(vit(nf_tot + D*e+d, k) - vit(nf_tot + D*e+d, n_l)); // relative speed = gas speed - liquid speed
          u_r = std::sqrt(u_r);
          double Reb = diam(e,k)*u_r/nu(e,n_l);
          secmem(e, 0) += ve(e) * pe(e) * tab_alp(e, n_l)* tab_rho(e, n_l) * tab_alp(e, k) *(tab_rho(e, n_l)-tab_rho(e, k))/tab_rho(e, n_l)*g_*u_r*(0.9 - exp(-Reb/Reb_c_));
        }
}
