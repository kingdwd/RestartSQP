/* Copyright (C) 2019
 * All Rights Reserved.
 *
 * Authors: Xinyi Luo
 * Date:    2019-07
 */
#include <sqphot/qpOASESInterface.hpp>



namespace SQPhotstart {
/**
 * @brief Allocate memory for the class members
 * @param nlp_index_info  the struct that stores simple nlp dimension info
 * @param qptype is the problem to be solved QP or LP or SOC?
 * @return
 */
void qpOASESInterface::allocate(Index_info nlp_index_info, QPType qptype) {


    lbA_ = make_shared<Vector>(nConstr_QP_);
    ubA_ = make_shared<Vector>(nConstr_QP_);
    lb_ = make_shared<Vector>(nVar_QP_);
    ub_ = make_shared<Vector>(nVar_QP_);
    g_ = make_shared<Vector>(nVar_QP_);
    A_ = make_shared<SpHbMat>(
             nlp_index_info.nnz_jac_g + 2 * nlp_index_info.nCon, nConstr_QP_, nVar_QP_);
    x_qp_ = make_shared<Vector>(nVar_QP_);
    y_qp_ = make_shared<Vector>(nConstr_QP_+nVar_QP_);

    if (qptype != LP) {
        H_ = make_shared<SpHbMat>(nVar_QP_, nVar_QP_, true);
    }

#if DEBUG
#if GET_QP_INTERFACE_MEMBERS or COMPARE_QP_SOLVER
    A_triplet_ = make_shared<SpTripletMat>(nlp_index_info
                                           .nnz_jac_g+2*nlp_index_info.nCon,nConstr_QP_,
                                           nVar_QP_);
    H_triplet_ = make_shared<SpTripletMat>(nlp_index_info.nnz_h_lag,nConstr_QP_,
                                           nVar_QP_,true);
#endif
#endif
    //FIXME: the qpOASES does not accept any extra input
    solver_ = std::make_shared<qpOASES::SQProblem>((qpOASES::int_t) nVar_QP_,
              (qpOASES::int_t) nConstr_QP_);
}


/**
 * @brief Constructor which also initializes the qpOASES SQProblem objects
 * @param nlp_index_info the struct that stores simple nlp dimension info
 * @param qptype  is the problem to be solved QP or LP or SOC?
 */
qpOASESInterface::qpOASESInterface(Index_info nlp_index_info, QPType qptype,
                                   shared_ptr<const Options> options) :
    status_(UNSOLVED),
    nConstr_QP_(nlp_index_info.nCon),
    nVar_QP_(nlp_index_info.nVar+2*nlp_index_info.nCon),
    options_(options)
{

    //options_ = options;

    allocate(nlp_index_info, qptype);
}


/**Default destructor*/
qpOASESInterface::~qpOASESInterface() = default;


/**
 * @brief This function solves the QP problem specified in the data, with given
 * options.
 * After the QP being solved, it updates the stats, adding the iteration
 * number used to solve the QP to the qp_iter in object stats
 */
void
qpOASESInterface::optimizeQP(shared_ptr<Stats> stats) {

    qpOASES::int_t nWSR = options_->qp_maxiter;

    if (!firstQPsolved_) {//if haven't solve any QP before then initialize the first QP
        if (DEBUG) {
            if (CHECK_QP_INFEASIBILITY) {
//                A_qpOASES_->print("A_");
//                H_qpOASES_->print("H");
//                g_->print("g", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                lb_->print("lb_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                ub_->print("ub_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                lbA_->print("lbA_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                ubA_->print("ubA_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
            }
        }

        setQP_options();

        solver_->init(H_qpOASES_.get(), g_->values(), A_qpOASES_.get(), lb_->values(),
                      ub_->values(), lbA_->values(), ubA_->values(), nWSR);

        if (solver_->isSolved()) {
            firstQPsolved_ = true;
        }
        else {
            obtain_status();
            handler_error(QP, stats);
        }
    }
    else {
        if (DEBUG) {
            if (CHECK_QP_INFEASIBILITY) {
//                A_qpOASES_->print("A_");
//                H_qpOASES_->print("H");
//                g_->print("g", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                lb_->print("lb_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                ub_->print("ub_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                lbA_->print("lbA_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
//                ubA_->print("ubA_", jnlst_, Ipopt::J_DBG, Ipopt::J_ALL);;
            }
        }
        get_Matrix_change_status();
        if (new_QP_matrix_status_ == UNDEFINED) {
            assert(old_QP_matrix_status_ != UNDEFINED);
            if (old_QP_matrix_status_ == FIXED)
                solver_->hotstart(g_->values(), lb_->values(), ub_->values(),
                                  lbA_->values(),
                                  ubA_->values(), nWSR);
            else {
                solver_->hotstart(H_qpOASES_.get(), g_->values(), A_qpOASES_.get(),
                                  lb_->values(), ub_->values(), lbA_->values(),
                                  ubA_->values(), nWSR);

            }
        }
        else {
            if (new_QP_matrix_status_ == FIXED && old_QP_matrix_status_ == FIXED) {
                solver_->hotstart(g_->values(), lb_->values(), ub_->values(),
                                  lbA_->values(),
                                  ubA_->values(), nWSR);
            }
            else if (new_QP_matrix_status_ == VARIED &&
                     old_QP_matrix_status_ == VARIED) {
                solver_->hotstart(H_qpOASES_.get(), g_->values(), A_qpOASES_.get(),
                                  lb_->values(), ub_->values(), lbA_->values(),
                                  ubA_->values(), nWSR);
            }
            else if (new_QP_matrix_status_ != old_QP_matrix_status_) {
                solver_->init(H_qpOASES_.get(), g_->values(), A_qpOASES_.get(),
                              lb_->values(),
                              ub_->values(), lbA_->values(), ubA_->values(), nWSR);
                new_QP_matrix_status_ = old_QP_matrix_status_ = UNDEFINED;
            }
        }
    }

    reset_flags();

    stats->qp_iter_addValue((int) nWSR);

    solver_->getPrimalSolution(x_qp_->values());
    solver_->getDualSolution(y_qp_->values());
    if (!solver_->isSolved()) {
        obtain_status();
        handler_error(QP, stats);
    }

}


void qpOASESInterface::optimizeLP(shared_ptr<Stats> stats) {

    qpOASES::int_t nWSR = options_->lp_maxiter;//TODO modify it
    if (!firstQPsolved_) {
        setQP_options();
        solver_->init(0, g_->values(), A_qpOASES_.get(), lb_->values(),
                      ub_->values(), lbA_->values(), ubA_->values(), nWSR);
        if (solver_->isSolved()) {
            firstQPsolved_ = true;
        }
        else {
            obtain_status();
            handler_error(LP, stats);
        }
    }
    else {
        get_Matrix_change_status();
        if (new_QP_matrix_status_ == UNDEFINED) {
            if (old_QP_matrix_status_ == FIXED)
                solver_->hotstart(g_->values(), lb_->values(), ub_->values(),
                                  lbA_->values(),
                                  ubA_->values(), nWSR);
            else {
                solver_->hotstart(0, g_->values(), A_qpOASES_.get(),
                                  lb_->values(), ub_->values(), lbA_->values(),
                                  ubA_->values(), nWSR);
            }
        }

        else {
            if (new_QP_matrix_status_ == FIXED && old_QP_matrix_status_ == FIXED) {
                solver_->hotstart(g_->values(), lb_->values(), ub_->values(),
                                  lbA_->values(),
                                  ubA_->values(), nWSR);
            }
            else if (new_QP_matrix_status_ == VARIED &&
                     old_QP_matrix_status_ == VARIED) {
                solver_->hotstart(0, g_->values(), A_qpOASES_.get(),
                                  lb_->values(), ub_->values(), lbA_->values(),
                                  ubA_->values(), nWSR);
            }
            else if (new_QP_matrix_status_ != old_QP_matrix_status_) {
                solver_->init(0, g_->values(), A_qpOASES_.get(), lb_->values(),
                              ub_->values(), lbA_->values(), ubA_->values(), nWSR);
                new_QP_matrix_status_ = old_QP_matrix_status_ = UNDEFINED;
            }
        }
        reset_flags();
        solver_->getPrimalSolution(x_qp_->values());
        solver_->getDualSolution(y_qp_->values());
        if (!solver_->isSolved()) {
            obtain_status();
            handler_error(LP, stats);
        }
        stats->qp_iter_addValue((int) nWSR);
    }
}


/**
 * @brief copy the multipliers of the QP to the input pointer
 *
 * @param y_k   a pointer to an array with allocated memory equals to
 * sizeof(double)*(num_variable+num_constraint)
 */
inline double* qpOASESInterface::get_multipliers() {

    return y_qp_->values();
}


/**
 * @brief copy the optimal solution of the QP to the input pointer
 *
 * @param x_optimal a pointer to an empty array with allocated memory equals to
 * sizeof(double)*number_variables
 *
 */
inline double* qpOASESInterface::get_optimal_solution() {
//    x_qp_->print("x_qp_");
    return x_qp_->values();
}


/**
 *@brief get the objective value from the QP solvers
 *
 * @return the objective function value of the QP problem
 */


inline double qpOASESInterface::get_obj_value() {

    return (double) (solver_->getObjVal());
}


void qpOASESInterface::obtain_status() {

    qpOASES::QProblemStatus finalStatus = solver_->getStatus();

    if (solver_->isInfeasible()) {
        status_ = QP_INFEASIBLE;
    }
    else if (solver_->isUnbounded()) {
        status_ = QP_UNBOUNDED;
    }
    else
        switch (finalStatus) {
        case qpOASES::QPS_NOTINITIALISED:
            status_ = QP_NOTINITIALISED;
            break;
        case qpOASES::QPS_PREPARINGAUXILIARYQP:
            status_ = QP_PREPARINGAUXILIARYQP;
            break;
        case qpOASES::QPS_AUXILIARYQPSOLVED:
            status_ = QP_AUXILIARYQPSOLVED;
            break;
        case qpOASES::QPS_PERFORMINGHOMOTOPY:
            status_ = QP_PERFORMINGHOMOTOPY;
            break;
        case qpOASES::QPS_HOMOTOPYQPSOLVED:
            status_ = QP_HOMOTOPYQPSOLVED;
            break;
        }
}


QPReturnType qpOASESInterface::get_status() {

    return status_;
}


//@}
void qpOASESInterface::set_lb(int location, double value) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    lb_->setValueAt(location, value);
}


void qpOASESInterface::set_ub(int location, double value) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    ub_->setValueAt(location, value);
}


void qpOASESInterface::set_lbA(int location, double value) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    lbA_->setValueAt(location, value);
}


void qpOASESInterface::set_ubA(int location, double value) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    ubA_->setValueAt(location, value);
}


void qpOASESInterface::set_g(int location, double value) {

    if (firstQPsolved_ && !data_change_flags_.Update_g)
        data_change_flags_.Update_g = true;
    g_->setValueAt(location, value);
}


void qpOASESInterface::set_H_structure(shared_ptr<const SpTripletMat> rhs) {

#if DEBUG
#if GET_QP_INTERFACE_MEMBERS or COMPARE_QP_SOLVER
    H_triplet_->copy(rhs,false);
#endif
#endif
    H_->setStructure(rhs);//TODO: move to somewhere else?
    H_qpOASES_ = std::make_shared<qpOASES::SymSparseMat>(H_->RowNum(),
                 H_->ColNum(),
                 H_->RowIndex(),
                 H_->ColIndex(),
                 H_->MatVal());
}


void qpOASESInterface::set_H_values(shared_ptr<const SpTripletMat> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_H) {
        data_change_flags_.Update_H = true;
    }
    H_->setMatVal(rhs);
    H_qpOASES_->setVal(H_->MatVal());
    H_qpOASES_->createDiagInfo();
}


void qpOASESInterface::set_A_structure(shared_ptr<const SpTripletMat> rhs,
                                       Identity2Info I_info) {

#if DEBUG
#if GET_QP_INTERFACE_MEMBERS or COMPARE_QP_SOLVER
    A_triplet_->copy(rhs, false);
#endif
#endif
    A_->setStructure(rhs, I_info);
    A_qpOASES_ = std::make_shared<qpOASES::SparseMatrix>(A_->RowNum(),
                 A_->ColNum(),
                 A_->RowIndex(),
                 A_->ColIndex(),
                 A_->MatVal());
}


void qpOASESInterface::set_A_values(

    shared_ptr<const SQPhotstart::SpTripletMat> rhs, Identity2Info I_info) {
#if DEBUG
#if GET_QP_INTERFACE_MEMBERS or COMPARE_QP_SOLVER
    A_triplet_->copy(rhs, false);
#endif
#endif


    if (firstQPsolved_ && !data_change_flags_.Update_A) {
        data_change_flags_.Update_A = true;
    }
    A_->setMatVal(rhs, I_info);
    A_qpOASES_->setVal(A_->MatVal());
}


void qpOASESInterface::set_ub(shared_ptr<const Vector> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    ub_->copy_vector(rhs->values());
}


void qpOASESInterface::set_lb(shared_ptr<const Vector> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    lb_->copy_vector(rhs->values());

}


void qpOASESInterface::set_lbA(shared_ptr<const Vector> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    lbA_->copy_vector(rhs->values());
}


void qpOASESInterface::set_ubA(shared_ptr<const Vector> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_bounds)
        data_change_flags_.Update_bounds = true;
    ubA_->copy_vector(rhs->values());

}


void qpOASESInterface::set_g(shared_ptr<const Vector> rhs) {

    if (firstQPsolved_ && !data_change_flags_.Update_g)
        data_change_flags_.Update_g = true;
    g_->copy_vector(rhs->values());
}


#if DEBUG
#if GET_QPOASES_MEMBERS
const shared_ptr<Vector>& qpOASESInterface::getLb() const {
    return lb_;
}

const shared_ptr<Vector>& qpOASESInterface::getUb() const {
    return ub_;
}

const shared_ptr<Vector>& qpOASESInterface::getLbA() const {
    return lbA_;
}

const shared_ptr<Vector>& qpOASESInterface::getUbA() const {
    return ubA_;
}

const shared_ptr<Vector>& qpOASESInterface::getG() const {
    return g_;
}
#endif
#endif


void qpOASESInterface::reset_flags() {

    data_change_flags_.Update_A = false;
    data_change_flags_.Update_delta = false;
    data_change_flags_.Update_H = false;
    data_change_flags_.Update_penalty = false;
    data_change_flags_.Update_g = false;
    data_change_flags_.Update_bounds = false;
}


void qpOASESInterface::handler_error(QPType qptype, shared_ptr<Stats> stats) {

    if (qptype == LP) {
        if (true) {
            qpOASES::int_t nWSR = options_->lp_maxiter;//TODO modify it
            solver_->init(0, g_->values(), A_qpOASES_.get(),
                          lb_->values(), ub_->values(), lbA_->values(),
                          ubA_->values(), nWSR);
            obtain_status();
            old_QP_matrix_status_ = new_QP_matrix_status_ = UNDEFINED;
            stats->qp_iter_addValue((int) nWSR);

            if (!solver_->isSolved()) {
#if DEBUG
#if PRINT_OUT_QP_WITH_ERROR
                WriteQPDataToFile(jnlst_, Ipopt::J_ALL, Ipopt::J_DBG);
#endif
#endif

                THROW_EXCEPTION(LP_NOT_OPTIMAL,
                                "the QP problem didn't solved to optimality\n")
            }
        }
        else {
#if DEBUG
#if PRINT_OUT_QP_WITH_ERROR
            WriteQPDataToFile(jnlst_, Ipopt::J_ALL, Ipopt::J_DBG);
#endif
#endif
            THROW_EXCEPTION(LP_NOT_OPTIMAL,
                            "the QP problem didn't solved to optimality\n")
        }

    }
    else {
        if (true) {
            qpOASES::int_t nWSR = options_->lp_maxiter;//TODO modify it
            solver_->init(H_qpOASES_.get(), g_->values(), A_qpOASES_.get(),
                          lb_->values(), ub_->values(), lbA_->values(),
                          ubA_->values(), nWSR);
            obtain_status();
            old_QP_matrix_status_ = new_QP_matrix_status_ = UNDEFINED;
            stats->qp_iter_addValue((int) nWSR);
            if (!solver_->isSolved()) {
#if DEBUG
#if PRINT_OUT_QP_WITH_ERROR
                WriteQPDataToFile(jnlst_, Ipopt::J_ALL, Ipopt::J_DBG);
#endif
#endif
                THROW_EXCEPTION(QP_NOT_OPTIMAL,
                                "the QP problem didn't solved to optimality\n")

            }
        }
        else {
#if DEBUG
#if PRINT_OUT_QP_WITH_ERROR
            WriteQPDataToFile(jnlst_, Ipopt::J_ALL, Ipopt::J_DBG);
#endif
#endif
            THROW_EXCEPTION(QP_NOT_OPTIMAL,
                            "the QP problem didn't solved to optimality\n")
        }
    }
}


void qpOASESInterface::setQP_options() {

    qpOASES::Options qp_options;
    qp_options.setToReliable();
    //setup the printlevel of q
    switch (options_->qpPrintLevel) {
    case 0:
        qp_options.printLevel = qpOASES::PL_NONE;
        break;
    case 1:
        qp_options.printLevel = qpOASES::PL_TABULAR;
        break;
    case 2:
        qp_options.printLevel = qpOASES::PL_LOW;
        break;
    case 3:
        qp_options.printLevel = qpOASES::PL_MEDIUM;
        break;
    case 4:
        qp_options.printLevel = qpOASES::PL_HIGH;
        break;
    case -2:
        qp_options.printLevel = qpOASES::PL_DEBUG_ITER;
        break;
    }

    solver_->setOptions(qp_options);
}


void qpOASESInterface::WriteQPDataToFile(
    Ipopt::SmartPtr<Ipopt::Journalist> jnlst,
    Ipopt::EJournalLevel level,
    Ipopt::EJournalCategory category) {
//TODO: can be write as a cpp file ....
#if DEBUG
    lb_->write_to_file("lb",jnlst,level,category,QPOASES_QP);
    ub_->write_to_file("ub",jnlst,level,category,QPOASES_QP);
    lbA_->write_to_file("lbA",jnlst,level,category,QPOASES_QP);
    ubA_->write_to_file("ubA",jnlst,level,category,QPOASES_QP);
    g_->write_to_file("g",jnlst,level,category,QPOASES_QP);
    A_->write_to_file("a",jnlst,level,category,QPOASES_QP);
    H_->write_to_file("h",jnlst,level,category,QPOASES_QP);
#endif

}


void qpOASESInterface::get_Matrix_change_status() {

    if (old_QP_matrix_status_ == UNDEFINED) {
        old_QP_matrix_status_ = data_change_flags_.Update_A ||
                                data_change_flags_.Update_H ? VARIED
                                : FIXED;
    }
    else {
        if (new_QP_matrix_status_ != UNDEFINED)
            old_QP_matrix_status_ = new_QP_matrix_status_;
        new_QP_matrix_status_ = data_change_flags_.Update_A ||
                                data_change_flags_.Update_H ? VARIED
                                : FIXED;
    }


}

void qpOASESInterface::GetWorkingSet(ActiveType* W_constr, ActiveType* W_bounds) {
    int* tmp_W_c = new int[nConstr_QP_];
    int* tmp_W_b = new int[nVar_QP_];


    assert(nConstr_QP_==solver_->getNC());
    assert(nVar_QP_==solver_->getNV());
    solver_->getWorkingSetConstraints(tmp_W_c);
    solver_->getWorkingSetBounds(tmp_W_b);

    for (int i = 0; i < nVar_QP_; i++) {
        switch((int)tmp_W_b[i]) {
        case 1:
            W_constr[i] = ACTIVE_ABOVE;
            break;
        case -1:
            W_constr[i] = ACTIVE_BELOW;
            break;
        case 0:
            W_constr[i] = INACTIVE;
            break;
        default:
            THROW_EXCEPTION(INVALID_WORKING_SET,INVALID_WORKING_SET_MSG);
        }
    }
    for (int i = 0; i < nConstr_QP_; i++) {
        switch((int)tmp_W_c[i]) {
        case 1:
            W_bounds[i] = ACTIVE_ABOVE;
            break;
        case -1:
            W_bounds[i] = ACTIVE_BELOW;
            break;
        case 0:
            W_bounds[i] = INACTIVE;
            break;
        default:
            THROW_EXCEPTION(INVALID_WORKING_SET,INVALID_WORKING_SET_MSG);
        }
    }
    delete[] tmp_W_b;
    delete[] tmp_W_c;
}

}//SQPHOTSTART

