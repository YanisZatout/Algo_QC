/****************************************************************************
* Copyright (c) 2019, CEA
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
// File:        Navier_Stokes_Turbulent.cpp
// Directory:   $TURBULENCE_ROOT/src/ThHyd/Incompressible/Equations/BASIC
//
//////////////////////////////////////////////////////////////////////////////

#include <Navier_Stokes_Turbulent.h>
#include <Op_Diff_Turbulent_base.h>
#include <Probleme_base.h>
#include <Domaine.h>
#include <Champ_Uniforme.h>
#include <Fluide_Incompressible.h>
#include <Avanc.h>
#include <Modele_turbulence_hyd_nul.h>
#include <Discretisation_base.h>
#include <Schema_Temps_base.h>
#include <Schema_Temps.h>
#include <Discret_Thyd.h>
#include <Modele_turbulence_hyd_K_Eps.h>
#include <Modele_turbulence_hyd_K_Eps_Realisable.h>
#include <Modele_turbulence_hyd_K_Eps_Bicephale.h>
#include <Param.h>

Implemente_instanciable(Navier_Stokes_Turbulent,"Navier_Stokes_Turbulent",Navier_Stokes_std);

/*! @brief Impression de l'equation sur un flot de sortie.
 *
 * Simple appel a Equation_base::printOn(Sortie&).
 *
 * @param (Sortie& is) un flot de sortie
 * @return (Sortie&) le flot de sortie modifie
 */
Sortie& Navier_Stokes_Turbulent::printOn(Sortie& is) const
{
  return Equation_base::printOn(is);
}


/*! @brief Lit les specifications de l'equation de Navier Stokes a partir d'un flot d'entree.
 *
 *     Simple appel a Navier_Stokes_std::readOn(Entree&)
 *
 * @param (Entree& is) un flot d'entree
 * @return (Entree&) le flot d'entree modifie
 * @throws pas de modele de turbulence speficie
 */
Entree& Navier_Stokes_Turbulent::readOn(Entree& is)
{
  Navier_Stokes_std::readOn(is);
  return is;
}

void Navier_Stokes_Turbulent::set_param(Param& param)
{
  Navier_Stokes_std::set_param(param);
  //param.ajouter_non_std("solveur_pression",(this),Param::REQUIRED);
  param.ajouter_non_std("modele_turbulence",(this),Param::REQUIRED);
}

int Navier_Stokes_Turbulent::lire_motcle_non_standard(const Motcle& mot, Entree& is)
{
  if (mot=="diffusion")
    {
      Cerr << "Reading and typing of the diffusion operator : ";
      terme_diffusif.associer_diffusivite(diffusivite_pour_transport());
      // Si on a lu le modele de turbulence et qu'il est nul,
      // alors on utilise l'operateur de diffusion standard.
      if (le_modele_turbulence.non_nul() // L'operateur a ete type (donc lu)
          && sub_type(Modele_turbulence_hyd_nul, le_modele_turbulence.valeur()))
        is >> terme_diffusif;
      else
        lire_op_diff_turbulent(is);
      terme_diffusif.associer_diffusivite_pour_pas_de_temps(diffusivite_pour_pas_de_temps());
      return 1;
    }
  else if (mot=="modele_turbulence")
    {
      Cerr << "Reading and typing of the turbulence model :" ;
      le_modele_turbulence.associer_eqn(*this);
      is >> le_modele_turbulence;
      // Si on vient de lire un modele de turbulence nul et que l'operateur
      // de diffusion a deja ete lu, alors on s'est plante d'operateur,
      // stop.
      if (sub_type(Modele_turbulence_hyd_nul, le_modele_turbulence.valeur())
          && terme_diffusif.non_nul())
        {
          Cerr << "Erreur dans Navier_Stokes_Turbulent::lire:\n"
               " Si le modele de turbulence est nul, il faut le specifier\n"
               " en premier dans Navier_Stokes_Turbulent { ... }\n";

          Cerr << "Error for the method Navier_Stokes_Turbulent::lire_motcle_non_standard:\n"
               " For the case of a nul turbulence model, it must be specified\n"
               " first for the data of Navier_Stokes_Turbulent { ... }\n";
          exit();
        }
      le_modele_turbulence.discretiser();
      RefObjU le_modele;
      le_modele = le_modele_turbulence.valeur();
      liste_modeles_.add_if_not(le_modele);
      return 1;
    }
  else
    return Navier_Stokes_std::lire_motcle_non_standard(mot,is);
}

const Champ_Don& Navier_Stokes_Turbulent::diffusivite_pour_transport() const
{
  return fluide().viscosite_cinematique();
}

const Champ_base& Navier_Stokes_Turbulent::diffusivite_pour_pas_de_temps() const
{
  return fluide().viscosite_cinematique();
}

// Lecture et typage de l'operateur diffusion turbulente.
// Attention : il faut avoir fait "terme_diffusif.associer_diffusivite" avant
//             d'enter ici.
Entree& Navier_Stokes_Turbulent::lire_op_diff_turbulent(Entree& is)
{
  Motcle accouverte = "{" , accfermee = "}" ;
  Nom type="Op_Dift_";

  Nom discr=discretisation().que_suis_je();
  // les operateurs de diffusion sont communs aux discretisations VEF et VEFP1B
  if(discr=="VEFPreP1B") discr="VEF";
  type+=discr;

  Nom nb_inc;
  if (terme_diffusif.diffusivite().nb_comp() == 1)
    nb_inc = "_";
  else
    nb_inc = "_Multi_inco_";
  type+= nb_inc ;

  Nom type_diff;

  if (discr == "VDF") type_diff = ""; /* pas de const/var en VDF */
  else
    {
      if (sub_type(Champ_Uniforme, terme_diffusif.diffusivite())) type_diff = "";
      else type_diff = "var_";
    }

  type+= type_diff;

  Nom type_inco=inconnue()->que_suis_je();
  if (type_inco=="Champ_Q1_EF")
    {
      type+="Q1";
    }
  else
    {
      type+=(type_inco.suffix("Champ_"));
    }

  if (axi)
    type+="_Axi";

  Motcle motbidon;
  is >>  motbidon;
  if (motbidon!=accouverte)
    {
      Cerr << "A { was expected while reading" << finl;
      Cerr << "the turbulent diffusive term" << finl;
      exit();
    }
  is >>  motbidon;
  if (motbidon=="negligeable")
    {
      type="Op_Dift_negligeable";
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
      is >> motbidon;
      //on lit la fin de diffusion { }
      if ( motbidon != accfermee)
        Cerr << " On ne peut plus entrer d option apres negligeable "<< finl;
    }
  else if (motbidon=="tenseur_Reynolds_externe")
    {
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      Cerr << terme_diffusif.valeur().que_suis_je() << finl;
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
      terme_diffusif.valeur().Oublie_Tenseur_Reynolds();
      is >> motbidon;
      //on lit la fin de diffusion { }
      if ( motbidon != accfermee)
        Cerr << " On ne peut plus entrer d option apres tenseur_Reynolds_externe "<< finl;
    }
  else if (motbidon=="standard")
    {
      type+="_standard";
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
      is>>terme_diffusif.valeur();
    }
  else if (motbidon==accfermee)
    {
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      Cerr << terme_diffusif.valeur().que_suis_je() << finl;
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
    }
  else if (motbidon=="stab")
    {
      type+="_stab";
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
      is>>terme_diffusif.valeur();
    }
  else if (motbidon=="option")
    {
      type+="option";
      if ( discr == "EF" ) type="Op_Dift_EF_Q1_option";
      terme_diffusif.typer(type);
      Cerr << terme_diffusif.valeur().que_suis_je() << finl;
      terme_diffusif.l_op_base().associer_eqn(*this);
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
      is>>terme_diffusif.valeur();
      is >> motbidon;
      //on lit la fin de diffusion { }
      if ( motbidon != accfermee)
        Cerr << " On ne peut plus entrer d option apres option "<< finl;
    }
  else
    {
      type += motbidon;
      is >> motbidon;
      if ( motbidon != accfermee)
        {
          Cerr << " No option are now readable "<< finl;
          Cerr << " for the turbulent diffusive term" << finl;

        }
      if ( discr == "VEF" )
        {
          type += "_const_P1NC" ;
        }
      terme_diffusif.typer(type);
      terme_diffusif.l_op_base().associer_eqn(*this);
      Cerr << terme_diffusif.valeur().que_suis_je() << finl;
      terme_diffusif->associer_diffusivite(terme_diffusif.diffusivite());
    }
  return is;
}

/*! @brief Prepare le calcul.
 *
 * Simple appe a Mod_turb_hyd::preparer_caclul() sur
 *     le membre reprresentant la turbulence.
 *
 * @return (int) renvoie toujours 1
 */
int Navier_Stokes_Turbulent::preparer_calcul()
{

  Turbulence_paroi& loipar=le_modele_turbulence.valeur().loi_paroi();
  if (loipar.non_nul())
    loipar.init_lois_paroi();

  Navier_Stokes_std::preparer_calcul();
  le_modele_turbulence.preparer_calcul();
  return 1;
}

bool Navier_Stokes_Turbulent::initTimeStep(double dt)
{
  bool ok=Navier_Stokes_std::initTimeStep(dt);
  ok = ok && le_modele_turbulence->initTimeStep(dt);
  return ok;
}



/*! @brief Sauvegarde l'equation (et son modele de turbulence) sur un flot de sortie.
 *
 * @param (Sortie& os) un flot de sortie
 * @return (int) renvoie toujours 1
 */
int Navier_Stokes_Turbulent::sauvegarder(Sortie& os) const
{
  int bytes=0;
  bytes += Navier_Stokes_std::sauvegarder(os);
  assert(bytes % 4 == 0);
  bytes += le_modele_turbulence.sauvegarder(os);
  assert(bytes % 4 == 0);
  return bytes;
}


/*! @brief Reprise de l'equation et de son modele de turbulence a partir d'un flot d'entree.
 *
 * @param (Entree& is) un flot d'entree
 * @return (int) renvoie toujours 1
 * @throws fin de fichier rencontre pendant la reprise
 */
int Navier_Stokes_Turbulent::reprendre(Entree& is)
{
  Navier_Stokes_std::reprendre(is);
  double temps = schema_temps().temps_courant();
  Nom ident_modele(le_modele_turbulence.valeur().que_suis_je());
  ident_modele += probleme().domaine().le_nom();
  ident_modele += Nom(temps,probleme().reprise_format_temps());

  avancer_fichier(is, ident_modele);
  le_modele_turbulence.reprendre(is);

  return 1;
}


/*! @brief Appels successifs a: Navier_Stokes_std::completer()
 *
 *       Mod_Turb_Hyd::completer() [sur le membre concerne]
 *
 */
void Navier_Stokes_Turbulent::completer()
{
  Navier_Stokes_std::completer();
  le_modele_turbulence.completer();
}


/*! @brief Effecttue une mise a jour en temps de l'equation.
 *
 * @param (double temps) le temps de mise a jour
 */
void Navier_Stokes_Turbulent::mettre_a_jour(double temps)
{
  Navier_Stokes_std::mettre_a_jour(temps);
  le_modele_turbulence.mettre_a_jour(temps);
}

void Navier_Stokes_Turbulent::creer_champ(const Motcle& motlu)
{
  Navier_Stokes_std::creer_champ(motlu);

  if (le_modele_turbulence.non_nul())
    le_modele_turbulence->creer_champ(motlu);

  // to create k_eps_residu field
  if(le_modele_turbulence.non_nul())
    {
      if (sub_type(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()))
        ref_cast(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()).eqn_transp_K_Eps().creer_champ(motlu);
      else if (sub_type(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()))
        ref_cast(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()).eqn_transp_K_Eps().creer_champ(motlu);
    }

}

const Champ_base& Navier_Stokes_Turbulent::get_champ(const Motcle& nom) const
{
  try
    {
      return Navier_Stokes_std::get_champ(nom);
    }
  catch (Champs_compris_erreur)
    {
    }
  if (le_modele_turbulence.non_nul())
    try
      {
        return le_modele_turbulence->get_champ(nom);
      }
    catch (Champs_compris_erreur)
      {
      }
  throw Champs_compris_erreur();
  REF(Champ_base) ref_champ;

  return ref_champ;
}

void Navier_Stokes_Turbulent::get_noms_champs_postraitables(Noms& nom,Option opt) const
{
  Navier_Stokes_std::get_noms_champs_postraitables(nom,opt);
  if (le_modele_turbulence.non_nul())
    le_modele_turbulence->get_noms_champs_postraitables(nom,opt);
}

void Navier_Stokes_Turbulent::imprimer(Sortie& os) const
{
  Navier_Stokes_std::imprimer(os);
  le_modele_turbulence.imprimer(os);
}

const RefObjU& Navier_Stokes_Turbulent::get_modele(Type_modele type) const
{
  for (const auto& itr : liste_modeles_)
    {
      const RefObjU&  mod = itr;
      if (mod.non_nul())
        if ((sub_type(Mod_turb_hyd_base,mod.valeur())) && (type==TURBULENCE))
          return mod;
    }
  return Equation_base::get_modele(type);
}

// Impression du residu dans fic (generalement dt_ev)
// Cette methode peut etre surchargee par des equations
// imprimant des residus particuliers (K-Eps, Concentrations,...)
void Navier_Stokes_Turbulent::imprime_residu(SFichier& fic)
{
  Equation_base::imprime_residu(fic);
  // Si K-Eps, on imprime
  if (sub_type(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()))
    ref_cast(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()).eqn_transp_K_Eps().imprime_residu(fic);
  else if (sub_type(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()))
    ref_cast(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()).eqn_transp_K_Eps().imprime_residu(fic);
  else if (sub_type(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()))
    {
      ref_cast(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()).eqn_transp_K().imprime_residu(fic);
      ref_cast(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()).eqn_transp_Eps().imprime_residu(fic);
    }
}

// Retourne l'expression du residu (de meme peut etre surcharge)
Nom Navier_Stokes_Turbulent::expression_residu()
{
  Nom tmp=Equation_base::expression_residu();
  if (sub_type(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()))
    tmp+=ref_cast(Modele_turbulence_hyd_K_Eps,le_modele_turbulence.valeur()).eqn_transp_K_Eps().expression_residu();
  else if (sub_type(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()))
    tmp+=ref_cast(Modele_turbulence_hyd_K_Eps_Realisable,le_modele_turbulence.valeur()).eqn_transp_K_Eps().expression_residu();
  else if (sub_type(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()))
    {
      tmp+=ref_cast(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()).eqn_transp_K().expression_residu();
      tmp+=ref_cast(Modele_turbulence_hyd_K_Eps_Bicephale,le_modele_turbulence.valeur()).eqn_transp_Eps().expression_residu();
    }
  return tmp;
}

