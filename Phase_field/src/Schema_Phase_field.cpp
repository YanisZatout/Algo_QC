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
// File:        Schema_Phase_field.cpp
// Directory:   $TRUST_ROOT/src/Phase_field
// Version:     /main/19
//
//////////////////////////////////////////////////////////////////////////////

#include <Schema_Phase_field.h>
#include <Equation.h>
#include <Probleme_base.h>
#include <Source_Con_Phase_field_base.h>
#include <Param.h>

Implemente_instanciable(Schema_Phase_field,"Schema_Phase_field",Schema_Temps_base);


// Description:
//    Simple appel a: Schema_Temps_base::printOn(Sortie& )
//    Ecrit le schema en temps sur un flot de sortie.
// Precondition:
// Parametre: Sortie& s
//    Signification: un flot de sortie
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: Sortie&
//    Signification: le flot de sortie modifie
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
Sortie& Schema_Phase_field::printOn(Sortie& s) const
{
  return  Schema_Temps_base::printOn(s);
}


// Description:
//    Lit le schema en temps a partir d'un flot d'entree.
//    Simple appel a: Schema_Temps_base::readOn(Entree& )
// Precondition:
// Parametre: Entree& s
//    Signification: un flot d'entree
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: Entree&
//    Signification: le flot d'entree modifie
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
Entree& Schema_Phase_field::readOn(Entree& s)
{

  return Schema_Temps_base::readOn(s) ;
}


////////////////////////////////
//                            //
// Caracteristiques du schema //
//                            //
////////////////////////////////

// Description:
//    Renvoie le nombre de valeurs temporelles a conserver.
//    Ici : le max des deux schemas utilises.
int Schema_Phase_field::nb_valeurs_temporelles() const
{
  return max(sch2.valeur().nb_valeurs_temporelles(),sch3.valeur().nb_valeurs_temporelles());
}

// Description:
//    Renvoie le nombre de valeurs temporelles futures.
//    Ici : la valeur commune aux deux schemas utilises.
int Schema_Phase_field::nb_valeurs_futures() const
{
  int n=sch2.valeur().nb_valeurs_futures();
  assert (n==sch3.valeur().nb_valeurs_futures());
  return n;
}

// Description:
//    Renvoie le le temps a la i-eme valeur future.
//    Ici : la valeur commune aux deux schemas utilises.
double Schema_Phase_field::temps_futur(int i) const
{
  double t=sch2.valeur().temps_futur(i);
  assert(est_egal(t,sch3.valeur().temps_futur(i),pas_temps_min()));
  return t;
}

// Description:
//    Renvoie le le temps le temps que doivent rendre les champs a
//    l'appel de valeurs()
//    Ici : la valeur commune aux deux schemas utilises.
double Schema_Phase_field::temps_defaut() const
{
  double t=sch2.valeur().temps_defaut();
  assert(est_egal(t,sch3.valeur().temps_defaut(),pas_temps_min()));
  return t;
}

/////////////////////////////////////////
//                                     //
// Fin des caracteristiques du schema  //
//                                     //
/////////////////////////////////////////


void Schema_Phase_field::initialize()
{
  sch2.valeur().initialize();
  sch3.valeur().initialize();
  Schema_Temps_base::initialize();
}

bool Schema_Phase_field::initTimeStep(double dt)
{
  sch2.valeur().initTimeStep(dt);
  sch3.valeur().initTimeStep(dt);
  return Schema_Temps_base::initTimeStep(dt);
}

// Description:
//    Mise a jour du temps courant (t+=dt)
//    et du nombre de pas de temps effectue (nb_pas_dt_++).
// Precondition:
// Parametre:
//    Signification:
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Retour: int
//    Signification: retourne mettre_a_jour de la classe mere
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::mettre_a_jour()
{
  sch2.valeur().mettre_a_jour();
  sch3.valeur().mettre_a_jour();

  return Schema_Temps_base::mettre_a_jour();
}


// Description:
//    Appel a l'objet sous-jacent
//    Change le temps courant
// Precondition:
// Parametre: double& t
//    Signification: la nouvelle valeur du temps courant
//    Valeurs par defaut:
//    Contraintes: reference constante
//    Acces:
// Retour:
//    Signification:
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
void Schema_Phase_field::changer_temps_courant(const double& t)
{
  sch2.valeur().changer_temps_courant(t);
  sch3.valeur().changer_temps_courant(t);

  Schema_Temps_base::changer_temps_courant(t);
}

// Description:
//    Appel a l'objet sous-jacent
//    Renvoie 1 si il y lieu de stopper le calcul pour differente raisons:
//        - le temps final est atteint
//        - le nombre de pas de temps maximum est depasse
//        - l'etat stationnaire est atteint
//        - indicateur d'arret fichier
//    Renvoie 0 sinon
// Precondition:
// Parametre:
//    Signification:
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Retour:entier
//    Signification: 1 si il y a lieu de s'arreter 0 sinon
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::stop() const
{
  int ls2 = sch2.valeur().stop();
  int ls3 = sch3.valeur().stop();

  return (ls2 | ls3 | Schema_Temps_base::stop());
}


// Description:
//    Appel a l'objet sous-jacent
//    Imprime le schema en temp sur un flot de sortie (si il y a lieu).
// Precondition:
// Parametre: Sortie& os
//    Signification: le flot de sortie
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour:
//    Signification:
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition: la methode ne modifie pas l'objet
void Schema_Phase_field::imprimer(Sortie& os) const
{
  sch2.valeur().imprimer(os);
  sch3.valeur().imprimer(os);

  Schema_Temps_base::imprimer(os);
}


// Description
//    Appel a l'objet sous-jacent:
//    Renvoie 1 s'il y a lieu de faire une sauvegarde
//    0 sinon.
// Precondition:
// Parametre:
//    Signification:
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Retour: int
//    Signification: 1 si il faut faire une sauvegarde 0 sinon
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition: la methode ne modifie pas l'objet
// int Schema_Phase_field::lsauv() const
// {
//   int ls2 = sch2.valeur().lsauv();
//   int ls3 = sch3.valeur().lsauv();
//   return (ls2 | ls3 | Schema_Temps_base::lsauv());
// }


// Description:
//    Corrige le pas de temps dt_min <= dt <= dt_max
// Precondition:
// Parametre:
//    Signification:
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Retour: int
//    Signification: retourne corriger_pas_temps de la classe mere
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
bool Schema_Phase_field::corriger_dt_calcule(double& dt) const
{
  bool ok=sch2.valeur().corriger_dt_calcule(dt);
  ok = ok && sch3.valeur().corriger_dt_calcule(dt);
  ok = ok && Schema_Temps_base::corriger_dt_calcule(dt);
  return ok;
}

// Description:
//    Complete les attributs de sch2
// Precondition:
// Parametre:
//    Signification:
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Retour: int
//    Signification: retourne toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
void Schema_Phase_field::completer()
{
  Schema_Temps_base& le_sch2 = sch2.valeur();
  le_sch2.set_temps_init()=temps_init();
  le_sch2.set_temps_max()=temps_max();
  le_sch2.set_temps_courant()=temps_courant();
  le_sch2.set_nb_pas_dt()=nb_pas_dt();
  le_sch2.set_nb_pas_dt_max()=nb_pas_dt_max();
  le_sch2.set_dt_min()=pas_temps_min();
  le_sch2.set_dt_max()=pas_temps_max();
  le_sch2.set_dt_sauv()=temps_sauv();
  le_sch2.set_dt_impr()=temps_impr();
  le_sch2.set_facsec()=facteur_securite_pas();
  le_sch2.set_seuil_statio()=seuil_statio();
  le_sch2.set_stationnaire_atteint()=isStationary();
  le_sch2.set_diffusion_implicite()=diffusion_implicite();
  le_sch2.set_seuil_diffusion_implicite()=seuil_diffusion_implicite();
  le_sch2.set_niter_max_diffusion_implicite()=niter_max_diffusion_implicite();
  le_sch2.set_dt()=pas_de_temps();
  le_sch2.set_mode_dt_start()=mode_dt_start();
  le_sch2.set_indice_tps_final_atteint()=indice_tps_final_atteint();
  le_sch2.set_indice_nb_pas_dt_max_atteint()=indice_nb_pas_dt_max_atteint();
  le_sch2.set_lu()=lu();

  Schema_Temps_base& le_sch3 = sch3.valeur();
  le_sch3.set_temps_init()=temps_init();
  le_sch3.set_temps_max()=temps_max();
  le_sch3.set_temps_courant()=temps_courant();
  le_sch3.set_nb_pas_dt()=nb_pas_dt();
  le_sch3.set_nb_pas_dt_max()=nb_pas_dt_max();
  le_sch3.set_dt_min()=pas_temps_min();
  le_sch3.set_dt_max()=pas_temps_max();
  le_sch3.set_dt_sauv()=temps_sauv();
  le_sch3.set_dt_impr()=temps_impr();
  le_sch3.set_facsec()=facteur_securite_pas();
  le_sch3.set_seuil_statio()=seuil_statio();
  le_sch3.set_stationnaire_atteint()=isStationary();
  le_sch3.set_diffusion_implicite()=diffusion_implicite();
  le_sch3.set_seuil_diffusion_implicite()=seuil_diffusion_implicite();
  le_sch3.set_niter_max_diffusion_implicite()=niter_max_diffusion_implicite();
  le_sch3.set_dt()=pas_de_temps();
  le_sch3.set_mode_dt_start()=mode_dt_start();
  le_sch3.set_indice_tps_final_atteint()=indice_tps_final_atteint();
  le_sch3.set_indice_nb_pas_dt_max_atteint()=indice_nb_pas_dt_max_atteint();
  le_sch3.set_lu()=lu();
}


// Description:
//    Lecture du nombre d'iterations pour l'etape de relaxation du
//    Schema_Phase_field
// Precondition:
// Parametre: const Motcle& mot
//    Signification: un mot cle
//    Valeurs par defaut:
//    Contraintes:
//    Acces:
// Parametre: Entree& is
//    Signification: un flot d'entree
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour:
//    Signification:
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::lire_motcle_non_standard(const Motcle& mot, Entree& is)
{
  Cerr << "mot=" << mot << finl;
  if (mot=="schema_CH")
    {
      Nom type_sch;
      Cerr << "Lecture du schema utilise dans l'equation Phase_field" << finl;
      is >> type_sch;
      sch2.typer(type_sch);
      is >> sch2.valeur();
    }
  else if (mot=="schema_NS")
    {
      Nom type_sch;
      Cerr << "Lecture du schema utilise dans l'equation Navier-Stokes" << finl;
      is >> type_sch;
      sch3.typer(type_sch);
      is >> sch3.valeur();
    }
  else
    return Schema_Temps_base::lire_motcle_non_standard(mot,is);
  return 1;

}
void Schema_Phase_field::set_param(Param& param)
{
  param.ajouter_non_std("schema_ch",this);
  param.ajouter_non_std("Schema_NS",this);
  Schema_Temps_base::set_param(param);
}

// Description:
//    Effectue un pas de temps
//    sur l'equation passee en parametre.
//    Le pas de temps effectue n'est pas standard si l'equation
//    est de type Convection_Diffusion_Phase_field
// Precondition:
// Parametre: Equation_base& eqn
//    Signification: l'equation que l'on veut faire avancer d'un
//                   pas de temps
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: int
//    Signification: renvoie toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::faire_un_pas_de_temps_eqn_base(Equation_base& eqn)
{
  if (eqn.que_suis_je() == "Convection_Diffusion_Phase_field")
    {
      Convection_Diffusion_Phase_field& eq_c=ref_cast(Convection_Diffusion_Phase_field,eqn);
      return faire_un_pas_de_temps_C_D_Phase_field(eq_c);
    }
  else
    {
      sch3.valeur().set_dt()=pas_de_temps();
      sch3.valeur().faire_un_pas_de_temps_eqn_base(eqn);
      set_stationnaire_atteint()=sch3.valeur().isStationary();
      return 1;
    }
  return 1;
}


// Description:
// faire_un_pas_de_temps_pb_base avec NS = equation(0) et CH = equation(1)
// Sert a calculer l'equation de Cahn-Hilliard en premier puis Navier-Stokes, en appelant
// equation(1) d'abord puis equation(0) pour obtenir les couplages voulus.
// (dans le cas nb_eqn=2)
// Precondition:
// Parametre: Pb_base& eqn
//    Signification: Pb a resoudre
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: int
//    Signification: renvoie toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
bool Schema_Phase_field::iterateTimeStep(bool& converged)
{
  Probleme_base& prob=pb_base();
  double temps=temps_courant_+dt_;
  int nb_eqn=prob.nombre_d_equations();
  // On resout dans l'ordre inverse des equations du probleme
  for(int i=nb_eqn-1; i>-1; i--)
    {
      Equation_base& eqn_i=prob.equation(i);
      if (eqn_i.equation_non_resolue())
        {
          Cout<< "====================================================" << finl;
          Cout<< eqn_i.que_suis_je()<<" equation is not solved."<<finl;
          Cout<< "====================================================" << finl;
          // On calcule une fois la derivee pour avoir les flux bord
          if (eqn_i.schema_temps().nb_pas_dt()==0)
            {
              DoubleTab inconnue_valeurs(eqn_i.inconnue().valeurs());
              eqn_i.derivee_en_temps_inco(inconnue_valeurs);
            }
        }
      else
        {
          eqn_i.zone_Cl_dis().mettre_a_jour(temps);
          faire_un_pas_de_temps_eqn_base(eqn_i);
        }
    }
  converged=true;
  return true;
}

// Description:
//    Effectue un pas de temps
//    sur l'equation de type Convection_Diffusion_Phase_field.
// Precondition:
// Parametre: Convection_Diffusion_Phase_field& eq_c
//    Signification: l'equation que l'on veut faire avancer d'un
//                   pas de temps
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: int
//    Signification: renvoie toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::faire_un_pas_de_temps_C_D_Phase_field(Convection_Diffusion_Phase_field& eq_c)
{
  premier_dt(eq_c);
  deuxieme_dt(eq_c);
  eq_c.calculer_rho();

  return 1;
}


// Description:
//    Effectue le premier demi-pas de temps1
//    sur l'equation de type Convection_Diffusion_Phase_field.
// Precondition:
// Parametre: Convection_Diffusion_Phase_field& eq_c
//    Signification: l'equation que l'on veut faire avancer d'un
//                   demi-pas de temps
//    Valeurs par defaut:
//    Contraintes: On fait comme s'il n'y a qu'une source pour l'instant
//    Acces: entree/sortie
// Retour: int
//    Signification: renvoie toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::premier_dt(Convection_Diffusion_Phase_field& eq_c)
{
  Sources& list_sources = eq_c.sources();
  Source_Con_Phase_field_base& source_pf = ref_cast(Source_Con_Phase_field_base, list_sources(0).valeur());

  // Calcule c_demi
  source_pf.premier_demi_dt();

  return 1;
}


// Description:
//    Effectue le deuxieme demi-pas de temps
//    sur l'equation de type Convection_Diffusion_Phase_field.
// Precondition:
// Parametre: Convection_Diffusion_Phase_field& eq_c
//    Signification: l'equation que l'on veut faire avancer d'un
//                   demi-pas de temps
//    Valeurs par defaut:
//    Contraintes:
//    Acces: entree/sortie
// Retour: int
//    Signification: renvoie toujours 1
//    Contraintes:
// Exception:
// Effets de bord:
// Postcondition:
int Schema_Phase_field::deuxieme_dt(Convection_Diffusion_Phase_field& eq_c)
{
  DoubleTab& present = eq_c.inconnue().valeurs();
  DoubleTab intermediaire(present);
  intermediaire = present;

  sch2.valeur().set_dt()=pas_de_temps();

  present=eq_c.get_c_demi();

  sch2.valeur().faire_un_pas_de_temps_eqn_base(eq_c);
  set_stationnaire_atteint()=sch2.valeur().isStationary();
  present=intermediaire;

  return 1;
}

void Schema_Phase_field::associer_pb(const Probleme_base& un_probleme)
{
  Schema_Temps_base::associer_pb(un_probleme);
  sch2->associer_pb(un_probleme);
  sch3->associer_pb(un_probleme);
}
