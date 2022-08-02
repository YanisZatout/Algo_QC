//TRUST_NO_INDENT
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
// File      : Multigrille_Adrien.cpp
// Directory : $IJK_ROOT/src/IJK/solveur_mg
//
/////////////////////////////////////////////////////////////////////////////
//
// WARNING: DO NOT EDIT THIS FILE! Only edit the template file Multigrille_Adrien.cpp.P
//
#include <Param.h>
#include <Parser.h>
#include <Statistiques.h>
#include <TRUSTArray.h>
#include <Multigrille_Adrien.h>
#include <communications.h>
#include <Domaine.h>
#include <Schema_Comm.h>
#include <SSE_kernels.h>
#include <IJK_discretization.h>
#include <Interprete_bloc.h>
#include <stat_counters.h>

Implemente_instanciable_sans_constructeur(Multigrille_Adrien, "Solv_Multigrille_Adrien", Multigrille_base);

long long flop_count = 0;

Sortie & Multigrille_Adrien::printOn(Sortie & os) const
{
  return os;
}

Multigrille_Adrien::Multigrille_Adrien()
{
  ghost_size_ = 1;
}

void Multigrille_Adrien::ajouter_param(Param & param)
{
  param.ajouter("ghost_size", &ghost_size_);
  param.ajouter("coarsen_operators", &coarsen_operators_);

  Multigrille_base::ajouter_param(param);
}

Entree & Multigrille_Adrien::readOn(Entree & is)
{
  Nom ijkdis_name;
  Param param(que_suis_je());
  ajouter_param(param);
  param.ajouter("ijkdis_name",&ijkdis_name);
  param.lire_avec_accolades(is);

  if (ijkdis_name!=Nom())
  {
  const IJK_discretization & ijkdis = ref_cast(IJK_discretization, Interprete_bloc::objet_global(ijkdis_name));
  const IJK_Splitting & split = ijkdis.get_IJK_splitting();

  initialize(split);
 }
  return is;
}

// search non empty lists in src and fills non_empty and dest with non empty lists
void pack_lists(const VECT(ArrOfInt) & src, ArrOfInt & non_empty, Static_Int_Lists & dest)
{
  // count non empty lists
  int n = 0;
  int i;
  const int nmax = src.size();
  for (i = 0; i < nmax; i++)
    if (src[i].size_array() > 0)
      n++;

  VECT(ArrOfInt) tmp(n);
  non_empty.resize_array(n);

  // copy non empty lists into tmp
  int j = 0;
  for (i = 0; i < nmax; i++) {
    if (src[i].size_array() > 0) {
      non_empty[j] = i;
      tmp[j] = src[i];
      j++;
    }
  }

  // Built dest
  dest.set(tmp);
}

int bsearch_double(const ArrOfDouble & tab, double valeur)
{
  int i = 0;
  int j = tab.size_array() - 1;
  while (j > i + 1) {
    // Le tableau doit etre trie
    assert(j == tab.size_array() || tab[i] <= tab[j]);
    const int milieu = (i + j) / 2;
    const double val = tab[milieu];
    if (val > valeur)
      j = milieu;
    else
      i = milieu;
  }
  return i;
}

int Multigrille_Adrien::completer(const Equation_base & eq)
{
  // fait dans readOn si on a lu ijkdis_name
  // fetch the vdf_to_ijk translator (assume there is one unique object, with conventional name)
  const char * ijkdis_name = IJK_discretization::get_conventional_name();
  const IJK_discretization & ijkdis = ref_cast(IJK_discretization, Interprete_bloc::objet_global(ijkdis_name));
  const IJK_Splitting & split = ijkdis.get_IJK_splitting();

  initialize(split);
  
  return 1;
}

void Multigrille_Adrien::initialize(const IJK_Splitting & split)
{
  if (solver_precision_ == precision_double_)
    completer_double(split);
  else if (solver_precision_ == precision_float_)
    completer_float(split);
  else if (solver_precision_ == precision_mix_) {
    completer_float(split);
    completer_double_for_residue(split);
  }
  IJK_Field_float rho;
  rho.allocate(split, IJK_Splitting::ELEM, 0);
  rho.data() = 1.;
  set_rho(rho);
}




// Apply n_jacobi iterations of jacobi to x vector,
//  and, if the residu pointer is not zero, computes the residue.
// Precondition: ghost cells for secmem must be up to date.
//  The number of required ghost layers is n_jacobi + (1 if residu is required)
// Postcondition: ghost cells are not updated
void Multigrille_Adrien::jacobi_residu(IJK_Field_double & x,
				       const IJK_Field_double *secmem,
				       const int grid_level,
				       const int n_jacobi_tot,
				       IJK_Field_double *residu) const
{
  static Stat_Counter_Id jacobi_residu_counter_ = statistiques().new_counter(2, "multigrille : residu jacobi");
  
  statistiques().begin_count(jacobi_residu_counter_);

  const IJK_Field_local_double & coeffs_face = grids_data_double_[grid_level].get_faces_coefficients();

  IJ_layout layout(x);
  const double relax = (double)relax_jacobi(grid_level);
  // We can do at most this number of passes per sweep:
  const int nb_passes_max_per_sweep = x.ghost();
  // We have to do this number of passes in total:
  const int nb_passes_to_do_total = n_jacobi_tot + (residu ? 1 : 0);
  int nb_passes_done = 0;

  // Repeat as many sweeps as necessary:
  while (nb_passes_done < nb_passes_to_do_total) {

    int nb_passes_to_do = nb_passes_to_do_total - nb_passes_done;
    if (nb_passes_to_do > nb_passes_max_per_sweep)
      nb_passes_to_do = nb_passes_max_per_sweep;

    x.echange_espace_virtuel(nb_passes_to_do);

    if (grid_level == 0) {
      // Place counter here to avoid counting communication time:
      //statistiques().begin_count(counter);
      flop_count = 0;
    }

    const bool last_pass_is_residue = (nb_passes_done + nb_passes_to_do == n_jacobi_tot + 1);
    if (1)
      Multipass_Jacobigeneric_double(x, *residu, coeffs_face, *secmem, nb_passes_to_do, last_pass_is_residue, relax);
    else {
      for (int pass = 0; pass < nb_passes_to_do; pass++) {
	if (pass == nb_passes_to_do-1 && last_pass_is_residue) {
	  reference_kernel_double(x, coeffs_face, *secmem, *residu, relax, true);
	} else {
	  IJK_Field_double tmp = x;
	  reference_kernel_double(tmp, coeffs_face, *secmem, x, relax, false);
	}
      }
    }

    nb_passes_done += nb_passes_to_do;

    if (grid_level == 0) {
      //statistiques().end_count(counter, flop_count);
      flop_count = 0;
    }
  }
 	statistiques().end_count(jacobi_residu_counter_, (int)flop_count);
}

void Multigrille_Adrien::coarsen(const IJK_Field_double & fine, IJK_Field_double & coarse, int fine_level) const
{
  coarsen_operators_[fine_level].valeur().coarsen(fine, coarse);
}

// calcul de "fine -= interpolated(coarse)"
void Multigrille_Adrien::interpolate_sub_shiftk(const IJK_Field_double & coarse, IJK_Field_double & fine, int fine_level) const
{
  // Shift by maximum value:
  const int shift = needed_kshift_for_jacobi(fine_level) - fine.k_shift();
  coarsen_operators_[fine_level].valeur().interpolate_sub_shiftk(coarse, fine, shift);
}

void Multigrille_Adrien::completer_double(const IJK_Splitting & split)
{
  Cerr << "Multigrille_Adrien::completer_double" << finl;

  const int nb_operators = coarsen_operators_.size();
  const int nb_grids = nb_operators + 1;
  grids_data_double_.dimensionner(nb_grids);
  grids_data_double_[0].initialize(split, ghost_size_, nsweeps_jacobi_residu(0));
  int i;
  for (i = 0; i < nb_operators; i++) {
    coarsen_operators_[i].valeur().initialize_grid_data(grids_data_double_[i], grids_data_double_[i+1],
							nsweeps_jacobi_residu(i+1));
  }
  for (i = 0; i < nb_grids; i++) {
    const IJK_Field_double & g = grids_data_double_[i].get_rho();
    Journal() << "Grid level " << i << " local size: " << g.ni() << "x" << g.nj() << "x" << g.nk() << finl;
  }
}



// Allocate an array for the grid data
void Multigrille_Adrien::alloc_field(IJK_Field_double & field, int level, bool with_additional_layers) const
{
  if (level < 0 || level > grids_data_double_.size()) {
    Cerr << "Fatal: wrong level in alloc_field" << finl;
    Process::exit();
  }
  int n = 0;
  if (with_additional_layers)
    n = ghost_size_;
  field.allocate(grids_data_double_[level].get_splitting(), IJK_Splitting::ELEM, ghost_size_, n);
}

IJK_Field_double & Multigrille_Adrien::get_storage_double(StorageId id, int level)
{
  switch (id) {
  case STORAGE_RHS: return grids_data_double_[level].get_update_rhs();
  case STORAGE_X:   return grids_data_double_[level].get_update_x();
  case STORAGE_RESIDUE: return grids_data_double_[level].get_update_residue();
  default:
    Cerr << "Error in Multigrille_Adrien::get_storage_double" << finl;
    Process::exit();
  }
  return grids_data_double_[level].get_update_rhs(); // never arrive here...
}

void Multigrille_Adrien::set_rho_double(const IJK_Field_double & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_double_.size();
  const int ghost = grids_data_double_[0].get_rho().ghost();

  for (int l = 0; l < nlevels; l++)
  {
    // Fill the "rho" field
    if (l == 0)
	 {
      IJK_Field_double & r = grids_data_double_[l].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i, j, k;
      for (k = 0; k < nk; k++) 
		  for (j = 0; j < nj; j++)
			 for (i = 0; i < ni; i++)
				r(i,j,k) = (double)rho(i,j,k);
    }
    else
      coarsen_operators_[l-1].valeur().coarsen(grids_data_double_[l-1].get_rho(),
					       grids_data_double_[l].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_double_[l].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    grids_data_double_[l].compute_faces_coefficients_from_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_double_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_rho_double(const IJK_Field_float & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_double_.size();
  const int ghost = grids_data_double_[0].get_rho().ghost();

  for (int l = 0; l < nlevels; l++)
  {
    // Fill the "rho" field
    if (l == 0)
	 {
      IJK_Field_double & r = grids_data_double_[l].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i, j, k;
      for (k = 0; k < nk; k++) 
		  for (j = 0; j < nj; j++)
			 for (i = 0; i < ni; i++)
				r(i,j,k) = (double)rho(i,j,k);
    }
    else
      coarsen_operators_[l-1].valeur().coarsen(grids_data_double_[l-1].get_rho(),
					       grids_data_double_[l].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_double_[l].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    grids_data_double_[l].compute_faces_coefficients_from_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_double_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_inv_rho_double(const IJK_Field_double & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_double_.size();
  int i;
  
  const int ghost = grids_data_double_[0].get_rho().ghost();

  for (i = 0; i < nlevels; i++) {
    // Fill the "rho" field
    if (i == 0) {
      IJK_Field_double & r = grids_data_double_[i].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i2, j, k;
      for (k = 0; k < nk; k++) 
	for (j = 0; j < nj; j++)
	  for (i2 = 0; i2 < ni; i2++)
	    r(i2,j,k) = (double)rho(i2,j,k);
    } else
      coarsen_operators_[i-1].valeur().coarsen(grids_data_double_[i-1].get_rho(),
					       grids_data_double_[i].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_double_[i].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    grids_data_double_[i].compute_faces_coefficients_from_inv_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_double_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_inv_rho_double(const IJK_Field_float & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_double_.size();
  int i;
  
  const int ghost = grids_data_double_[0].get_rho().ghost();

  for (i = 0; i < nlevels; i++) {
    // Fill the "rho" field
    if (i == 0) {
      IJK_Field_double & r = grids_data_double_[i].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i2, j, k;
      for (k = 0; k < nk; k++) 
	for (j = 0; j < nj; j++)
	  for (i2 = 0; i2 < ni; i2++)
	    r(i2,j,k) = (double)rho(i2,j,k);
    } else
      coarsen_operators_[i-1].valeur().coarsen(grids_data_double_[i-1].get_rho(),
					       grids_data_double_[i].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_double_[i].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    grids_data_double_[i].compute_faces_coefficients_from_inv_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_double_[coarse_level].get_faces_coefficients());
  }
}
// Substract the average of the x field to get a zero average field.
void Multigrille_Adrien::prepare_secmem(IJK_Field_double & x) const
{
  double moyenne = somme_ijk(x);
  // if (Process::je_suis_maitre())
  //  Cout << "prepare secmem: somme initiale " << moyenne << finl;
  double nb_elem_tot = (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_I)
    * (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_J)
    * (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_K);
  double val = moyenne / nb_elem_tot;
  const int m = x.data().size_array();
  for (int i = 0; i < m; i++)
    x.data()[i] -= (double)val;
}

void Multigrille_Adrien::dump_lata(const Nom & field, const IJK_Field_double & data, int tstep) const
{
   /* const IJK_Grid_Geometry & g = grids_data_double_[0].get_grid_geometry();
   data.dumplata(field,
		g.get_node_coordinates(0),
		g.get_node_coordinates(1),
		g.get_node_coordinates(2),			      
		tstep); */
  Process::exit();
}

void Multigrille_Adrien::set_rho(const IJK_Field_double & rho)
{
  if (solver_precision_ == precision_double_)  
    set_rho_double(rho, true, false);
  else if (solver_precision_ == precision_float_)
    set_rho_float(rho, true, false);
  else {
    set_rho_double(rho, false, false);
    set_rho_float(rho, true, true);
  }  
}

void Multigrille_Adrien::set_inv_rho(const IJK_Field_double & inv_rho)
{
  if (solver_precision_ == precision_double_)  
    set_inv_rho_double(inv_rho, true, false);
  else if (solver_precision_ == precision_float_)
    set_inv_rho_float(inv_rho, true, false);
  else {
    set_inv_rho_double(inv_rho, false, false);
    set_inv_rho_float(inv_rho, true, true);
  }  
}


// Apply n_jacobi iterations of jacobi to x vector,
//  and, if the residu pointer is not zero, computes the residue.
// Precondition: ghost cells for secmem must be up to date.
//  The number of required ghost layers is n_jacobi + (1 if residu is required)
// Postcondition: ghost cells are not updated
void Multigrille_Adrien::jacobi_residu(IJK_Field_float & x,
				       const IJK_Field_float *secmem,
				       const int grid_level,
				       const int n_jacobi_tot,
				       IJK_Field_float *residu) const
{
  static Stat_Counter_Id jacobi_residu_counter_ = statistiques().new_counter(2, "multigrille : residu jacobi");
  
  statistiques().begin_count(jacobi_residu_counter_);

  const IJK_Field_local_float & coeffs_face = grids_data_float_[grid_level].get_faces_coefficients();

  IJ_layout layout(x);
  const float relax = (float)relax_jacobi(grid_level);
  // We can do at most this number of passes per sweep:
  const int nb_passes_max_per_sweep = x.ghost();
  // We have to do this number of passes in total:
  const int nb_passes_to_do_total = n_jacobi_tot + (residu ? 1 : 0);
  int nb_passes_done = 0;

  // Repeat as many sweeps as necessary:
  while (nb_passes_done < nb_passes_to_do_total) {

    int nb_passes_to_do = nb_passes_to_do_total - nb_passes_done;
    if (nb_passes_to_do > nb_passes_max_per_sweep)
      nb_passes_to_do = nb_passes_max_per_sweep;

    x.echange_espace_virtuel(nb_passes_to_do);

    if (grid_level == 0) {
      // Place counter here to avoid counting communication time:
      //statistiques().begin_count(counter);
      flop_count = 0;
    }

    const bool last_pass_is_residue = (nb_passes_done + nb_passes_to_do == n_jacobi_tot + 1);
    if (1)
      Multipass_Jacobigeneric_float(x, *residu, coeffs_face, *secmem, nb_passes_to_do, last_pass_is_residue, relax);
    else {
      for (int pass = 0; pass < nb_passes_to_do; pass++) {
	if (pass == nb_passes_to_do-1 && last_pass_is_residue) {
	  reference_kernel_float(x, coeffs_face, *secmem, *residu, relax, true);
	} else {
	  IJK_Field_float tmp = x;
	  reference_kernel_float(tmp, coeffs_face, *secmem, x, relax, false);
	}
      }
    }

    nb_passes_done += nb_passes_to_do;

    if (grid_level == 0) {
      //statistiques().end_count(counter, flop_count);
      flop_count = 0;
    }
  }
 	statistiques().end_count(jacobi_residu_counter_, (int)flop_count);
}

void Multigrille_Adrien::coarsen(const IJK_Field_float & fine, IJK_Field_float & coarse, int fine_level) const
{
  coarsen_operators_[fine_level].valeur().coarsen(fine, coarse);
}

// calcul de "fine -= interpolated(coarse)"
void Multigrille_Adrien::interpolate_sub_shiftk(const IJK_Field_float & coarse, IJK_Field_float & fine, int fine_level) const
{
  // Shift by maximum value:
  const int shift = needed_kshift_for_jacobi(fine_level) - fine.k_shift();
  coarsen_operators_[fine_level].valeur().interpolate_sub_shiftk(coarse, fine, shift);
}

void Multigrille_Adrien::completer_float(const IJK_Splitting & split)
{
  Cerr << "Multigrille_Adrien::completer_float" << finl;

  const int nb_operators = coarsen_operators_.size();
  const int nb_grids = nb_operators + 1;
  grids_data_float_.dimensionner(nb_grids);
  grids_data_float_[0].initialize(split, ghost_size_, nsweeps_jacobi_residu(0));
  int i;
  for (i = 0; i < nb_operators; i++) {
    coarsen_operators_[i].valeur().initialize_grid_data(grids_data_float_[i], grids_data_float_[i+1],
							nsweeps_jacobi_residu(i+1));
  }
  for (i = 0; i < nb_grids; i++) {
    const IJK_Field_float & g = grids_data_float_[i].get_rho();
    Journal() << "Grid level " << i << " local size: " << g.ni() << "x" << g.nj() << "x" << g.nk() << finl;
  }
}



// Allocate an array for the grid data
void Multigrille_Adrien::alloc_field(IJK_Field_float & field, int level, bool with_additional_layers) const
{
  if (level < 0 || level > grids_data_float_.size()) {
    Cerr << "Fatal: wrong level in alloc_field" << finl;
    Process::exit();
  }
  int n = 0;
  if (with_additional_layers)
    n = ghost_size_;
  field.allocate(grids_data_float_[level].get_splitting(), IJK_Splitting::ELEM, ghost_size_, n);
}

IJK_Field_float & Multigrille_Adrien::get_storage_float(StorageId id, int level)
{
  switch (id) {
  case STORAGE_RHS: return grids_data_float_[level].get_update_rhs();
  case STORAGE_X:   return grids_data_float_[level].get_update_x();
  case STORAGE_RESIDUE: return grids_data_float_[level].get_update_residue();
  default:
    Cerr << "Error in Multigrille_Adrien::get_storage_float" << finl;
    Process::exit();
  }
  return grids_data_float_[level].get_update_rhs(); // never arrive here...
}

void Multigrille_Adrien::set_rho_float(const IJK_Field_double & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_float_.size();
  const int ghost = grids_data_float_[0].get_rho().ghost();

  for (int l = 0; l < nlevels; l++)
  {
    // Fill the "rho" field
    if (l == 0)
	 {
      IJK_Field_float & r = grids_data_float_[l].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i, j, k;
      for (k = 0; k < nk; k++) 
		  for (j = 0; j < nj; j++)
			 for (i = 0; i < ni; i++)
				r(i,j,k) = (float)rho(i,j,k);
    }
    else
      coarsen_operators_[l-1].valeur().coarsen(grids_data_float_[l-1].get_rho(),
					       grids_data_float_[l].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_float_[l].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    if (use_coeffs_from_double && l < grids_data_double_.size())
      grids_data_float_[l].compute_faces_coefficients_from_double_coeffs(grids_data_double_[l]);
    else
      grids_data_float_[l].compute_faces_coefficients_from_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_float_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_rho_float(const IJK_Field_float & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_float_.size();
  const int ghost = grids_data_float_[0].get_rho().ghost();

  for (int l = 0; l < nlevels; l++)
  {
    // Fill the "rho" field
    if (l == 0)
	 {
      IJK_Field_float & r = grids_data_float_[l].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i, j, k;
      for (k = 0; k < nk; k++) 
		  for (j = 0; j < nj; j++)
			 for (i = 0; i < ni; i++)
				r(i,j,k) = (float)rho(i,j,k);
    }
    else
      coarsen_operators_[l-1].valeur().coarsen(grids_data_float_[l-1].get_rho(),
					       grids_data_float_[l].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_float_[l].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    if (use_coeffs_from_double && l < grids_data_double_.size())
      grids_data_float_[l].compute_faces_coefficients_from_double_coeffs(grids_data_double_[l]);
    else
      grids_data_float_[l].compute_faces_coefficients_from_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_float_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_inv_rho_float(const IJK_Field_double & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_float_.size();
  int i;
  
  const int ghost = grids_data_float_[0].get_rho().ghost();

  for (i = 0; i < nlevels; i++) {
    // Fill the "rho" field
    if (i == 0) {
      IJK_Field_float & r = grids_data_float_[i].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i2, j, k;
      for (k = 0; k < nk; k++) 
	for (j = 0; j < nj; j++)
	  for (i2 = 0; i2 < ni; i2++)
	    r(i2,j,k) = (float)rho(i2,j,k);
    } else
      coarsen_operators_[i-1].valeur().coarsen(grids_data_float_[i-1].get_rho(),
					       grids_data_float_[i].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_float_[i].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    if (use_coeffs_from_double && i < grids_data_double_.size())
      grids_data_float_[i].compute_faces_coefficients_from_double_coeffs(grids_data_double_[i]);
    else
      grids_data_float_[i].compute_faces_coefficients_from_inv_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_float_[coarse_level].get_faces_coefficients());
  }
}
void Multigrille_Adrien::set_inv_rho_float(const IJK_Field_float & rho, bool set_coarse_matrix_flag, bool use_coeffs_from_double)
{
  // size might be 1 for mixed precision solver if updating the double precision part:
  const int nlevels = grids_data_float_.size();
  int i;
  
  const int ghost = grids_data_float_[0].get_rho().ghost();

  for (i = 0; i < nlevels; i++) {
    // Fill the "rho" field
    if (i == 0) {
      IJK_Field_float & r = grids_data_float_[i].get_update_rho();
      int ni = r.ni();
      int nj = r.nj();
      int nk = r.nk();
      int i2, j, k;
      for (k = 0; k < nk; k++) 
	for (j = 0; j < nj; j++)
	  for (i2 = 0; i2 < ni; i2++)
	    r(i2,j,k) = (float)rho(i2,j,k);
    } else
      coarsen_operators_[i-1].valeur().coarsen(grids_data_float_[i-1].get_rho(),
					       grids_data_float_[i].get_update_rho(),
					       1 /* compute average, not sum */);

    grids_data_float_[i].get_update_rho().echange_espace_virtuel(ghost);

    // Compute matrix coefficients at faces
    if (use_coeffs_from_double && i < grids_data_double_.size())
      grids_data_float_[i].compute_faces_coefficients_from_double_coeffs(grids_data_double_[i]);
    else
      grids_data_float_[i].compute_faces_coefficients_from_inv_rho();
  }

  // Update coarse problem matrix:
  if (set_coarse_matrix_flag) {
    const int coarse_level = nlevels - 1;
    set_coarse_matrix().build_matrix(grids_data_float_[coarse_level].get_faces_coefficients());
  }
}
// Substract the average of the x field to get a zero average field.
void Multigrille_Adrien::prepare_secmem(IJK_Field_float & x) const
{
  double moyenne = somme_ijk(x);
  // if (Process::je_suis_maitre())
  //  Cout << "prepare secmem: somme initiale " << moyenne << finl;
  double nb_elem_tot = (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_I)
    * (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_J)
    * (double) x.get_splitting().get_nb_items_global(IJK_Splitting::ELEM, DIRECTION_K);
  double val = moyenne / nb_elem_tot;
  const int m = x.data().size_array();
  for (int i = 0; i < m; i++)
    x.data()[i] -= (float)val;
}

void Multigrille_Adrien::dump_lata(const Nom & field, const IJK_Field_float & data, int tstep) const
{
   /* const IJK_Grid_Geometry & g = grids_data_float_[0].get_grid_geometry();
   data.dumplata(field,
		g.get_node_coordinates(0),
		g.get_node_coordinates(1),
		g.get_node_coordinates(2),			      
		tstep); */
  Process::exit();
}

void Multigrille_Adrien::set_rho(const IJK_Field_float & rho)
{
  if (solver_precision_ == precision_double_)  
    set_rho_double(rho, true, false);
  else if (solver_precision_ == precision_float_)
    set_rho_float(rho, true, false);
  else {
    set_rho_double(rho, false, false);
    set_rho_float(rho, true, true);
  }  
}

void Multigrille_Adrien::set_inv_rho(const IJK_Field_float & inv_rho)
{
  if (solver_precision_ == precision_double_)  
    set_inv_rho_double(inv_rho, true, false);
  else if (solver_precision_ == precision_float_)
    set_inv_rho_float(inv_rho, true, false);
  else {
    set_inv_rho_double(inv_rho, false, false);
    set_inv_rho_float(inv_rho, true, true);
  }  
}
// Says that the Poisson equation has variable coefficient rho, given at elements
// Must be called in parallel.
// (the function converts the given field to ijk ordering, computes the coarsened
//  rho fields, the matrix coefficients with boundary conditions for all levels 
//  and the coarse matrix for the coarse grid solver).
void Multigrille_Adrien::set_rho(const DoubleVect & rho)
{
  static Stat_Counter_Id counter = statistiques().new_counter(0, "Multigrille_Adrien::set_rho");
  statistiques().begin_count(counter);
  if (solver_precision_ != precision_float_) {
    IJK_Field_double ijk_rho(grids_data_double_[0].get_update_rho());
    convert_to_ijk(rho, ijk_rho);
    set_rho(ijk_rho);
  } else {
    IJK_Field_float ijk_rho(grids_data_float_[0].get_update_rho());
    convert_to_ijk(rho, ijk_rho);
    set_rho(ijk_rho);
  }
  statistiques().end_count(counter);
}



int Multigrille_Adrien::needed_kshift_for_jacobi(int level) const
{
  return nsweeps_jacobi_residu(level);
}

void Multigrille_Adrien::completer_double_for_residue(const IJK_Splitting & splitting)
{
  Cerr << "Multigrille_Adrien::completer_double_for_residue" << finl;
  grids_data_double_.dimensionner(1);
  grids_data_double_[0].initialize(splitting, ghost_size_, nsweeps_jacobi_residu(0));
}


#if 0
// Line smoother in direction k, and compute residue.
// This smoother is designed to converge quickly for meshes that are much finer in the k direction
// than in other directions.
// Consider the 1D implicit problem for each line of cells in k direction.
// Assume x_n(i,j,k) contains an approximation of the solution, solve for x_{n+1}(i,j,k) 
// such that
//   A_(i,j,k-1/2) * x_{n+1}(i,j,k-1) + A_(i,j,k+1/2) * x_{n+1}(i,j,k+1)   => implicit in k
// + A_(i,j-1/2,k) * x_{n}(i,j-1,k)    + A_(i,j+1/2,k) * x_{n}(i,j+1,k)    => explicit in j
// + A_(i-1/2,j,k) * x_{n}(i-1,j,k)    + A_(i+1/2,j,k) * x_{n}(i+1,j,k)    => explicit in i
// - SUM(A_) * x_{n+1}(i,j,k) = secmem_(i,j,k)
void Multigrille_Adrien::line_smoother_k_residu(IJK_Field__TYPE_ & x,
						const IJK_Field__TYPE_ &secmem,
						const int grid_level,
						IJK_Field__TYPE_ & residu) const
{
  // Scanning downward:
  {
    // Transform problem into upper diagnal matrix problem
    // Original matrix:
    //  a11 a12  0   0 ...
    //  a12 a22 a23  0 ... 
    //  0   a23 a33 a34 ...
    // goes to
    //  b11 b12  0   0 ...
    //  0   b22 b23  0 ...  where b22 = a22 - b12*(a12/b11) , b23 = a23 - 0 * (a12/b11) = a23
    //  0   0   b33  b34   
    // store the new diagonal coefficient into residu 
    // and the new right hand side into x
    residu(i,j,k) = diag(i,j,k) * fact
  }
  // Scanning upward, compute result and residue

}
#endif
