/****************************************************************************
* Copyright (c) 2018, CEA
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
// File:        Mod_turb_hyd_RANS.cpp
// Directory:   $TURBULENCE_ROOT/src/ThHyd/Modeles_Turbulence/RANS/Hydr
//
//////////////////////////////////////////////////////////////////////////////

#include <Mod_turb_hyd_RANS.h>
#include <Transport_K_Eps_base.h>
#include <TRUSTTrav.h>
#include <Param.h>

Implemente_base_sans_constructeur(Mod_turb_hyd_RANS,"Mod_turb_hyd_RANS",Mod_turb_hyd_base);
// XD mod_turb_hyd_rans modele_turbulence_hyd_deriv mod_turb_hyd_rans -1 Class for RANS turbulence model for Navier-Stokes equations.

Mod_turb_hyd_RANS::Mod_turb_hyd_RANS()
{
  Prandtl_K = 1.;
  Prandtl_Eps = 1.3;
  LeEPS_MIN = 1.e-20  ;
  LeEPS_MAX = 1.e+10;
  LeK_MIN = 1.e-20;
  lquiet = 0;
}
/*! @brief Simple appel a Mod_turb_hyd_base::printOn(Sortie&)
 *
 * @param (Sortie& is) un flot de sortie
 * @return (Sortie&) le flot de sortie modifie
 */
Sortie& Mod_turb_hyd_RANS::printOn(Sortie& is) const
{
  return Mod_turb_hyd_base::printOn(is);
}


/*! @brief Simple appel a Mod_turb_hyd_base::readOn(Entree&)
 *
 * @param (Entree& is) un flot d'entree
 * @return (Entree&) le flot d'entree modifie
 */
Entree& Mod_turb_hyd_RANS::readOn(Entree& is)
{
  Mod_turb_hyd_base::readOn(is);
  return is;
}
void Mod_turb_hyd_RANS::set_param(Param& param)
{
  Mod_turb_hyd_base::set_param(param);
  param.ajouter("eps_min",&LeEPS_MIN); // XD_ADD_P double Lower limitation of epsilon (default value 1.e-10).
  param.ajouter("eps_max",&LeEPS_MAX); // XD_ADD_P double Upper limitation of epsilon (default value 1.e+10).
  param.ajouter("k_min",&LeK_MIN); // XD_ADD_P double Lower limitation of k (default value 1.e-10).
  param.ajouter_flag("quiet",&lquiet); // XD_ADD_P flag To disable printing of information about k and epsilon.
}


/*! @brief
 *
 */
void Mod_turb_hyd_RANS::completer()
{
  eqn_transp_K_Eps().completer();
  verifie_loi_paroi();
}

void Mod_turb_hyd_RANS::verifie_loi_paroi()
{
  Nom lp=loipar.valeur().que_suis_je();
  if (lp=="negligeable_VEF" || lp=="negligeable_VDF")
    {
      Cerr<<"The turbulence model of type "<<que_suis_je()<<finl;
      Cerr<<"must not be used with a wall law of type negligeable."<<finl;
      Cerr<<"Another wall law must be selected with this kind of turbulence model."<<finl;
      exit();
    }
}

const Champ_base& Mod_turb_hyd_RANS::get_champ(const Motcle& nom) const
{

  try
    {
      return Mod_turb_hyd_base::get_champ(nom);
    }
  catch (Champs_compris_erreur)
    {
    }
  int i;
  int nb_eq = nombre_d_equations();
  for (i=0; i<nb_eq; i++)
    {
      try
        {
          return equation_k_eps(i).get_champ(nom);
        }
      catch (Champs_compris_erreur)
        {
        }
    }
  throw Champs_compris_erreur();
  REF(Champ_base) ref_champ;
  return ref_champ;

  //return champs_compris_.get_champ(nom);
}
void Mod_turb_hyd_RANS::get_noms_champs_postraitables(Noms& nom,Option opt) const
{
  Mod_turb_hyd_base::get_noms_champs_postraitables(nom,opt);

  int i;
  int nb_eq = nombre_d_equations();
  for (i=0; i<nb_eq; i++)
    {
      equation_k_eps(i).get_noms_champs_postraitables(nom,opt);
    }
}

/*! @brief Sauvegarde le modele de turbulence sur un flot de sortie.
 *
 * (en vue d'une reprise)
 *     Sauvegarde le type de l'objet et
 *     l'equation de transport K-epsilon associee.
 *
 * @param (Sortie& os) un flot de sortie
 * @return (int) code de retour propage de: Transport_K_Eps::sauvegarder(Sortie&)
 */
int Mod_turb_hyd_RANS::sauvegarder(Sortie& os) const
{

  Mod_turb_hyd_base::sauvegarder(os);
  return eqn_transp_K_Eps().sauvegarder(os);
}


/*! @brief Reprise du modele a partir d'un flot d'entree.
 *
 * Si l'equation portee par l'objet est non nulle
 *     on effectue une reprise "bidon".
 *
 * @param (Entree& is) un flot d'entree
 * @return (int) code de retour propage de: Transport_K_Eps::sauvegarder(Sortie&) ou 1 si la reprise est bidon.
 */
int Mod_turb_hyd_RANS::reprendre(Entree& is)
{
  Mod_turb_hyd_base::reprendre(is);
  if (mon_equation.non_nul())
    {
      return eqn_transp_K_Eps().reprendre(is);
    }
  else
    {
      double dbidon;
      Nom bidon;
      DoubleTrav tab_bidon;
      is >> bidon >> bidon;
      is >> dbidon;
      tab_bidon.jump(is);
      return 1;
    }
}

