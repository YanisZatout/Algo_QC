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
// File:        Fluide_Diphasique.cpp
// Directory:   $TRUST_ROOT/src/Front_tracking_discontinu
// Version:     /main/17
//
//////////////////////////////////////////////////////////////////////////////
#include <Fluide_Diphasique.h>
#include <Fluide_Incompressible.h>
#include <Motcle.h>
#include <Interprete.h>
#include <Ref_Fluide_Diphasique.h>
#include <Param.h>
#include <Champ_Uniforme.h>

Implemente_instanciable_sans_constructeur(Fluide_Diphasique,"Fluide_Diphasique",Milieu_base);

Implemente_ref(Fluide_Diphasique);

Fluide_Diphasique::Fluide_Diphasique()
{
  indic_rayo_ = NONRAYO;
}

Sortie& Fluide_Diphasique::printOn(Sortie& os) const
{
  return os;
}

// cf Milieu_base::readOn
Entree& Fluide_Diphasique::readOn(Entree& is)
{
  Milieu_base::readOn(is);
  return is;
}

void Fluide_Diphasique::set_param(Param& param)
{
  param.ajouter("sigma",&sigma_,Param::REQUIRED);
  param.ajouter_non_std("fluide0",(this),Param::REQUIRED);
  param.ajouter_non_std("fluide1",(this),Param::REQUIRED);
  param.ajouter("chaleur_latente",&chaleur_latente_);
}

int Fluide_Diphasique::lire_motcle_non_standard(const Motcle& mot, Entree& is)
{
  Motcle motlu;
  if ((mot=="fluide0") || (mot=="fluide1"))
    {
      Nom nom_objet;
      is >> nom_objet;
      const Objet_U& objet = Interprete::objet(nom_objet);
      const Fluide_Incompressible& fluide = ref_cast(Fluide_Incompressible,objet);
      if (mot=="fluide0")
        phase0_ = fluide;
      else if (mot=="fluide1")
        phase1_ = fluide;
      return 1;
    }
  else
    return Milieu_base::lire_motcle_non_standard(mot,is);
  return 1;
}

void Fluide_Diphasique::verifier_coherence_champs(int& err,Nom& msg)
{
  msg="";
  if (!sub_type(Champ_Uniforme,sigma_.valeur()))
    {
      msg += "The surface tension sigma must be specify with a Champ_Uniforme type field. \n";
      err = 1;
    }
  else
    {
      if (sigma_(0,0) < 0)
        {
          msg += "The surface tension sigma is not positive. \n";
          err = 1;
        }
    }

  if (chaleur_latente_.non_nul())
    {
      if (!sub_type(Champ_Uniforme,chaleur_latente_.valeur()))
        {
          msg += "The latent heat chaleur_latente must be specify with a Champ_Uniforme type field. \n";
          err = 1;
        }
    }

  Milieu_base::verifier_coherence_champs(err,msg);
}

const Fluide_Incompressible&
Fluide_Diphasique::fluide_phase(int phase) const
{
  assert(phase == 0 || phase == 1);
  if (phase == 0)
    return phase0_.valeur();
  else
    return phase1_.valeur();
}

double Fluide_Diphasique::sigma() const
{
  return sigma_(0,0);
}

double Fluide_Diphasique::chaleur_latente() const
{
  if (!chaleur_latente_.non_nul())
    {
      Cerr << "Fluide_Diphasique::chaleur_latente()\n";
      Cerr << " The latent heat has not been specified." << finl;
      exit();
    }
  return chaleur_latente_(0,0);
}

int Fluide_Diphasique::initialiser(const double& temps)
{
  phase0_.valeur().initialiser(temps);
  phase1_.valeur().initialiser(temps);
  return 1;
}

void Fluide_Diphasique::mettre_a_jour(double temps)
{
  // en particulier, mise a jour de g qui peut dependre de t...
  Milieu_base::mettre_a_jour(temps);
  phase0_.valeur().mettre_a_jour(temps);
  phase1_.valeur().mettre_a_jour(temps);
}
void Fluide_Diphasique::discretiser(const Probleme_base& pb, const  Discretisation_base& dis)
{
  // sigma chaleur latente phase_0 phase_1  diffusivite revoir
  phase0_.valeur().discretiser(pb,dis);
  phase1_.valeur().discretiser(pb,dis);
  //Milieu_base::discretiser(pb,dis);
}



// ===========================================================================
// Appels invalides :

const Champ_Don& Fluide_Diphasique::masse_volumique() const
{
  Cerr << "Error : Fluide_Diphasique::masse_volumique()" << finl;
  assert(0);
  exit();
  return phase0_.valeur().masse_volumique();
}

Champ_Don& Fluide_Diphasique::masse_volumique()
{
  Cerr << "Error : Fluide_Diphasique::masse_volumique()" << finl;
  assert(0);
  exit();
  return masse_volumique();
}

const Champ_Don& Fluide_Diphasique::diffusivite() const
{
  Cerr << "Error : Fluide_Diphasique::diffusivite()" << finl;
  assert(0);
  exit();
  return diffusivite();
}

Champ_Don& Fluide_Diphasique::diffusivite()
{
  Cerr << "Error : Fluide_Diphasique::diffusivite()" << finl;
  assert(0);
  exit();
  return diffusivite();
}

const Champ_Don& Fluide_Diphasique::conductivite() const
{
  Cerr << "Error : Fluide_Diphasique::conductivite()" << finl;
  assert(0);
  exit();
  return conductivite();
}

Champ_Don& Fluide_Diphasique::conductivite()
{
  Cerr << "Error : Fluide_Diphasique::conductivite()" << finl;
  assert(0);
  exit();
  return conductivite();
}

const Champ_Don& Fluide_Diphasique::capacite_calorifique() const
{
  Cerr << "Error : Fluide_Diphasique::capacite_calorifique()" << finl;
  assert(0);
  exit();
  return capacite_calorifique();
}

Champ_Don& Fluide_Diphasique::capacite_calorifique()
{
  Cerr << "Error : Fluide_Diphasique::capacite_calorifique()" << finl;
  assert(0);
  exit();
  return capacite_calorifique();
}

const Champ_Don& Fluide_Diphasique::beta_t() const
{
  Cerr << "Error : Fluide_Diphasique::beta_t()" << finl;
  assert(0);
  exit();
  return beta_t();
}

Champ_Don& Fluide_Diphasique::beta_t()
{
  Cerr << "Error : Fluide_Diphasique::beta_t()" << finl;
  assert(0);
  exit();
  return beta_t();
}

