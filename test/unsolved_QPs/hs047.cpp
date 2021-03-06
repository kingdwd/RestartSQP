/*
 *	This file is part of qpOASES.
 *
 *	qpOASES -- An Implementation of the Online Active Set Strategy.
 *	Copyright (C) 2007-207 by Hans Joachim Ferreau, Andreas Potschka,
 *	Christian Kirches et al. All rights reserved.
 *
 *	qpOASES is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	qpOASES is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with qpOASES; if not, write to the Free Software
 *	Foundation, Inc., 31 Franklin Street, Fifth Floor, Boston, MA  02110-1101  USA
 *
 */


/**
 *	\file examples/qrecipe.cpp
 *	\author Andreas Potschka
 *	\version 3.2
 *	\date 2007-207
 *
 *	QRECIPE example from the CUTEr test set with sparse matrices.
 */



#include <qpOASES.hpp>

#include "hs047.hpp"



int main( )
{
    USING_NAMESPACE_QPOASES

    long i;
    int_t nWSR;
    real_t err, tic, toc;
    real_t *x = new real_t[11];
    real_t *y = new real_t[11+3];
    /* create sparse matrices */
    SymSparseMat *H = new SymSparseMat(11, 11, H_ir, H_jc, H_val);
    SparseMatrix *A = new SparseMatrix(3, 11, A_ir, A_jc, A_val);
    H->createDiagInfo();


    /* solve with sparse matrices */
    nWSR = 1000;
    QProblem QP(11, 3);
    Options options;
    options.printLevel = PL_DEBUG_ITER;
    QP.setOptions(options);
    tic = getCPUtime();
    QP.init(H, g, A, lb, ub, lbA, ubA, nWSR, 0);
    toc = getCPUtime();
    QP.getPrimalSolution(x);
    QP.getDualSolution(y);

    fprintf(stdFile, "Solved sparse problem in %d iterations, %.3f seconds.\n", (int)nWSR, toc-tic);


    delete H;
    delete A;

    delete[] y;
    delete[] x;

    return 0;
}


/*
 *	end of file
 */
