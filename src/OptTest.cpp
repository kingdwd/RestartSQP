/* Copyright (C) 2019
* All Rights Reserved.
*
* Authors: Xinyi Luo
* Date:2019-07-23
*/

#include <sqphot/OptTest.hpp>
#include <coin/IpTNLP.hpp>
#include <sqphot/Options.hpp>
#include <sqphot/Vector.hpp>

namespace SQPhotstart {

    NLP_OptTest::NLP_OptTest(shared_ptr<const Vector> x_k, shared_ptr<const Vector> x_u,
                             shared_ptr<const Vector> x_l, shared_ptr<const Vector> c_k,
                             shared_ptr<const Vector> c_u, shared_ptr<const Vector> c_l,
                             shared_ptr<SQPhotstart::Options> options,
                             const ConstraintType* cons_type,
                             const ConstraintType* bound_cons_type) :
            cons_type_(cons_type),
            bound_cons_type_(bound_cons_type),
            opt_tol_(options->opt_tol),
            opt_compl_tol_(options->opt_compl_tol),
            opt_prim_fea_tol_(options->opt_prim_fea_tol),
            opt_dual_fea_tol_(options->opt_dual_fea_tol),
            opt_second_tol_(options->opt_second_tol),
            active_set_tol_(options->active_set_tol),
            nVar_(x_k->Dim()),
            nCon_(c_k->Dim()),
            x_k_(x_k),
            x_l_(x_l),
            x_u_(x_u),
            c_u_(c_u),
            c_k_(c_k),
            c_l_(c_l),
            Active_Set_constraints_(NULL),
            Active_Set_bounds_(NULL) {
    }


    NLP_OptTest::~NLP_OptTest() {
        delete[] Active_Set_constraints_;
        Active_Set_constraints_ = NULL;
        delete[] Active_Set_bounds_;
        Active_Set_bounds_ = NULL;

    }

    bool NLP_OptTest::Check_KKTConditions(bool isPointChanged, bool
    isConstraintChanged) {
        IdentifyActiveSet();
        Check_Feasibility();
        Check_Complementarity();
        Check_Dual_Feasibility();
        Check_Stationarity();
        return true;
    }

    bool NLP_OptTest::Check_Complementarity() {
        return true;
    }


    bool NLP_OptTest::Check_Dual_Feasibility() {
        return true;
    }


    bool NLP_OptTest::Check_Dual_Feasibility(const double* multiplier,
                                             ConstraintType nlp_cons_type) {

        return true;
    }

    bool NLP_OptTest::Check_SecondOrder() {
        if (complementarity_ && dual_feasibility_ && stationarity_ &&
            primal_feasibility_) {
            //Check the 2nd order condition
        } else {
            //print error message
        }
        return true;
    }


    bool NLP_OptTest::Check_Stationarity() {
        return true;
    }

    bool NLP_OptTest::Check_Feasibility(double infea_measure) {
        if (primal_feasibility_ == false && infea_measure < opt_prim_fea_tol_)
            primal_feasibility_ = true;
    }

    void NLP_OptTest::setActiveSetConstraints(int* activeSetConstraints) {
        Active_Set_constraints_ = activeSetConstraints;
    }

    void NLP_OptTest::setActiveSetBounds(int* activeSetBounds) {
        Active_Set_bounds_ = activeSetBounds;
    }

    int* NLP_OptTest::getActiveSetConstraints() const {
        return Active_Set_constraints_;
    }

    int* NLP_OptTest::getActiveSetBounds() const {
        return Active_Set_bounds_;
    }

    bool NLP_OptTest::IdentifyActiveSet() {
        if (Active_Set_constraints_ == NULL)
            Active_Set_constraints_ = new int[nCon_]();

        if (Active_Set_bounds_ == NULL)
            Active_Set_bounds_ = new int[nVar_]();


        for (int i = 0; i < nCon_; i++) {

            if (cons_type_[i] == BOUNDED_ABOVE) {
                if (std::abs(c_u_->values()[i] - c_k_->values()[i]) <
                    active_set_tol_)
                    // consider adding another tolerance for identifying active set...
                    Active_Set_constraints_[i] = 1;
            } else if (cons_type_[i] == BOUNDED_BELOW) {
                if (std::abs(c_k_->values()[i] - c_l_->values()[i]) <
                    active_set_tol_) {
                    // consider adding another tolerance for identifying active set...
                    Active_Set_constraints_[i] = -1;
                }
            } else if (cons_type_[i] == EQUAL) {
                if ((std::abs(c_u_->values()[i] - c_k_->values()[i]) <
                     active_set_tol_) &&
                    (std::abs(c_k_->values()[i] - c_l_->values()[i]) <
                     active_set_tol_))
                    Active_Set_constraints_[i] = 99;
                else {
                    //TODO: Print out warning message
                }
            }
        }


        for (int i = 0; i < nVar_; i++) {
            if (bound_cons_type_[i] == BOUNDED_ABOVE) {
                if (std::abs(x_u_->values()[i] - x_k_->values()[i]) <
                    active_set_tol_)
                    // consider adding another tolerance for identifying active set...
                    Active_Set_bounds_[i] = 1;
            } else if (cons_type_[i] == BOUNDED_BELOW) {
                if (std::abs(x_k_->values()[i] - x_l_->values()[i]) <
                    active_set_tol_)
                    // consider adding another tolerance for identifying active set...
                    Active_Set_bounds_[i] = -1;
            } else if (cons_type_[i] == EQUAL) {
                if ((std::abs(x_u_->values()[i] - x_k_->values()[i]) <
                     active_set_tol_) &&
                    (std::abs(x_k_->values()[i] - x_l_->values()[i]) <
                     active_set_tol_))
                    Active_Set_bounds_[i] = 99;
                else {
                    //TODO: Print out warning message
                }
            }
        }


        return true;
    }


    bool
    QP_OptTest::Check_KKTConditions(bool isPointChanged,
                                    bool isConstraintChanged) {
        Check_Feasibility();
        Check_Complementarity();
        Check_Dual_Feasibility();
        Check_Stationarity();
        return true;
    }

    bool QP_OptTest::Check_Complementarity() {
        return true;
    }

    bool QP_OptTest::Check_Feasibility() {

        return true;
    }


    bool QP_OptTest::Check_Dual_Feasibility() {
        return true;
    }


    bool QP_OptTest::Check_SecondOrder() {
        if (complementarity_ && dual_feasibility_ && stationarity_ &&
            primal_feasibility_) {
            //Check the 2nd order condition
        } else {
            //print error message
        }
        return true;

    }


    bool QP_OptTest::Check_Stationarity() {
        return true;
    }


}