/*
 * Copyright (C) 2009-2010 Dynare Team
 *
 * This file is part of Dynare.
 *
 * Dynare is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Dynare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Dynare.  If not, see <http://www.gnu.org/licenses/>.
 */

///////////////////////////////////////////////////////////
//  KalmanFilter.cpp
//  Implementation of the Class KalmanFilter
//  Created on:      02-Feb-2010 12:44:41
///////////////////////////////////////////////////////////

#include "KalmanFilter.hh"

KalmanFilter::~KalmanFilter()
{

}

KalmanFilter::KalmanFilter(const std::string &modName, size_t n_endo, size_t n_exo,
                           const std::vector<size_t> &zeta_fwrd_arg, const std::vector<size_t> &zeta_back_arg,
                           const std::vector<size_t> &zeta_mixed_arg, const std::vector<size_t> &zeta_static_arg,
                           double qz_criterium_arg, const std::vector<size_t> &varobs_arg,
                           double riccati_tol_arg, double lyapunov_tol_arg, int &info) :
  zeta_varobs_back_mixed(compute_zeta_varobs_back_mixed(zeta_back_arg, zeta_mixed_arg, varobs_arg)),
  Z(varobs_arg.size(), zeta_varobs_back_mixed.size()), T(zeta_varobs_back_mixed.size()), R(zeta_varobs_back_mixed.size(), n_exo),
  Pstar(zeta_varobs_back_mixed.size(), zeta_varobs_back_mixed.size()), Pinf(zeta_varobs_back_mixed.size(), zeta_varobs_back_mixed.size()), RQRt(zeta_varobs_back_mixed.size(), zeta_varobs_back_mixed.size()),
  Ptmp(zeta_varobs_back_mixed.size(), zeta_varobs_back_mixed.size()), F(varobs_arg.size(), varobs_arg.size()),
  Finv(varobs_arg.size(), varobs_arg.size()), K(zeta_varobs_back_mixed.size(), varobs_arg.size()), KFinv(zeta_varobs_back_mixed.size(), varobs_arg.size()),
  oldKFinv(zeta_varobs_back_mixed.size(), varobs_arg.size()), Finverter(varobs_arg.size()), a_init(zeta_varobs_back_mixed.size(), 1),
  a_new(zeta_varobs_back_mixed.size(), 1), vt(varobs_arg.size(), 1), vtFinv(1, varobs_arg.size()), vtFinvVt(1), riccati_tol(riccati_tol_arg),
  initKalmanFilter(modName, n_endo, n_exo, zeta_fwrd_arg, zeta_back_arg, zeta_mixed_arg,
                   zeta_static_arg, zeta_varobs_back_mixed, qz_criterium_arg, lyapunov_tol_arg, info)
{
  a_init.setAll(0.0);
  Z.setAll(0.0);
  for (size_t i = 0; i < varobs_arg.size(); ++i)
    {
      size_t j = find(zeta_varobs_back_mixed.begin(), zeta_varobs_back_mixed.end(),
                      varobs_arg[i]) - zeta_varobs_back_mixed.begin();
      Z(i, j) = 1.0;
    }
}

std::vector<size_t>
KalmanFilter::compute_zeta_varobs_back_mixed(const std::vector<size_t> &zeta_back_arg, const std::vector<size_t> &zeta_mixed_arg, const std::vector<size_t> &varobs_arg)
{
  std::vector<size_t> varobs_sorted = varobs_arg;
  sort(varobs_sorted.begin(), varobs_sorted.end());
  std::vector<size_t> zeta_back_mixed, zeta_varobs_back_mixed;
  set_union(zeta_back_arg.begin(), zeta_back_arg.end(),
            zeta_mixed_arg.begin(), zeta_mixed_arg.end(),
            back_inserter(zeta_back_mixed));
  set_union(zeta_back_mixed.begin(), zeta_back_mixed.end(),
            varobs_arg.begin(), varobs_arg.end(),
            back_inserter(zeta_varobs_back_mixed));
  return zeta_varobs_back_mixed;
}

double
KalmanFilter::compute(const MatrixConstView &dataView, VectorView &steadyState,
                      const Matrix &Q, const Matrix &H, const Vector &deepParams,
                      VectorView &vll, size_t start, size_t period, double &penalty, int &info)
{
  Matrix Y(dataView.getRows(), dataView.getCols());    // data

  if(period==0) // initialise all KF matrices
    initKalmanFilter.initialize(steadyState, deepParams, R, Q, RQRt, T, Pstar, Pinf,
                              penalty, dataView, Y, info);
  else  // initialise parameter dependent KF matrices only but not Ps
    initKalmanFilter.initialize(steadyState, deepParams, R, Q, RQRt, T, 
                              penalty, dataView, Y, info);

  return filter(Y, H, vll, start, info);

};

/**
 * 30:*
 */
double
KalmanFilter::filter(const Matrix &dataView,  const Matrix &H, VectorView &vll, size_t start, int &info)
{
  double loglik=0.0, ll, logFdet, Fdet;
  size_t p = Finv.getRows();

  bool nonstationary = true;
  for (size_t t = 0; t < dataView.getCols(); ++t)
    {
      if (nonstationary)
        {
          // K=PZ'
          blas::gemm("N", "T", 1.0, Pstar, Z, 0.0, K);

          //F=ZPZ' +H = ZK+H
          F = H;
          blas::gemm("N", "N", 1.0, Z, K, 1.0, F);
          // logFdet=log|F|

          // Finv=inv(F)
          mat::set_identity(Finv);
          Finverter.invMult("N", F, Finv); // F now contains its LU decomposition!
          // KFinv gain matrix
          blas::gemm("N", "N", 1.0, K, Finv, 0.0, KFinv);
          // deteminant of F:
          Fdet = 1;
          for (size_t d = 0; d < p; ++d)
            Fdet *= (-F(d, d));

          logFdet=log(fabs(Fdet));

          Ptmp = Pstar;
          // Pt+1= T(Pt - KFinvK')T' +RQR'
          // 1) Ptmp= Pt - K*FinvK'
          blas::gemm("N", "T", -1.0, KFinv, K, 1.0, Ptmp);
          // 2) Ptmp= T*Ptmp
          Pstar = Ptmp;
          blas::gemm("N", "N", 1.0, T, Pstar, 0.0, Ptmp);
          // 3) Pt+1= Ptmp*T' +RQR'
          Pstar = RQRt;
          blas::gemm("N", "T", 1.0, Ptmp, T, 1.0, Pstar);
          if (t>0)
            nonstationary = mat::isDiff(KFinv, oldKFinv, riccati_tol);
          oldKFinv=KFinv;
        }

      // err= Yt - Za
      MatrixConstView yt(dataView, 0, t, dataView.getRows(), 1); // current observation vector
      vt = yt;
      blas::gemm("N", "N", -1.0, Z, a_init, 1.0, vt);
      // at+1= T(at+ KFinv *err)
      blas::gemm("N", "N", 1.0, KFinv, vt, 1.0, a_init);
      blas::gemm("N", "N", 1.0, T, a_init, 0.0, a_new);
      a_init = a_new;

      /*****************
         Here we calc likelihood and store results.
      *****************/
      blas::gemm("T", "N", 1.0, vt, Finv, 0.0, vtFinv);
      blas::gemm("N", "N", 1.0, vtFinv, vt, 0.0, vtFinvVt);

      ll = -0.5*(p*log(2*M_PI)+logFdet+(*(vtFinvVt.getData())));

      vll(t) = ll;
      if (t >= start) loglik += ll;

    }

  return loglik;
}

