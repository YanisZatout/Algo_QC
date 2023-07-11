/****************************************************************************
* Copyright (c) 2015 - 2016, CEA
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
/////////////////////////////////////////////////////////////////////////////
//
// File      : Echange_contact_VDF_FT_Disc.cpp
// Directory : $FRONT_TRACKING_DISCONTINU_ROOT/src/VDF
//
/////////////////////////////////////////////////////////////////////////////

#include <Echange_contact_VDF_FT_Disc.h>

#include <Champ_front_calc.h>
#include <Probleme_base.h>
#include <Champ_Uniforme.h>
#include <Schema_Temps_base.h>
#include <Milieu_base.h>
#include <Modele_turbulence_scal_base.h>
#include <Domaine_VDF.h>
#include <Equation_base.h>
#include <Conduction.h>
#include <Param.h>
#include <Probleme_FT_Disc_gen.h>
#include <Triple_Line_Model_FT_Disc.h>
#include <Domaine_VF.h>
#include <Front_VF.h>


Implemente_instanciable( Echange_contact_VDF_FT_Disc, "Echange_contact_VDF_FT_Disc", Echange_contact_VDF ) ;
// XD echange_contact_vdf_ft_disc condlim_base echange_contact_vdf_ft_disc 1 echange_conatct_vdf en prescisant la phase

Sortie& Echange_contact_VDF_FT_Disc::printOn( Sortie& os ) const
{
  Echange_contact_VDF::printOn( os );
  return os;
}

Entree& Echange_contact_VDF_FT_Disc::readOn( Entree& s )
{
  if (app_domains.size() == 0) app_domains = { Motcle("Thermique") };

  //  Echange_contact_VDF::readOn( is );
  Cerr<<"Lecture des parametres du contact (Echange_contact_VDF_FT_Disc::readOn)"<<finl;
  Param param("Echange_contact_VDF_FT_Disc::readOn");
  param.ajouter("autre_probleme",&nom_autre_pb_,Param::REQUIRED); // XD_ADD_P chaine name of other problem
  param.ajouter("autre_bord",&nom_bord,Param::REQUIRED); // XD_ADD_P chaine name of other boundary
  param.ajouter("autre_champ_temperature",&nom_champ,Param::REQUIRED); // XD_ADD_P chaine name of other field
  param.ajouter("nom_mon_indicatrice",&nom_champ_indicatrice_,Param::REQUIRED);  // XD_ADD_P chaine name of indicatrice
  int phase;
  param.ajouter("phase",&phase,Param::REQUIRED); // XD_ADD_P int phase
  param.lire_avec_accolades(s);
  indicatrice_ref_ = double(phase);
  nom_bord_oppose_=nom_bord;

  h_paroi=1e10; // why not git 1/h_paroi = 0....?
  T_autre_pb().typer("Champ_front_calc");
  T_ext().typer("Ch_front_var_instationnaire_dep");
  T_ext()->fixer_nb_comp(1);

  return s;
}
void Echange_contact_VDF_FT_Disc::mettre_a_jour(double temps)
{
  Champ_front_calc& ch=ref_cast(Champ_front_calc, T_autre_pb().valeur());
  // le_milieu =  SOLID
  const Milieu_base& le_milieu=ch.milieu();
  int nb_comp = le_milieu.conductivite()->nb_comp();
  assert(nb_comp==1);

  T_autre_pb().mettre_a_jour(temps);

  int is_pb_fluide=0;

  DoubleTab& mon_h= h_imp_->valeurs();
  DoubleTab& mon_Ti= Ti_wall_->valeurs();

  int opt=0;
  calculer_h_autre_pb( autre_h, 0., opt);
  // Here, compute h_diff in the fluid side
  calculer_h_mon_pb(mon_h,0.,opt);

  calculer_Teta_paroi(mon_Ti,mon_h,autre_h,is_pb_fluide,temps);
  calculer_Teta_equiv(T_ext()->valeurs_au_temps(temps),mon_h,autre_h,is_pb_fluide,temps);

  int taille=mon_h.dimension(0);
  for (int ii=0; ii<taille; ii++)
    for (int jj=0; jj<nb_comp; jj++)
      mon_h(ii,jj)=1./(1./autre_h(ii,jj)+1./mon_h(ii,jj));

  indicatrice_.mettre_a_jour(temps);
  const DoubleTab& I = indicatrice_.valeur().valeurs_au_temps(temps);

  for (int ii=0; ii<taille; ii++)
    for (int jj=0; jj<nb_comp; jj++ )
      {
        if (!est_egal(I(ii,0),indicatrice_ref_))
          {
            mon_h(ii,jj)=0. ;
            mon_Ti(ii, jj) = 0.;
          }
// **************************************To be implemented*******************
        // 2 - phase cells at pb-Boundary when solving T-eq at Liquid side
        //mixed mesh => Text, Twall, mon_h
        if (I(ii,0) > 0 && I(ii,0) < 1  && (indicatrice_ref_ = 1 ))
          {
            Nom nom_pb=mon_dom_cl_dis->equation().probleme().que_suis_je();
            Probleme_base& pb_gen=ref_cast(Probleme_base, Interprete::objet(nom_pb));
            const Probleme_FT_Disc_gen *pbft = dynamic_cast<const Probleme_FT_Disc_gen*>(&pb_gen);
            const Triple_Line_Model_FT_Disc *tcl = pbft ? &pbft->tcl() : nullptr;
            const ArrOfDouble& Q_from_CL = tcl->Q();
            const ArrOfInt& faces_with_CL_contrib = tcl-> boundary_faces();


            Nom  nom_dom = (mon_dom_cl_dis -> domaine()).que_suis_je();
            Domaine_VF& le_dom=ref_cast(Domaine_VF, Interprete::objet(nom_dom));
            const IntTab& face_voisins = le_dom.face_voisins();

            Frontiere&  le_front = h_imp_.frontiere_dis().frontiere();
            const int face = ii+le_front.num_premiere_face();


            const Equation_base& mon_eqn = domaine_Cl_dis().equation();
            const DoubleTab& mon_inco=mon_eqn.inconnue().valeurs();


            const int nb_contact_line_contribution = faces_with_CL_contrib.size_array();
            for (int idx = 0; idx < nb_contact_line_contribution; idx++)
              {
                // face i


                const int facei = faces_with_CL_contrib[idx];
                if (facei == face)
                  {
                    const double sign = (face_voisins(face, 0) == -1) ? -1. : 1.;
                    const double TCL_wall_flux = Q_from_CL[idx];
                    const double val = sign*TCL_wall_flux;
                    if (Ti_wall_(ii, jj) != 0.)
                      mon_h(ii) += val/(T_ext()->valeurs_au_temps(temps)(ii)-mon_inco(ii, 0));
                    mon_Ti(ii, jj) += T_ext().valeurs()(ii, jj) - val/autre_h(ii) ;
                  }
              }

          }
      }

  Echange_global_impose::mettre_a_jour(temps);
}


void Echange_contact_VDF_FT_Disc::completer()
{
  Echange_contact_VDF::completer();
  indicatrice_.typer("Champ_front_calc");
  Champ_front_calc& ch=ref_cast(Champ_front_calc, indicatrice_.valeur());


  Nom nom_bord_=frontiere_dis().frontiere().le_nom();
  Nom nom_pb=domaine_Cl_dis().equation().probleme().le_nom();
  int distant=0;

  // when solving pure condution pb for solid
  if (sub_type(Conduction,domaine_Cl_dis().equation()))
    {
      nom_pb=nom_autre_pb_;
      nom_bord_=nom_bord_oppose_;
      distant=1;
    }
  ch.creer(nom_pb, nom_bord_, nom_champ_indicatrice_);
  ch.set_distant(distant);


  ch.associer_fr_dis_base(T_ext().frontiere_dis());

  ch.completer();

  int nb_cases=domaine_Cl_dis().equation().schema_temps().nb_valeurs_temporelles();
  ch.fixer_nb_valeurs_temporelles(nb_cases);

  // we will implenment the mis a jour of temperature here
  Ti_wall_.typer("Champ_front_calc");
  Ti_wall_.associer_fr_dis_base(T_ext().frontiere_dis());
  Ti_wall_->completer();
  Ti_wall_->fixer_nb_valeurs_temporelles(nb_cases);
}



/*! @brief Change le i-eme temps futur de la CL.
 *
 */
void Echange_contact_VDF_FT_Disc::changer_temps_futur(double temps,int i)
{
  Echange_contact_VDF::changer_temps_futur(temps,i);
  indicatrice_->changer_temps_futur(temps,i);
  Ti_wall_ -> changer_temps_futur(temps,i);
}

/*! @brief Tourne la roue de la CL
 *
 */
int Echange_contact_VDF_FT_Disc::avancer(double temps)
{
  int ok=Echange_contact_VDF::avancer(temps);
  ok = ok && indicatrice_->avancer(temps);
  ok = ok && Ti_wall_ -> avancer(temps);
  return ok;
}

/*! @brief Tourne la roue de la CL
 *
 */
int Echange_contact_VDF_FT_Disc::reculer(double temps)
{
  int ok=Echange_contact_VDF::reculer(temps);
  ok = ok && indicatrice_->reculer(temps);
  ok = ok && Ti_wall_ -> reculer(temps);
  return ok;
}

int Echange_contact_VDF_FT_Disc::initialiser(double temps)
{
  if (!Echange_contact_VDF::initialiser(temps))
    return 0;

  // XXX : On rempli les valeurs ici et pas dans le readOn car le milieu de pb2 ets pas encore lu !!!
  Champ_front_calc& cha=ref_cast(Champ_front_calc, T_autre_pb().valeur());
  cha.creer(nom_autre_pb_, nom_bord, nom_champ);

  // initialization of Ti_wall_ with temperature of T_autre_pb
  Champ_front_calc& chT=ref_cast(Champ_front_calc, Ti_wall_.valeur());
  chT.creer(nom_autre_pb_, nom_bord, nom_champ);

  Champ_front_calc& ch=ref_cast(Champ_front_calc, indicatrice_.valeur());
  return ch.initialiser(temps,domaine_Cl_dis().equation().inconnue());
}
