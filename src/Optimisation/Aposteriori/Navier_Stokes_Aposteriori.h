/****************************************************************************
* Copyright (c) 2022, CEA
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

#ifndef Navier_Stokes_Aposteriori_included
#define Navier_Stokes_Aposteriori_included

#include <Champ_Post_Operateur_Eqn.h>
#include <Navier_Stokes_std.h>

class Navier_Stokes_Aposteriori : public Navier_Stokes_std
{
  Declare_instanciable_sans_constructeur(Navier_Stokes_Aposteriori);
public:

  // constructeur
  Navier_Stokes_Aposteriori() : Navier_Stokes_std()
  {
    champs_compris_.ajoute_nom_compris("estimateur_aposteriori");
  }

  void discretiser() override;
  const Champ_base& get_champ(const Motcle& nom) const override;
  void creer_champ(const Motcle& motlu) override;

  Champ_Post_Operateur_Eqn& get_champ_source() { return champ_src_; }

private:
  Champ_Fonc estimateur_aposteriori_;
  Champ_Post_Operateur_Eqn champ_src_;
  void estimateur_aposteriori();
};

#endif /* Navier_Stokes_Aposteriori_included */
