/** Copyright (C) 2019
 * All Rights Reserved.
 *
 * Authors: Xinyi Luo
 * Date:2019-07
 */
#include <sqphot/QPhandler.hpp>


namespace SQPhotstart {
using namespace std;

QPhandler::QPhandler(NLPInfo nlp_info, shared_ptr<const Options> options,
                     Ipopt::SmartPtr<Ipopt::Journalist> jnlst) :
    nlp_info_(nlp_info),
    jnlst_(jnlst),
    QPsolverChoice_(options->QPsolverChoice) {
#if NEW_FORMULATION
    nConstr_QP_ = nlp_info.nCon+nlp_info.nVar;
    nVar_QP_ = nlp_info.nVar*3+2*nlp_info.nCon;
    I_info_A_.length = 4;
    I_info_A_.irow = new int[4];
    I_info_A_.jcol = new int[4];
    I_info_A_.size = new int[4];
    I_info_A_.value = new double[4];
    I_info_A_.irow[0] = I_info_A_.irow[1] = 1;
    I_info_A_.irow[2] = I_info_A_.irow[3] = nlp_info.nCon+1;
    I_info_A_.jcol[0] = nlp_info.nVar+1;
    I_info_A_.jcol[1] = nlp_info_.nVar+nlp_info.nCon+1;
    I_info_A_.jcol[2] = nlp_info_.nVar+nlp_info.nCon*2+1;
    I_info_A_.jcol[3] = nlp_info_.nVar*2+nlp_info.nCon*2+1;
    I_info_A_.size[0] = I_info_A_.size[1] = nlp_info.nCon;
    I_info_A_.size[2] = I_info_A_.size[3] = nlp_info.nVar;

#else
    nConstr_QP_ = nlp_info.nCon;
    nVar_QP_ = nlp_info.nVar+2*nlp_info.nCon;
    I_info_A_.length = 2;
    I_info_A_.irow = new int[2];
    I_info_A_.jcol = new int[2];
    I_info_A_.size = new int[2];
    I_info_A_.value = new double[2];
    I_info_A_.irow[0] = I_info_A_.irow[1] = 1;
    I_info_A_.jcol[0] = nlp_info.nVar+1;
    I_info_A_.jcol[1] = nlp_info_.nVar+nlp_info.nCon+1;
    I_info_A_.size[0] = I_info_A_.size[1] = nlp_info.nCon;
#endif

    W_b_ = new ActiveType[nVar_QP_];
    W_c_ = new ActiveType[nConstr_QP_];


    switch (QPsolverChoice_) {
    case QPOASES:
        solverInterface_ = make_shared<qpOASESInterface>(nlp_info, QP, options,
                           jnlst);
        break;
    case QORE:
        solverInterface_ = make_shared<QOREInterface>(nlp_info, QP, options, jnlst);
        break;
    case GUROBI:
#ifdef USE_GUROBI
        solverInterface_ = make_shared<GurobiInterface>(nlp_info, QP, options, jnlst);
#endif

        break;
    case CPLEX:
#ifdef USE_CPLEX
        solverInterface_ = make_shared<CplexInterface>(nlp_info, QP, options, jnlst);
#endif
        break;
    }

#if DEBUG
#if COMPARE_QP_SOLVER
    qpOASESInterface_ = make_shared<qpOASESInterface>(nlp_info, QP,options);
    QOREInterface_= make_shared<QOREInterface>(nlp_info,QP,options,jnlst);
    W_b_qpOASES_ = new ActiveType[nVar_QP_];
    W_c_qpOASES_ = new ActiveType[nConstr_QP_];
    W_b_qore_ = new ActiveType[nVar_QP_];
    W_c_qore_ = new ActiveType[nConstr_QP_];

#endif
#endif
}




/**
 *Default destructor
 */
QPhandler::~QPhandler() {
    delete[] W_b_;
    W_b_ = NULL;
    delete[] W_c_;
    W_c_ = NULL;
    delete[] I_info_A_.irow;
    I_info_A_.irow = NULL;
    delete[] I_info_A_.jcol;
    I_info_A_.jcol = NULL;
    delete[] I_info_A_.size;
    I_info_A_.size = NULL;
    delete[] I_info_A_.value;
    I_info_A_.value = NULL;
#if DEBUG
#if COMPARE_QP_SOLVER
    delete[] W_b_qpOASES_;
    delete[] W_c_qpOASES_;
    delete[] W_b_qore_;
    delete[] W_c_qore_;
#endif
#endif

}


/**
 * Get the optimal solution from the QPhandler_interface
 *
 *This is only an interface for user to avoid call interface directly.
 * @param p_k       the pointer to an empty array with the length equal to the size
 *                  of the QP subproblem
 */
double* QPhandler::get_optimal_solution() {
    return solverInterface_->get_optimal_solution();
}


/**
 *Get the multipliers from the QPhandler_interface
 *
 *This is only an interface for user to avoid call interface directly.
 *
 * @param y_k       the pointer to an empty array with the length equal to the size of
 * multipliers of the QP subproblem
 */
double*  QPhandler::get_multipliers_bounds() {
    return solverInterface_->get_multipliers_bounds();
}


double* QPhandler::get_multipliers_constr() {
    return solverInterface_->get_multipliers_constr();
}

/**
 * Setup the bounds for the QP subproblems according to the information from current
 * iterate. We have
 * 	c_l -c_k <=J_p+ u-v<=c_u-c_k
 * The bound is formulated as
 *   max(-delta, x_l-x_k)<=p<=min(delta,x_u-x_k)
 * and  u,v>=0
 * @param delta      trust region radius
 * @param x_k        current iterate point
 * @param c_k        current constraint value evaluated at x_k
 * @param x_l        the lower bounds for variables
 * @param x_u        the upper bounds for variables
 * @param c_l        the lower bounds for constraints
 * @param c_u        the upper bounds for constraints
 */


void QPhandler::set_bounds(double delta, shared_ptr<const Vector> x_l,
                           shared_ptr<const Vector> x_u, shared_ptr<const Vector> x_k,
                           shared_ptr<const Vector> c_l, shared_ptr<const Vector> c_u,
                           shared_ptr<const Vector> c_k) {


#if DEBUG
#if COMPARE_QP_SOLVER
    set_bounds_debug(delta, x_l, x_u, x_k, c_l, c_u, c_k);
#endif
#endif

    /*-------------------------------------------------------------*/
    /* Set lbA, ubA as well as lb and ub as qpOASES differentiates */
    /*the bound constraints from the linear constraints            */
    /*-------------------------------------------------------------*/
    if(QPsolverChoice_!=QORE) {
        for (int i = 0; i < nlp_info_.nCon; i++) {
            solverInterface_->set_lbA(i, c_l->values(i) - c_k->values(i));//must
            // place before set_ubA
            solverInterface_->set_ubA(i, c_u->values(i) - c_k->values(i));
        }
#if not NEW_FORMULATION
        for (int i = 0; i < nlp_info_.nVar; i++) {
            solverInterface_->set_lb(i, std::max(
                                         x_l->values(i) - x_k->values(i), -delta));
            solverInterface_->set_ub(i, std::min(
                                         x_u->values(i) - x_k->values(i), delta));
        }
#endif
        /**
         * only set the upper bound for the last half to be infinity(those are slack variables).
         * The lower bounds are initialized as 0
         */
        for (int i = 0; i < nlp_info_.nCon * 2; i++)
            solverInterface_->set_ub(nlp_info_.nVar + i, INF);

#if NEW_FORMULATION
        for(int i = 0; i< nConstr_QP_-nlp_info_.nCon; i++) {
            solverInterface_->set_lbA(i+nlp_info_.nCon, x_l->values(i)
                                      - x_k->values(i));
            solverInterface_->set_ubA(i+nlp_info_.nCon, x_u->values(i)
                                      - x_k->values(i-nlp_info_.nCon));
        }

        for (int i = 0; i < nlp_info_.nVar * 2; i++)
            solverInterface_->set_ub(nlp_info_.nVar +2*nlp_info_.nCon+i, INF);
#endif

    }

    /*-------------------------------------------------------------*/
    /* Only set lb and ub, where lb = [lbx;lbA]; and ub=[ubx; ubA] */
    /*-------------------------------------------------------------*/
    else {
#if not NEW_FORMULATION
        for (int i = 0; i < nlp_info_.nVar; i++) {
            solverInterface_->set_lb(i, std::max(
                                         x_l->values(i) - x_k->values(i), -delta));
            solverInterface_->set_ub(i, std::min(
                                         x_u->values(i) - x_k->values(i), delta));

        }

#endif

        for (int i = 0; i < nlp_info_.nCon * 2; i++)
            solverInterface_->set_ub(nlp_info_.nVar + i, INF);

        for (int i = 0; i < nlp_info_.nCon; i++) {
            solverInterface_->set_lb(nVar_QP_+i, c_l->values(i)- c_k->values(i));
            solverInterface_->set_ub(nVar_QP_+i, c_u->values(i)- c_k->values(i));
        }

#if NEW_FORMULATION
        for(int i = 0; i< nConstr_QP_-nlp_info_.nCon; i++) {
            solverInterface_->set_lb(i+nlp_info_.nCon, x_l->values(i)
                                     - x_k->values(i));
            solverInterface_->set_ub(i+nlp_info_.nCon, x_u->values(i)
                                     - x_k->values(i));
        }
        for (int i = 0; i < nlp_info_.nVar * 2; i++)
            solverInterface_->set_ub(nlp_info_.nVar +2*nlp_info_.nCon+i, INF);
        //DEBUG
        solverInterface_->getLb()->print("lb");
        solverInterface_->getUb()->print("ub");
#endif
    }
}


/**
 * This function sets up the object vector g of the QP problem
 * The (2*nCon+nVar) vector g_^T in QP problem will be the same as
 * [grad_f^T, rho* e^T], where the unit vector is of length (2*nCon).
 * @param grad      Gradient vector from nlp class
 * @param rho       Penalty Parameter
 */

void QPhandler::set_g(shared_ptr<const Vector> grad, double rho) {

#if DEBUG
#if COMPARE_QP_SOLVER
    for (int i = 0; i < nVar_QP_; i++)
        if (i < nlp_info_.nVar) {
            qpOASESInterface_->set_g(i, grad->values(i));
            QOREInterface_->set_g(i, grad->values(i));
        }
        else {
            qpOASESInterface_->set_g(i, rho);
            QOREInterface_->set_g(i,rho);
        }
#endif
#endif
    for (int i = 0; i < nVar_QP_; i++)
        if (i < nlp_info_.nVar)
            solverInterface_->set_g(i, grad->values(i));

        else
            solverInterface_->set_g(i, rho);

    //DEBUG
    solverInterface_->getG()->print("G");

}

/**
 * @brief Set up the H for the first time in the QP problem.
 * It will be concatenated as [H_k 0]
 *          		         [0   0]
 * where H_k is the Lagragian hessian evaluated at x_k and lambda_k.
 *
 * This method should only be called for once.
 *
 * @param hessian the Lagragian hessian evaluated at x_k and lambda_k from nlp
 * readers.
 */
void QPhandler::set_H(shared_ptr<const SpTripletMat> hessian) {
#if DEBUG
#if COMPARE_QP_SOLVER
    qpOASESInterface_->set_H_structure(hessian);
    qpOASESInterface_->set_H_values(hessian);
    QOREInterface_->set_H_structure(hessian);
    QOREInterface_->set_H_values(hessian);
#endif
#endif
    if(QPsolverChoice_==GUROBI||QPsolverChoice_==CPLEX) {
        solverInterface_->set_H_values(hessian);
    }
    else {
        solverInterface_->set_H_structure(hessian);
        solverInterface_->set_H_values(hessian);
    }
}


/**
 * @brief This function sets up the matrix A in the QP subproblem
 * The matrix A in QP problem will be concatenate as [J I -I]
 * @param jacobian  the Matrix object for Jacobian from c(x)
 */
void QPhandler::set_A(shared_ptr<const SpTripletMat> jacobian) {
#if DEBUG
#if COMPARE_QP_SOLVER
    qpOASESInterface_->set_A_structure(jacobian, I_info_A_);
    qpOASESInterface_->set_A_values(jacobian, I_info_A_);
    QOREInterface_->set_A_structure(jacobian, I_info_A_);
    QOREInterface_->set_A_values(jacobian, I_info_A_);
#endif
#endif
    if(QPsolverChoice_==GUROBI||QPsolverChoice_==CPLEX)
        solverInterface_->set_A_values(jacobian, I_info_A_);
    else {
        solverInterface_->set_A_structure(jacobian, I_info_A_);
        solverInterface_->set_A_values(jacobian, I_info_A_);
    }
}



/**
 * @brief This function updates the constraint if there is any changes to
 * the iterates
 */
void QPhandler::update_bounds(double delta, shared_ptr<const Vector> x_l,
                              shared_ptr<const Vector> x_u,
                              shared_ptr<const Vector> x_k,
                              shared_ptr<const Vector> c_l,
                              shared_ptr<const Vector> c_u,
                              shared_ptr<const Vector> c_k) {
#if DEBUG
#if COMPARE_QP_SOLVER
    set_bounds_debug(delta, x_l, x_u, x_k, c_l, c_u, c_k);
#endif
#endif
    if(QPsolverChoice_ == QPOASES||QPsolverChoice_==GUROBI||QPsolverChoice_==CPLEX) {
        if(QPsolverChoice_==GUROBI||QPsolverChoice_==CPLEX)
            solverInterface_->reset_constraints();

        for (int i = 0; i < nlp_info_.nCon; i++) {
            solverInterface_->set_lbA(i, c_l->values(i) - c_k->values(i));
        }

        for (int i = 0; i < nlp_info_.nVar; i++) {
            solverInterface_->set_lb(i, std::max(
                                         x_l->values(i) - x_k->values(i), -delta));
            solverInterface_->set_ub(i, std::min(
                                         x_u->values(i) - x_k->values(i), delta));
        }
    }
    else if(QPsolverChoice_ == QORE) {
        for (int i = 0; i < nlp_info_.nVar; i++) {
            solverInterface_->set_lb(i, std::max(
                                         x_l->values(i) - x_k->values(i), -delta));
            solverInterface_->set_ub(i, std::min(
                                         x_u->values(i) - x_k->values(i), delta));

        }
        for (int i = 0; i < nlp_info_.nCon; i++) {
            solverInterface_->set_lb(nlp_info_.nVar +2*nlp_info_.nCon+i, c_l->values(i)
                                     - c_k->values(i));
            solverInterface_->set_ub(nlp_info_.nVar +2*nlp_info_.nCon+i, c_u->values(i)
                                     - c_k->values(i));
        }
    }
}


/**
 * @brief This function updates the vector g in the QP subproblem when there are any
 * change to the values of penalty parameter
 *
 * @param rho               penalty parameter
 * @param nVar              number of variables in NLP
 */

void QPhandler::update_penalty(double rho) {
#if DEBUG
#if COMPARE_QP_SOLVER
    for (int i = nlp_info_.nVar; i < nlp_info_.nVar + nlp_info_.nCon * 2; i++) {
        qpOASESInterface_->set_g(i, rho);
        QOREInterface_->set_g(i, rho);
    }
#endif
#endif
    for (int i = nlp_info_.nVar; i < nVar_QP_; i++)
        solverInterface_->set_g(i, rho);
}


/**
 * @brief This function updates the vector g in the QP subproblem when there are any
 * change to the values of gradient in NLP
 *
 * @param grad              the gradient vector from NLP
 */
void QPhandler::update_grad(shared_ptr<const Vector> grad) {

#if DEBUG
#if COMPARE_QP_SOLVER

    for (int i = 0; i < nlp_info_.nVar; i++) {
        qpOASESInterface_->set_g(i, grad->values(i));
        QOREInterface_->set_g(i, grad->values(i));
    }
#endif
#endif
    for (int i = 0; i < nlp_info_.nVar; i++)
        solverInterface_->set_g(i, grad->values(i));
}



/**
 *@brief Solve the QP with objective and constraints defined by its class members
 */
void QPhandler::solveQP(shared_ptr<SQPhotstart::Stats> stats,
                        shared_ptr<Options> options) {


#if DEBUG
#if COMPARE_QP_SOLVER
    QOREInterface_->optimizeQP(stats);
    qpOASESInterface_->optimizeQP(stats);
    bool qpOASES_optimal = OptimalityTest(qpOASESInterface_,QPOASES,W_b_qpOASES_,W_c_qpOASES_);
    bool qore_optimal = OptimalityTest(QOREInterface_,QORE,W_b_qore_,W_c_qore_);
    if(!qpOASES_optimal||!qore_optimal)
        testQPsolverDifference();

#endif
#endif

    solverInterface_->optimizeQP(stats);

    //manually check if the optimality condition is satisfied
    bool isOptimal= test_optimality(solverInterface_, QPsolverChoice_, W_b_, W_c_);
    if(!isOptimal) {
        THROW_EXCEPTION(QP_NOT_OPTIMAL,QP_NOT_OPTIMAL_MSG);
    }
}


double QPhandler::get_objective() {

    return solverInterface_->get_obj_value();
}


void QPhandler::update_H(shared_ptr<const SpTripletMat> Hessian) {

#if DEBUG
#if COMPARE_QP_SOLVER
    QOREInterface_->set_H_values(Hessian);
    qpOASESInterface_->set_H_values(Hessian);
#endif
#endif
    solverInterface_->set_H_values(Hessian);
}


void QPhandler::update_A(shared_ptr<const SpTripletMat> Jacobian) {

#if DEBUG
#if COMPARE_QP_SOLVER
    QOREInterface_->set_A_values(Jacobian, I_info_A_);
    qpOASESInterface_->set_A_values(Jacobian, I_info_A_);
#endif
#endif
    solverInterface_->set_A_values(Jacobian, I_info_A_);

}


void QPhandler::update_delta(double delta, shared_ptr<const Vector> x_l,
                             shared_ptr<const Vector> x_u,
                             shared_ptr<const Vector> x_k) {

#if DEBUG
#if COMPARE_QP_SOLVER
    for (int i = 0; i < nlp_info_.nVar; i++) {
        qpOASESInterface_->set_lb(i, std::max(
                                      x_l->values(i) - x_k->values(i), -delta));
        qpOASESInterface_->set_ub(i, std::min(
                                      x_u->values(i) - x_k->values(i), delta));
        QOREInterface_->set_lb(i, std::max(
                                   x_l->values(i) - x_k->values(i), -delta));
        QOREInterface_->set_ub(i, std::min(
                                   x_u->values(i) - x_k->values(i), delta));
    }
#endif
#endif
    for (int i = 0; i < nlp_info_.nVar; i++) {
        solverInterface_->set_lb(i, std::max(
                                     x_l->values(i) - x_k->values(i), -delta));
        solverInterface_->set_ub(i, std::min(
                                     x_u->values(i) - x_k->values(i), delta));
    }

}

void QPhandler::WriteQPData(const string filename ) {

    solverInterface_->WriteQPDataToFile(Ipopt::J_LAST_LEVEL, Ipopt::J_USER1,filename);

}

Exitflag QPhandler::get_status() {
    return (solverInterface_->get_status());
}


bool QPhandler::test_optimality(
    shared_ptr<QPSolverInterface> qpsolverInterface,
    Solver qpSolver,
    ActiveType* W_b,
    ActiveType* W_c) {

    qpOptimalStatus_ = qpsolverInterface->get_optimality_status();
    return (qpsolverInterface->test_optimality(W_c,W_b));
}




double QPhandler::get_infea_measure_model() {
    return oneNorm(solverInterface_->get_optimal_solution()+nVar_QP_-2*nConstr_QP_,2*nConstr_QP_);
}

const OptimalityStatus &QPhandler::get_QpOptimalStatus() const {
    return qpOptimalStatus_;
}

void QPhandler::get_active_set(ActiveType* A_c, ActiveType* A_b, shared_ptr<Vector> x,
                               shared_ptr<Vector> Ax) {
    //use the class member to get the qp problem information
    auto lb = solverInterface_->getLb();
    auto ub = solverInterface_->getUb();
    if (x == nullptr) {
        x = make_shared<Vector>(nVar_QP_);
        x->copy_vector(get_optimal_solution());
    }
    if (Ax == nullptr) {
        Ax = make_shared<Vector>(nConstr_QP_);
        auto A = solverInterface_->getA();
        A->times(x, Ax);
    }

    for (int i = 0; i < nVar_QP_; i++) {
        if (abs(x->values(i) - lb->values(i)) < sqrt_m_eps) {
            if (abs(ub->values(i) - x->values(i)) < sqrt_m_eps) {
                A_b[i] = ACTIVE_BOTH_SIDE;
            } else
                A_b[i] = ACTIVE_BELOW;
        } else if (abs(ub->values(i) - x->values(i)) < sqrt_m_eps)
            A_b[i] = ACTIVE_ABOVE;
        else
            A_b[i] = INACTIVE;
    }
    if (QPsolverChoice_ == QORE) {
        //if no x and Ax are input
        for (int i = 0; i < nConstr_QP_; i++) {
            if (abs(Ax->values(i) - lb->values(i+nVar_QP_)) < sqrt_m_eps) {
                if (abs(ub->values(i + nVar_QP_) - Ax->values(i)) < sqrt_m_eps) {
                    A_c[i] = ACTIVE_BOTH_SIDE;
                } else
                    A_c[i] = ACTIVE_BELOW;
            } else if (abs(ub->values(i + nVar_QP_) - Ax->values(i)) < sqrt_m_eps)
                A_c[i] = ACTIVE_ABOVE;
            else
                A_c[i] = INACTIVE;
        }
    }
    else {
        auto lbA = solverInterface_->getUbA();
        auto ubA = solverInterface_->getUbA();
        for (int i = 0; i < nConstr_QP_; i++) {
            if (abs(Ax->values(i) - lbA->values(i)) < sqrt_m_eps) {
                if (abs(ubA->values(i) - Ax->values(i)) < sqrt_m_eps) {
                    A_c[i] = ACTIVE_BOTH_SIDE;
                } else
                    A_c[i] = ACTIVE_BELOW;
            } else if (abs(ubA->values(i) - Ax->values(i)) < sqrt_m_eps)
                A_c[i] = ACTIVE_ABOVE;
            else
                A_c[i] = INACTIVE;
        }
    }
}

#if DEBUG
#if COMPARE_QP_SOLVER
void QPhandler::set_bounds_debug(double delta, shared_ptr<const Vector> x_l,
                                 shared_ptr<const Vector> x_u,
                                 shared_ptr<const Vector> x_k,
                                 shared_ptr<const Vector> c_l,
                                 shared_ptr<const Vector> c_u,
                                 shared_ptr<const Vector> c_k) {

    for (int i = 0; i < nlp_info_.nCon; i++) {
        qpOASESInterface_->set_lbA(i, c_l->values(i) - c_k->values(i));
        qpOASESInterface_->set_ubA(i, c_u->values(i) - c_k->values(i));
    }

    for (int i = 0; i < nlp_info_.nVar; i++) {
        qpOASESInterface_->set_lb(i, std::max(
                                      x_l->values(i) - x_k->values(i), -delta));
        qpOASESInterface_->set_ub(i, std::min(
                                      x_u->values(i) - x_k->values(i), delta));
    }
    /**
     * only set the upper bound for the last 2*nCon entries (those are slack variables).
     * The lower bounds are initialized as 0
     */
    for (int i = 0; i < nlp_info_.nCon * 2; i++)
        qpOASESInterface_->set_ub(nlp_info_.nVar + i, INF);


    /*-------------------------------------------------------------*/
    /* Only set lb and ub, where lb = [lbx;lbA]; and ub=[ubx; ubA] */
    /*-------------------------------------------------------------*/
    for (int i = 0; i < nlp_info_.nVar; i++) {
        QOREInterface_->set_lb(i, std::max(
                                   x_l->values(i) - x_k->values(i), -delta));
        QOREInterface_->set_ub(i, std::min(
                                   x_u->values(i) - x_k->values(i), delta));

    }

    for (int i = 0; i < nConstr_QP_ * 2; i++)
        QOREInterface_->set_ub(nlp_info_.nVar + i, INF);

    for (int i = 0; i < nConstr_QP_; i++) {
        QOREInterface_->set_lb(nVar_QP_+i, c_l->values(i)
                               - c_k->values(i));
        QOREInterface_->set_ub(nVar_QP_+i, c_u->values(i)
                               - c_k->values(i));
    }

}

bool QPhandler::testQPsolverDifference() {
    shared_ptr<Vector> qpOASESsol = make_shared<Vector>(nlp_info_.nVar);
    shared_ptr<Vector> QOREsol = make_shared<Vector>(nlp_info_.nVar);
    shared_ptr<Vector> difference = make_shared<Vector>(nlp_info_.nVar);
    qpOASESsol->print("qpOASESsol",jnlst_);
    QOREsol->print("QOREsol",jnlst_);
    qpOASESsol->copy_vector(qpOASESInterface_->get_optimal_solution());
    QOREsol->copy_vector(QOREInterface_->get_optimal_solution());
    difference->copy_vector(qpOASESsol);
    difference->subtract_vector(QOREsol->values());
    double diff_norm = difference->getOneNorm();
    if(diff_norm>1.0e-8) {
        printf("difference is %10e\n",diff_norm);
        qpOASESsol->print("qpOASESsol");
        QOREsol->print("QOREsol");
        QOREInterface_->WriteQPDataToFile(jnlst_,J_ALL,J_DBG);
    }
    assert(diff_norm<1.0e-8);
    return true;

}

#endif
#endif
} // namespace SQPhotstart







