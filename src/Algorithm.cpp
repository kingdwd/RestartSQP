/* Copyright (C) 2019
* All Rights Reserved.
*
* Authors: Xinyi Luo
* Date:2019-06
*/
#include <sqphot/Algorithm.hpp>

namespace SQPhotstart {
using namespace std;
DECLARE_STD_EXCEPTION(NEW_POINTS_WITH_INCREASE_OBJ_ACCEPTED);

DECLARE_STD_EXCEPTION(SMALL_TRUST_REGION);
/**
 * Default Constructor
 */
Algorithm::Algorithm() :
    W_constr_(NULL),
    W_bounds_(NULL),
    cons_type_(NULL),
    bound_cons_type_(NULL),
    obj_value_trial_(0),
    pred_reduction_(0),
    qp_obj_(0),
    actual_reduction_(0),
    infea_measure_(0.0),
    infea_measure_model_(0.0) {
    jnlst_ = new Ipopt::Journalist();
    roptions2_ = new Ipopt::OptionsList();
}


/**
 * Default Destructor
 */
Algorithm::~Algorithm() {

    delete[] cons_type_;
    cons_type_ = NULL;
    delete[] bound_cons_type_;
    bound_cons_type_ = NULL;
    delete[] W_bounds_;
    W_bounds_ = NULL;
    delete[] W_constr_;
    W_constr_ = NULL;

}


/**
 * @brief This is the main function to optimize the NLP given as the input
 *
 * @param nlp: the nlp reader that read data of the function to be minimized;
 */
void Algorithm::Optimize() {
    while (stats_->iter < options_->iter_max && exitflag_ == UNKNOWN) {
        iter_time_ = clock();
        setupQP();
        //for debugging
        //@{
        //    hessian_->print_full("hessian");
        //    jacobian_->print_full("jacobian");
        //@}
        try {
            myQP_->solveQP(stats_,
                           options_);//solve the QP subproblem and update the stats_
        }
        catch (QP_NOT_OPTIMAL) {
            myQP_->WriteQPData(problem_name_+"qpdata.log");
            exitflag_ = myQP_->get_status();
            break;
        }


        //get the search direction from the solution of the QPsubproblem
        get_search_direction();
        qp_obj_ = get_obj_QP();

        //Update the penalty parameter if necessary

        update_penalty_parameter();

        //calculate the infinity norm of the search direction
        norm_p_k_ = p_k_->getInfNorm();

        get_trial_point_info();

        ratio_test();

        // Calculate the second-order-correction steps
        second_order_correction();

        // Update the radius and the QP bounds if the radius has been changed
        stats_->iter_addone();
        /* output some information to the console*/

        //check if the current iterates is optimal and decide to
        //exit the loop or not
        if (options_->printLevel >= 2) {
            if (stats_->iter % 10 == 0) {
                jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_HEADER);
                jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
            }
            jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_OUTPUT);
        }
        else {
            jnlst_->DeleteAllJournals();
            Ipopt::SmartPtr<Ipopt::Journal> logout_jrnl = jnlst_->GetJournal("file_output");
            if(IsNull(logout_jrnl)) {
                jnlst_->AddFileJournal("file_output", problem_name_+"_output.log",
                                       Ipopt::J_ITERSUMMARY);

            }
            if (IsValid(logout_jrnl)) {
                logout_jrnl->SetPrintLevel(Ipopt::J_STATISTICS, Ipopt::J_NONE);
            }
            if (stats_->iter % 10 == 0) {
                jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_HEADER);
                jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
            }
            jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_OUTPUT);
        }


        check_optimality();
        if (exitflag_ != UNKNOWN) {
            break;
        }

        try {
            update_radius();
        }
        catch (SMALL_TRUST_REGION) {
            check_optimality();
            break;
        }

        iter_time_ = clock()-iter_time_;
        stats_->total_time += iter_time_;
        if(((float) stats_->total_time/CLOCKS_PER_SEC)>options_->time_max) {
            exitflag_ = EXCEED_TIME_LIMITS;
            break;
        }

    }

    //check if the current iterates get_status before exiting
    if (stats_->iter == options_->iter_max)
        exitflag_ = EXCEED_MAX_ITER;

    //    if (exitflag_ != OPTIMAL && exitflag_ != INVALID_NLP) {
    //        check_optimality();
    //    }

    // print the final summary message to the console
    print_final_stats();
    jnlst_->FlushBuffer();
}


/**
 * @brief This is the function that checks if the current point is optimal, and
 * decides if to exit the loop or not
 * *@return if it decides the function is optimal, the class member _exitflag =
 * OPTIMAL
 * if it decides that there is an error during the function run or the
 *  function cannot be solved, it will assign _exitflag the	corresponding
 *  code according to the error type.
 */
void Algorithm::check_optimality() {
    //FIXME: not sure if it is better to use the new multiplier or the old one

    double primal_violation = 0;
    double dual_violation =0;
    double compl_violation = 0;
    double statioanrity_violation = 0;

    int i;
    get_multipliers();

    if(multiplier_vars_== nullptr)
        /**-------------------------------------------------------**/
        /**               Check the KKT conditions                **/
        /**-------------------------------------------------------**/

        /**-------------------------------------------------------**/
        /**                    Identify Active Set                **/
        /**-------------------------------------------------------**/
        for (i = 0; i < nCon_; i++) {
            if (cons_type_[i] == BOUNDED_ABOVE) {
                if (abs(c_u_->values(i) - c_k_->values(i)) <
                        options_->active_set_tol)
                    W_constr_[i] = ACTIVE_ABOVE;
            } else if (cons_type_[i] == BOUNDED_BELOW) {
                if (abs(c_k_->values(i) - c_l_->values(i)) <
                        options_->active_set_tol) {
                    W_constr_[i] = ACTIVE_BELOW;
                }
            } else if (cons_type_[i] == EQUAL) {
                if ((abs(c_u_->values(i) - c_k_->values(i)) <
                        options_->active_set_tol) &&
                        (abs(c_k_->values(i) - c_l_->values(i)) <
                         options_->active_set_tol))
                    W_constr_[i] = ACTIVE_BOTH_SIDE;
            } else {
                W_constr_[i] = INACTIVE;
            }
        }


    for (i = 0; i < nVar_; i++) {
        if (bound_cons_type_[i] == BOUNDED_ABOVE) {
            if (abs(x_u_->values(i) - x_k_->values(i)) <
                    options_->active_set_tol)
                W_bounds_[i] = ACTIVE_ABOVE;
        } else if (bound_cons_type_[i] == BOUNDED_BELOW) {
            if (abs(x_k_->values(i) - x_l_->values(i)) <
                    options_->active_set_tol)
                W_bounds_[i] = ACTIVE_BELOW;
        } else if (bound_cons_type_[i] == EQUAL) {
            if ((abs(x_u_->values(i) - x_k_->values(i)) <
                    options_->active_set_tol) &&
                    (abs(x_k_->values(i) - x_l_->values(i)) <
                     options_->active_set_tol))
                W_bounds_[i] = ACTIVE_BOTH_SIDE;
        } else {
            W_bounds_[i] = INACTIVE;
        }
    }
    /**-------------------------------------------------------**/
    /**                    Primal Feasibility                 **/
    /**-------------------------------------------------------**/

    primal_violation += infea_measure_;

    /**-------------------------------------------------------**/
    /**                    Dual Feasibility                   **/
    /**-------------------------------------------------------**/

    //    multiplier_vars_->print("multiplier_vars");
    //    multiplier_cons_->print("multiplier_cons");
    //    x_k_->print("x_k");
    //    x_l_->print("x_l");
    //    x_u_->print("x_u");
    //
    //    c_k_->print("c_k");
    //    c_l_->print("c_l");
    //    c_u_->print("c_u");
    i = 0;
    opt_status_.dual_feasibility = true;
    while (i < nVar_) {
        if (bound_cons_type_[i] == BOUNDED_ABOVE) {
            dual_violation += max(multiplier_vars_->values(i), 0.0);
        } else if (bound_cons_type_[i] == BOUNDED_BELOW) {
            dual_violation += -min(multiplier_vars_->values(i), 0.0);
        }
        i++;

    }

    i = 0;
    while (i < nCon_) {
        if (cons_type_[i] == BOUNDED_ABOVE) {
            dual_violation += max(multiplier_cons_->values(i),0.0);
        } else if (cons_type_[i] == BOUNDED_BELOW) {
            dual_violation += -min(multiplier_cons_->values(i),0.0);
        }
        i++;
    }

    /**-------------------------------------------------------**/
    /**                    Complemtarity                      **/
    /**-------------------------------------------------------**/
    //@{

    i = 0;
    while (i < nCon_ ) {
        if (cons_type_[i] == BOUNDED_ABOVE) {
            compl_violation+=abs(multiplier_cons_->values(i) *
                                 (c_u_->values(i) - c_k_->values(i)));
        }
        else if (cons_type_[i] == BOUNDED_BELOW) {
            compl_violation+=abs(multiplier_cons_->values(i) *
                                 (c_k_->values(i) - c_l_->values(i)));
        }
        else if (cons_type_[i] == UNBOUNDED) {
            compl_violation+= abs(multiplier_cons_->values(i));
        }
        i++;
    }

    i = 0;
    while (i < nVar_ ) {
        if (bound_cons_type_[i] == BOUNDED_ABOVE) {
            compl_violation+=abs(multiplier_vars_->values(i) *
                                 (x_u_->values(i) - x_k_->values(i)));
        }
        else if (bound_cons_type_[i] == BOUNDED_BELOW) {
            compl_violation+=abs(multiplier_vars_->values(i) *
                                 (x_k_->values(i) - x_l_->values(i)));
        }
        else if (bound_cons_type_[i] == UNBOUNDED) {
            compl_violation+= abs(multiplier_vars_->values(i));
        }
        i++;
    }
    //@{
    //    c_l_->print("c_l");
    //    c_u_->print("c_u");
    //    std::cout<<compl_violation<<std::endl;
    //    multiplier_vars_->print("multiplier_vars");
    //    multiplier_cons_->print("multiplier_constr");
    //@}

    //@}
    /**-------------------------------------------------------**/
    /**                    Stationarity                       **/
    /**-------------------------------------------------------**/
    //@{
    shared_ptr<Vector> difference = make_shared<Vector>(nVar_);
    // the difference of g-J^T y -\lambda
    //
//    grad_f_->print("grad_f_");
//
//    jacobian_->print_full("jacobian");
//
//    multiplier_cons_->print("multiplier_cons_");
//    multiplier_vars_->print("multiplier_vars_");
    jacobian_->transposed_times(multiplier_cons_, difference);
    difference->add_vector(multiplier_vars_->values());
    difference->subtract_vector(grad_f_->values());

    statioanrity_violation = difference->getOneNorm();
    //@}


    /**-------------------------------------------------------**/
    /**             Decide if x_k is optimal                  **/
    /**-------------------------------------------------------**/


    opt_status_.dual_violation = dual_violation;
    opt_status_.primal_violation = primal_violation;
    opt_status_.compl_violation = compl_violation;
    opt_status_.stationarity_violation = statioanrity_violation;
    opt_status_.KKT_error =
        dual_violation+primal_violation+compl_violation+statioanrity_violation;
//    printf("primal_violation = %23.16e\n",primal_violation);
//    printf("dual_violation = %23.16e\n",dual_violation);
//    printf("compl_violation = %23.16e\n",compl_violation);
//    printf("statioanrity_violation = %23.16e\n",statioanrity_violation);
//    printf("KKT error = %23.16e\n", opt_status_.KKT_error);

    opt_status_.primal_feasibility = primal_violation < options_->opt_prim_fea_tol;
    opt_status_.dual_feasibility = dual_violation < options_->opt_dual_fea_tol;
    opt_status_.complementarity = compl_violation < options_->opt_compl_tol;
    opt_status_.stationarity= statioanrity_violation <
                              options_->opt_stat_tol;

    if (opt_status_.primal_feasibility && opt_status_.dual_feasibility &&
            opt_status_.complementarity && opt_status_.stationarity) {
        opt_status_.first_order_opt = true;
        exitflag_ = OPTIMAL;
    } else {
        //if it is not optimal
#if DEBUG
#if CHECK_TERMINATION

        EJournalLevel debug_print_level = options_->debug_print_level;
        SmartPtr<Journal> debug_jrnl = jnlst_->GetJournal("Debug");
        if (IsNull(debug_jrnl)) {
            debug_jrnl = jnlst_->AddFileJournal("Debug", "debug.out", Ipopt::J_ITERSUMMARY);
        }
        debug_jrnl->SetAllPrintLevels(debug_print_level);
        debug_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_ALL);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG,
                       DOUBLE_DIVIDER);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "           Iteration  %i\n", stats_->iter);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, DOUBLE_DIVIDER);
        grad_f_->print("grad_f", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, SINGLE_DIVIDER);
        c_u_->print("c_u", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        c_l_->print("c_l", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        c_k_->print("c_k", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        multiplier_cons_->print("multiplier_cons", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, SINGLE_DIVIDER);
        x_u_->print("x_u", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        x_l_->print("x_l", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        x_k_->print("x_k", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        multiplier_vars_->print("multiplier_vars", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, SINGLE_DIVIDER);
        jacobian_->print_full("jacobian", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        hessian_->print_full("hessian", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        difference->print("stationarity gap", jnlst_, Ipopt::J_MOREDETAILED, Ipopt::J_DBG);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, SINGLE_DIVIDER);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "Feasibility      ");
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "%i\n",
                       opt_status_.primal_feasibility);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "Dual Feasibility ");
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "%i\n", opt_status_.dual_feasibility);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "Stationarity     ");
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "%i\n", opt_status_.stationarity);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "Complementarity  ");
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, "%i\n", opt_status_.complementarity);
        jnlst_->Printf(Ipopt::J_ALL, Ipopt::J_DBG, SINGLE_DIVIDER);

#endif
#endif

    }
}


void Algorithm::get_trial_point_info() {

    x_trial_->copy_vector(x_k_->values());//make a deep copy
    x_trial_->add_vector(p_k_->values());

    // Calculate f_trial, c_trial and infea_measure_trial for the trial points
    // x_trial
    nlp_->Eval_f(x_trial_, obj_value_trial_);
    nlp_->Eval_constraints(x_trial_, c_trial_);

#if NEW_FORMULATION
    infea_measure_trial_ = cal_infea(c_trial_,c_l_,c_u_, x_trial_,x_l_, x_u_);
#else
    infea_measure_trial_=cal_infea(c_trial_, c_l_, c_u_); //calculate the infeasibility measure for x_k
#endif
}


/**
 *  @brief This function initializes the objects required by the SQP Algorithm,
 *  copies some parameters required by the algorithm, obtains the function
 *  information for the first QP.
 *
 */
void Algorithm::initialization(Ipopt::SmartPtr<Ipopt::TNLP> nlp,
                               const string& name) {
    std::size_t found = name.find_last_of("/\\");

    problem_name_  = name.substr(found+1);

    allocate_memory(nlp);

    delta_ = options_->delta;
    rho_ = options_->rho;
    norm_p_k_ = 0.0;

    /*-----------------------------------------------------*/
    /*         Get the nlp information                     */
    /*-----------------------------------------------------*/
    nlp_->Get_bounds_info(x_l_, x_u_, c_l_, c_u_);
    nlp_->Get_starting_point(x_k_, multiplier_cons_);

    //shift starting point to satisfy the bound constraint
#if not NEW_FORMULATION
    nlp_->shift_starting_point(x_k_, x_l_, x_u_);
#endif
    nlp_->Eval_f(x_k_, obj_value_);
    nlp_->Eval_gradient(x_k_, grad_f_);
    nlp_->Eval_constraints(x_k_, c_k_);
    nlp_->Get_Structure_Hessian(x_k_, multiplier_cons_, hessian_);
    nlp_->Eval_Hessian(x_k_, multiplier_cons_, hessian_);
    nlp_->Get_Strucutre_Jacobian(x_k_, jacobian_);
    nlp_->Eval_Jacobian(x_k_, jacobian_);
    classify_constraints_types();

#if NEW_FORMULATION
    infea_measure_=cal_infea(c_k_, c_l_, c_u_, x_k_, x_l_, x_u_); //calculate the infeasibility measure for x_k
#else
    infea_measure_=cal_infea(c_k_, c_l_, c_u_); //calculate the infeasibility measure for x_k
#endif

    /*-----------------------------------------------------*/
    /*             JOURNAL INIT & OUTPUT                   */
    /*-----------------------------------------------------*/


    if(options_->printLevel>1) {
        Ipopt::SmartPtr<Ipopt::Journal> stdout_jrnl =
            jnlst_->AddFileJournal("console", "stdout", Ipopt::J_ITERSUMMARY);
        if (IsValid(stdout_jrnl)) {
            // Set printlevel for stdout
            stdout_jrnl->SetAllPrintLevels(options_->print_level);
            stdout_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_NONE);
        }
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_HEADER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_OUTPUT);

    }
    else {
        string  output_file_name = problem_name_+"_output.log";
        Ipopt::SmartPtr<Ipopt::Journal> logout_jrnl =
            jnlst_->AddFileJournal("file_output", output_file_name.c_str(),
                                   Ipopt::J_ITERSUMMARY);
        if (IsValid(logout_jrnl)) {
            logout_jrnl->SetPrintLevel(Ipopt::J_STATISTICS, Ipopt::J_NONE);
        }

        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_HEADER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, STANDARD_OUTPUT);
    }

    //#if DEBUG
    //    Ipopt::SmartPtr<Ipopt::Journal> debug_jrnl =
    //        jnlst_->AddFileJournal("Debug", problem_name_+"debug.log",
    //                               Ipopt::J_MOREDETAILED);
    //    debug_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_MOREDETAILED);
    //#endif



}


/**
 * @brief alloocate memory for class members.
 * This function initializes all the shared pointer which will be used in the
 * Algorithm::Optimize, and it copies all parameters that might be changed during
 * the run of the function Algorithm::Optimize.
 *
 * @param nlp: the nlp reader that read data of the function to be minimized;
 */
void Algorithm::allocate_memory(Ipopt::SmartPtr<Ipopt::TNLP> nlp) {

    clock_t t = clock();
    nlp_ = make_shared<SQPTNLP>(nlp);
    nVar_ = nlp_->nlp_info_.nVar;
    nCon_ = nlp_->nlp_info_.nCon;
    cons_type_ = new ConstraintType[nCon_];
    bound_cons_type_ = new ConstraintType[nVar_];
    W_bounds_ = new ActiveType[nVar_];
    W_constr_ = new ActiveType[nCon_];

    x_k_ = make_shared<Vector>(nVar_);
    x_trial_ = make_shared<Vector>(nVar_);
    p_k_ = make_shared<Vector>(nVar_);
    multiplier_cons_ = make_shared<Vector>(nCon_);
    multiplier_vars_ = make_shared<Vector>(nVar_);
    c_k_ = make_shared<Vector>(nCon_);
    c_trial_ = make_shared<Vector>(nCon_);
    x_l_ = make_shared<Vector>(nVar_);
    x_u_ = make_shared<Vector>(nVar_);
    c_l_ = make_shared<Vector>(nCon_);
    c_u_ = make_shared<Vector>(nCon_);
    grad_f_ = make_shared<Vector>(nVar_);

    jacobian_ = make_shared<SpTripletMat>(nlp_->nlp_info_.nnz_jac_g, nCon_, nVar_,
                                          false);
    hessian_ = make_shared<SpTripletMat>(nlp_->nlp_info_.nnz_h_lag, nVar_, nVar_,
                                         true);
    //TODO: use roptions instead of this one
    options_ = make_shared<Options>();
    stats_ = make_shared<Stats>();

    myQP_ = make_shared<QPhandler>(nlp_->nlp_info_,QP, jnlst_, options_);
    myLP_ = make_shared<QPhandler>(nlp_->nlp_info_,LP, jnlst_, options_);

    stats_->total_time = clock() - t;


}



/**
* @brief This function calculates the infeasibility for given x_k and c_k with respect
* to their corresponding bounds
* @return infea_measure = ||-max(c_k-c_u),0||_1 +||-min(c_k-c_l),0||_1+
                  ||-max(x_k-x_u),0||_1 +||-min(x_k-x_l),0||_1
*/
double Algorithm::cal_infea(shared_ptr<const Vector> c_k,
                            shared_ptr<const Vector> c_l,
                            shared_ptr<const Vector> c_u,
                            shared_ptr<const Vector> x_k,
                            shared_ptr<const Vector> x_l,
                            shared_ptr<const Vector> x_u) {
    double infea_measure = 0.0;
    for (int i = 0; i < c_k_->Dim(); i++) {
        if (c_k->values(i) < c_l->values(i))
            infea_measure += (c_l->values(i) - c_k->values(i));
        else if (c_k->values(i) > c_u->values(i))
            infea_measure += (c_k->values(i) - c_u->values(i));
    }

    if(x_k!=nullptr) {
        for (int i = 0; i < x_k_->Dim(); i++) {
            if (x_k->values(i) < x_l->values(i))
                infea_measure += (x_l->values(i) - x_k->values(i));
            else if (x_k->values(i) > x_u->values(i))
                infea_measure += (x_k->values(i) - x_u->values(i));
        }
    }

    return infea_measure;

}


/**
 * @brief This function extracts the search direction for NLP from the QP subproblem
 * solved, and copies it to the class member _p_k
 */
void Algorithm::get_search_direction() {
    p_k_->copy_vector(myQP_->get_optimal_solution());
}


/**
 * @brief This function gets the Lagragian multipliers for constraints and for for the
 * bounds from QPsolver
 */

void Algorithm::get_multipliers() {
    if (options_->QPsolverChoice == QORE||options_->QPsolverChoice==QPOASES) {
        multiplier_cons_->copy_vector(myQP_->get_multipliers_constr());
        multiplier_vars_->copy_vector(myQP_->get_multipliers_bounds());
    } else if(options_->QPsolverChoice ==GUROBI||options_->QPsolverChoice==CPLEX) {
        multiplier_cons_->copy_vector(myQP_->get_multipliers_constr());
        shared_ptr<Vector> tmp_vec_nVar = make_shared<Vector>(nVar_);
        jacobian_->transposed_times(multiplier_cons_,tmp_vec_nVar);
        hessian_->times(p_k_,multiplier_vars_);
        multiplier_vars_->add_vector(grad_f_->values());
        multiplier_vars_->subtract_vector(tmp_vec_nVar->values());
    }
}


DECLARE_STD_EXCEPTION(QP_UNCHANGED);

/**
 * @brief This function will set up the data for the QP subproblem
 *
 * It will initialize all the data at once at the beginning of the Algorithm. After
 * that, the data in the QP problem will be updated according to the class
 * member QPinfoFlag_
 */


void Algorithm::setupQP() {
    if (stats_->iter == 0) {
        myQP_->set_A(jacobian_);
        myQP_->set_H(hessian_);
        myQP_->set_bounds(delta_, x_l_, x_u_, x_k_, c_l_, c_u_, c_k_);
        myQP_->set_g(grad_f_, rho_);
    } else {
        if ((!QPinfoFlag_.Update_g) && (!QPinfoFlag_.Update_H) &&
                (!QPinfoFlag_.Update_A)
                && (!QPinfoFlag_.Update_bounds) && (!QPinfoFlag_.Update_delta) &&
                (!QPinfoFlag_.Update_penalty)) {

            auto stdout_jrnl = jnlst_->GetJournal("console");

            if (IsNull(stdout_jrnl)) {
                Ipopt::SmartPtr<Ipopt::Journal> stdout_jrnl =
                    jnlst_->AddFileJournal("console", "stdout", Ipopt::J_ITERSUMMARY);
            }
            if (IsValid(stdout_jrnl)) {
                // Set printlevel for stdout
                stdout_jrnl->SetAllPrintLevels(options_->print_level);
                stdout_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_NONE);
            }

            jnlst_->Printf(Ipopt::J_WARNING, Ipopt::J_MAIN, "QP is not changed!");
            THROW_EXCEPTION(QP_UNCHANGED, "QP is not changed");
        }
        if (QPinfoFlag_.Update_A) {
            myQP_->update_A(jacobian_);
            QPinfoFlag_.Update_A = false;
        }
        if (QPinfoFlag_.Update_H) {
            myQP_->update_H(hessian_);
            QPinfoFlag_.Update_H = false;
        }
        if (QPinfoFlag_.Update_bounds) {
            myQP_->update_bounds(delta_, x_l_, x_u_, x_k_, c_l_, c_u_, c_k_);
            QPinfoFlag_.Update_bounds = false;
            QPinfoFlag_.Update_delta = false;
        } else if (QPinfoFlag_.Update_delta) {
            myQP_->update_delta(delta_, x_l_, x_u_, x_k_);
            QPinfoFlag_.Update_delta = false;
        }
        if (QPinfoFlag_.Update_penalty) {
            myQP_->update_penalty(rho_);
            QPinfoFlag_.Update_penalty = false;
        }
        if (QPinfoFlag_.Update_g) {
            myQP_->update_grad(grad_f_);
            QPinfoFlag_.Update_g = false;
        }
    }
}


void Algorithm::setupLP() {
    myLP_->set_bounds(delta_, x_l_, x_u_, x_k_, c_l_, c_u_, c_k_);
    myLP_->set_g(rho_);
    myLP_->set_A(jacobian_);
}


/**
 *
 * @brief This function performs the ratio test to determine if we should accept
 * the trial point
 *
 * The ratio is calculated by
 * (P_1(x_k;\rho)-P_1( x_trial;\rho))/(q_k(0;\rho)-q_k(p_k;rho), where
 * P_1(x,rho) = f(x) + rho* infeasibility_measure is the l_1 merit function and
 * q_k(p; rho) = f_k+ g_k^Tp +1/2 p^T H_k p+rho* infeasibility_measure_model is the
 * quadratic model at x_k.
 * The trial point  will be accepted if the ratio >= eta_s.
 * If it is accepted, the function will also updates the gradient, Jacobian
 * information by reading from nlp_ object. The corresponding flags of class member
 * QPinfoFlag_ will set to be true.
 */
void Algorithm::ratio_test() {


    double P1_x = obj_value_ + rho_ * infea_measure_;
    double P1_x_trial = obj_value_trial_ + rho_ * infea_measure_trial_;

    actual_reduction_ = P1_x - P1_x_trial;
    pred_reduction_ = rho_ * infea_measure_ - get_obj_QP();

#if DEBUG
#if CHECK_TR_ALG
    Ipopt::SmartPtr<Ipopt::Journal> debug_jrnl = jnlst_->GetJournal("Debug");
    if (IsNull(debug_jrnl)) {
        debug_jrnl = jnlst_->AddFileJournal("Debug", "debug.out", Ipopt::J_ITERSUMMARY);
    }
    debug_jrnl->SetAllPrintLevels(options_->debug_print_level);
    debug_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_ALL);

    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"\n");
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,SINGLE_DIVIDER);
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"       The actual reduction is %23.16e\n",
                   actual_reduction_ );
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"       The pred reduction is   %23.16e\n",
                   pred_reduction_ );
    double ratio = actual_reduction_ / pred_reduction_;
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"       The calculated ratio is %23.16e\n",ratio);
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG, "       The correct decision is ");
    if (ratio >= options_->eta_s)
        jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG, "to ACCEPT the trial point\n");
    else
        jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG, "to REJECT the trial point and change the "
                       "trust-region radius\n");
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"       The TRUE decision is ");

    if (actual_reduction_ >= (options_->eta_s * pred_reduction_)) {
        jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG, "to ACCEPT the trial point\n");
    } else
        jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG, "to REJECT the trial point and change the "
                       "trust-region radius\n");
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,SINGLE_DIVIDER);
    jnlst_->Printf(Ipopt::J_ALL,Ipopt::J_DBG,"\n");

#endif
#endif

#if 1
    if (actual_reduction_ >= (options_->eta_s * pred_reduction_)
            && actual_reduction_ >= -options_->tol)
#else
    if (pred_reduction_ < -1.0e-8) {
        //    myQP_->WriteQPData(problem_name_+"qpdata.log");
        exitflag_ = PRED_REDUCTION_NEGATIVE;
        return;
    }
    if (actual_reduction_ >= (options_->eta_s * pred_reduction_))
#endif
    {
        //succesfully update
        //copy information already calculated from the trial point
        infea_measure_ = infea_measure_trial_;

        obj_value_ = obj_value_trial_;
        x_k_->copy_vector(x_trial_->values());
        c_k_->copy_vector(c_trial_->values());
        //update function information by reading from nlp_ object
        get_multipliers();
        nlp_->Eval_gradient(x_k_, grad_f_);
        nlp_->Eval_Jacobian(x_k_, jacobian_);
        nlp_->Eval_Hessian(x_k_, multiplier_cons_, hessian_);

        QPinfoFlag_.Update_A = true;
        QPinfoFlag_.Update_H = true;
        QPinfoFlag_.Update_bounds = true;
        QPinfoFlag_.Update_g = true;

        isaccept_ = true;    //no need to calculate the SOC direction
    } else {
        isaccept_ = false;
    }
}


/**
 * @brief Update the trust region radius.
 *
 * This function update the trust-region radius when the ratio calculated by the
 * ratio test is smaller than eta_c or bigger than eta_e and the search_direction
 * hits the trust-region bounds.
 * If ratio<eta_c, the trust region radius will decrease by the parameter
 * gamma_c, to be gamma_c* delta_
 * If ratio_test> eta_e and delta_= norm_p_k_ the trust-region radius will be
 * increased by the parameter gamma_c.
 *
 * If trust region radius has changed, the corresponding flags will be set to be
 * true;
 */


void Algorithm::update_radius() {
    if (actual_reduction_ < options_->eta_c * pred_reduction_) {
        delta_ = options_->gamma_c * delta_;
        QPinfoFlag_.Update_delta = true;
        //decrease the trust region radius. gamma_c is the parameter in options_ object
    } else {
        //printf("delta_ = %23.16e, ||p_k|| = %23.16e\n",delta_,p_k_->getInfNorm());
        if (actual_reduction_ > options_->
                eta_e * pred_reduction_
                && (options_->tol > fabs(delta_ - p_k_->getInfNorm()))) {
            delta_ = min(options_->gamma_e * delta_, options_->delta_max);
            QPinfoFlag_.Update_delta = true;
        }
    }


    //if the trust-region becomes too small, throw the error message

    if (delta_ < options_->delta_min) {

        Ipopt::SmartPtr<Ipopt::Journal> stdout_jrnl = jnlst_->GetJournal("console");
        if (IsValid(stdout_jrnl)) {
            // Set printlevel for stdout
            stdout_jrnl->SetAllPrintLevels(options_->print_level);
            stdout_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_NONE);
        }
        exitflag_ = TRUST_REGION_TOO_SMALL;
        THROW_EXCEPTION(SMALL_TRUST_REGION, SMALL_TRUST_REGION_MSG);
    }
}


/**
 *
 * @brief This function checks how each constraint specified by the nlp readers are
 * bounded.
 * If there is only upper bounds for a constraint, c_i(x)<=c^i_u, then
 * cons_type_[i]= BOUNDED_ABOVE
 * If there is only lower bounds for a constraint, c_i(x)>=c^i_l, then
 * cons_type_[i]= BOUNDED_BELOW
 * If there are both upper bounds and lower bounds, c^i_l<=c_i(x)<=c^i_u, and
 * c^i_l<c^i_u then cons_type_[i]= BOUNDED,
 * If there is no constraints on all
 * of c_i(x), then cons_type_[i]= UNBOUNDED;
 *
 * The same rules are also applied to the bound-constraints.
 */


void Algorithm::classify_constraints_types() {

    for (int i = 0; i < nCon_; i++) {
        cons_type_[i] = classify_single_constraint(c_l_->values(i),
                        c_u_->values(i));
    }
    for (int i = 0; i < nVar_; i++) {
        bound_cons_type_[i] = classify_single_constraint(x_l_->values(i),
                              x_u_->values(i));
    }
}


/**
 * @brief update the penalty parameter for the algorithm.
 *
 */
void Algorithm::update_penalty_parameter() {

    if (options_->penalty_update) {
        infea_measure_model_ = myQP_->get_infea_measure_model();

        // prin/tf("infea_measure_model = %23.16e\n",infea_measure_model_);
        // printf("infea_measure_ = %23.16e\n",infea_measure_);
        if (infea_measure_model_ > options_->penalty_update_tol) {
            double infea_measure_model_tmp = infea_measure_model_;//temporarily store the value
            double rho_trial = rho_;//the temporary trial value for rho
            setupLP();

            try {
                myLP_->solveLP(stats_);
            }
            catch (LP_NOT_OPTIMAL) {
                exitflag_ = myLP_->get_status();
                THROW_EXCEPTION(LP_NOT_OPTIMAL, LP_NOT_OPTIMAL_MSG);
            }
            //calculate the infea_measure of the LP
            double infea_measure_infty = myLP_->get_infea_measure_model();

            //     printf("infea_measure_infty = %23.16e\n",infea_measure_infty);
            if (infea_measure_infty <= options_->penalty_update_tol) {
                //try to increase the penalty parameter to a number such that the
                // infeasibility measure of QP model with such penalty parameter
                // becomes zero

                while (infea_measure_model_ > options_->penalty_update_tol) {
                    if (rho_trial >= options_->rho_max) {
                        break;
                    }//TODO:safeguarded procedure...put here for now

                    rho_trial = min(options_->rho_max,
                                    rho_trial*options_->increase_parm);  //increase rho

                    stats_->penalty_change_trial_addone();

                    myQP_->update_penalty(rho_trial);

                    try {
                        myQP_->solveQP(stats_, options_);
                    }
                    catch (QP_NOT_OPTIMAL) {
                        exitflag_ = myQP_->get_status();
                        break;
                    }

                    //recalculate the infeasibility measure of the model by
                    // calculating the one norm of the slack variables

                    infea_measure_model_ = myQP_->get_infea_measure_model();

                }
            } else {
                while ((infea_measure_ - infea_measure_model_) <
                        options_->eps1 * (infea_measure_ - infea_measure_infty) &&
                        (stats_->penalty_change_trial <
                         options_->penalty_iter_max)) {


                    if (rho_trial >= options_->rho_max) {
                        break;
                    }

                    //try to increase the penalty parameter to a number such that
                    // the incurred reduction for the QP model is to a ratio to the
                    // maximum possible reduction for current linear model.
                    rho_trial = min(options_->rho_max, rho_trial*options_->increase_parm);

                    stats_->penalty_change_trial_addone();

                    myQP_->update_penalty(rho_trial);

                    try {
                        myQP_->solveQP(stats_, options_);
                    }
                    catch (QP_NOT_OPTIMAL) {
                        exitflag_ = myQP_->get_status();
                        break;
                    }

                    //recalculate the infeasibility measure of the model by
                    // calculating the one norm of the slack variables

                    infea_measure_model_ = myQP_->get_infea_measure_model();
                }
            }
            //if any change occurs
            if (rho_trial > rho_) {
                //printf("lhs = %23.16e\n",
                //rho_trial * infea_measure_ - get_obj_QP());
                //printf("rhs = %23.16e\n",options_->eps2 * rho_trial *(infea_measure_ - infea_measure_model_));
                if (rho_trial * infea_measure_ - get_obj_QP()>=
                        options_->eps2 * rho_trial *
                        (infea_measure_ - infea_measure_model_)) {
                    stats_->penalty_change_Succ_addone();

                    options_->eps1 +=
                        (1 - options_->eps1) * options_->eps1_change_parm;

                    //use the new solution as the search direction
                    get_search_direction();
                    rho_ = rho_trial; //update to the class variable
                    get_trial_point_info();
                    qp_obj_ = get_obj_QP();
                    double P1_x = obj_value_ + rho_ * infea_measure_;
                    double P1_x_trial = obj_value_trial_ + rho_ * infea_measure_trial_;

                    actual_reduction_ = P1_x - P1_x_trial;
                    pred_reduction_ = rho_ * infea_measure_ - get_obj_QP();

                } else {
                    stats_->penalty_change_Fail_addone();
                    infea_measure_model_ = infea_measure_model_tmp;
                    QPinfoFlag_.Update_penalty = true;

                }
            }
            else if(infea_measure_<options_->opt_prim_fea_tol*1.0e-3) {
                //            rho_ = rho_*0.5;
                //            shared_ptr<Vector> sol_tmp = make_shared<Vector>(nVar_ + 2 * nCon_);
                //            myQP_->update_penalty(rho_);
                //
                //            try {
                //                myQP_->solveQP(stats_, options_);
                //            }
                //            catch (QP_NOT_OPTIMAL) {
                //                handle_error("QP NOT OPTIMAL");
                //            }
                //
                //            //recalculate the infeasibility measure of the model by
                //            // calculating the one norm of the slack variables
                //
                //            infea_measure_model_ = myQP_->get_infea_measure_model();

                //            p_k_->copy_vector(sol_tmp);
                //            get_trial_point_info();
                //            qp_obj = get_obj_QP();
            }
        }
    }
}


/**
 * @brief Use the Ipopt Reference Options and set it to default values.
 */
void Algorithm::setDefaultOption() {

    roptions = new Ipopt::RegisteredOptions();
    roptions->SetRegisteringCategory("trust-region");
    roptions->AddNumberOption("eta_c", "trust-region parameter for the ratio test.",
                              0.25,
                              "If ratio<=eta_c, then the trust-region radius for the next "
                              "iteration will be decreased for the next iteration.");
    roptions->AddNumberOption("eta_s", "trust-region parameter for the ratio test.",
                              1.0e-8,
                              "The trial point will be accepted if ratio>= eta_s. ");
    roptions->AddNumberOption("eta_e", "trust-region parameter for the ratio test.",
                              0.75,
                              "If ratio>=eta_e and the search direction hits the  "
                              "trust-region boundary, the trust-region radius will "
                              " be increased for the next iteration.");
    roptions->AddNumberOption("gamma_c", "radius update parameter",
                              0.5,
                              "If the trust-region radius is going to be decreased,"
                              " then it will be set as gamma_c*delta, where delta "
                              "is current trust-region radius.");
    roptions->AddNumberOption("gamma_e", "radius update parameter",
                              2.0,
                              "If the trust-region radius is going to be "
                              "increased, then it will be set as gamma_e*delta,"
                              "where delta is current trust-region radius.");
    roptions->AddNumberOption("delta_0", "initial trust-region radius value", 1.0);
    roptions->AddNumberOption("delta_max", "the maximum value of trust-region radius"
                              " allowed for the radius update", 1.0e8);

    roptions->SetRegisteringCategory("Penalty Update");
    roptions->AddNumberOption("eps1", "penalty update parameter", 0.3, "");
    roptions->AddNumberOption("eps2", "penalty update parameter", 1.0e-6, "");
    roptions->AddNumberOption("print_level_penalty_update",
                              "print level for penalty update", 0);
    roptions->AddNumberOption("rho_max", "maximum value of penalty parameter", 1.0e6);
    roptions->AddNumberOption("increase_parm",
                              "the number which will be use for scaling the new "
                              "penalty parameter", 10);
    roptions->AddIntegerOption("penalty_iter_max",
                               "maximum number of penalty paramter update allowed in a "
                               "single iteration in the main algorithm", 10);
    roptions->AddIntegerOption("penalty_iter_max_total",
                               "maximum number of penalty paramter update allowed "
                               "in total", 100);

    roptions->SetRegisteringCategory("Optimality Test");
    roptions->AddIntegerOption("testOption_NLP", "Level of Optimality test for "
                               "NLP", 0);
    roptions->AddStringOption2("auto_gen_tol",
                               "Tell the algorithm to automatically"
                               "generate the tolerance level for optimality test "
                               "based on information from NLP",
                               "no",
                               "no", "will use user-defined values of tolerance for"
                               " the optimality test",
                               "yes", "will automatically generate the tolerance "
                               "level for the optimality test");
    roptions->AddNumberOption("opt_tol", "", 1.0e-5);
    roptions->AddNumberOption("active_set_tol", "", 1.0e-5);
    roptions->AddNumberOption("opt_compl_tol", "", 1.0e-6);
    roptions->AddNumberOption("opt_dual_fea_tol", " ", 1.0e-6);
    roptions->AddNumberOption("opt_prim_fea_tol", " ", 1.0e-5);
    roptions->AddNumberOption("opt_second_tol", " ", 1.0e-8);

    roptions->SetRegisteringCategory("General");
    roptions->AddNumberOption("step_size_tol", "the smallest stepsize can be accepted"
                              "before concluding convergence",
                              1.0e-15);
    roptions->AddNumberOption("iter_max",
                              "maximum number of iteration for the algorithm", 10);
    roptions->AddNumberOption("print_level", "print level for main algorithm", 2);
    roptions->AddStringOption2(
        "second_order_correction",
        "Tells the algorithm to calculate the second-order correction step "
        "during the main iteration"
        "yes",
        "no", "not calculate the soc steps",
        "yes", "will calculate the soc steps",
        "");

    roptions->SetRegisteringCategory("QPsolver");
    roptions->AddIntegerOption("testOption_QP",
                               "Level of Optimality test for QP", -99);
    roptions->AddNumberOption("iter_max_qp", "maximum number of iteration for the "
                              "QP solver in solving each QP", 100);
    roptions->AddNumberOption("print_level_qp", "print level for QP solver", 0);

    //    roptions->AddStringOption("QPsolverChoice",
    //		    "The choice of QP solver which will be used in the Algorithm",
    //		    "qpOASES");


    roptions->SetRegisteringCategory("LPsolver");
    //    roptions->AddStringOption("LPsolverChoice",
    //		    "The choice of LP solver which will be used in the Algorithm",
    //		    "qpOASES");

    roptions->AddIntegerOption("testOption_LP",
                               "Level of Optimality test for LP", -99);
    roptions->AddNumberOption("iter_max_lp", "maximum number of iteration for the "
                              "LP solver in solving each LP", 100);
    roptions->AddNumberOption("print_level_lp", "print level for LP solver", 0);

}





void Algorithm::second_order_correction() {
    //FIXME: check correctness
    if ((!isaccept_) && options_->second_order_correction) {
        isaccept_ = false;

#if DEBUG
        //#if CHECK_SOC
        //        EJournalLevel debug_print_level = options_->debug_print_level;
        //        SmartPtr<Journal> debug_jrnl = jnlst_->GetJournal("Debug");
        //        if (IsNull(debug_jrnl)) {
        //            debug_jrnl = jnlst_->AddFileJournal("Debug", "debug.out", Ipopt::J_ITERSUMMARY);
        //        }
        //        debug_jrnl->SetAllPrintLevels(debug_print_level);
        //        debug_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_ALL);
        //
        //
        //        cout << "       Entering the SOC STEP calculation\n";
        //        cout << "       class member isaccept is ";
        //        if (isaccept_)
        //            cout << "TRUE" << endl;
        //        else
        //            cout << "FALSE" << endl;
        //        cout << "---------------------------------------------------------\n";
        //
        //        cout << endl;
        //#endif
#endif

        shared_ptr<Vector> p_k_tmp = make_shared<Vector>(nVar_); //for temporarily storing data for p_k
        p_k_tmp->copy_vector(p_k_);

        shared_ptr<Vector> s_k = make_shared<Vector>(nVar_);//for storing solution for SOC
        shared_ptr<Vector> tmp_sol = make_shared<Vector>(nVar_ + 2 * nCon_);

        double norm_p_k_tmp = norm_p_k_;
        double qp_obj_tmp = qp_obj_;
        shared_ptr<Vector> Hp = make_shared<Vector>(nVar_);
        hessian_->times(p_k_, Hp); //Hp = H_k*p_k
        Hp->add_vector(grad_f_->values());//(H_k*p_k+g_k)
        myQP_->update_grad(Hp);
        myQP_->update_bounds(delta_, x_l_, x_u_, x_trial_, c_l_, c_u_, c_trial_);
        norm_p_k_ = p_k_->getInfNorm();

        try {
            myQP_->solveQP(stats_, options_);
        }
        catch (QP_NOT_OPTIMAL) {
            myQP_->WriteQPData(problem_name_+"qpdata.log");
            exitflag_ = myQP_->get_status();
            THROW_EXCEPTION(QP_NOT_OPTIMAL,QP_NOT_OPTIMAL_MSG);
        }
        tmp_sol->copy_vector(myQP_->get_optimal_solution());
        s_k->copy_vector(tmp_sol->values());

        qp_obj_ = get_obj_QP() + (qp_obj_tmp - rho_ * infea_measure_model_);
        p_k_->add_vector(s_k->values());
        get_trial_point_info();
        ratio_test();
        if (!isaccept_) {
            p_k_ ->copy_vector(p_k_tmp->values());
            qp_obj_ = qp_obj_tmp;
            norm_p_k_ = norm_p_k_tmp;
            myQP_->update_grad(grad_f_);
            myQP_->update_bounds(delta_, x_l_, x_u_, x_k_, c_l_, c_u_, c_k_);
        }
    }

}

/**
*@brief get the objective value of QP from myQP object
*@relates QPhandler.hpp
*@return QP obejctive
*/

double Algorithm::get_obj_QP() {
    return (myQP_->get_objective());
}


void Algorithm::print_final_stats() {
    if(options_ ->printLevel>0) {
        Ipopt::SmartPtr<Ipopt::Journal> stdout_jrnl = jnlst_->GetJournal("console");
        if (IsNull(stdout_jrnl)) {
            Ipopt::SmartPtr<Ipopt::Journal> stdout_jrnl =
                jnlst_->AddFileJournal("console", "stdout", Ipopt::J_ITERSUMMARY);
        }
        if (IsValid(stdout_jrnl)) {
            // Set printlevel for stdout
            stdout_jrnl->SetAllPrintLevels(options_->print_level);
            stdout_jrnl->SetPrintLevel(Ipopt::J_DBG, Ipopt::J_NONE);
        }
    }
    else {
        Ipopt::SmartPtr<Ipopt::Journal> logout_jrnl = jnlst_->GetJournal("file_output");
        if(IsNull(logout_jrnl)) {
            string  output_file_name = problem_name_+"_output.log";
            jnlst_->AddFileJournal("file_output", output_file_name.c_str(), Ipopt::J_ITERSUMMARY);
        }
        if (IsValid(logout_jrnl)) {
            logout_jrnl->SetPrintLevel(Ipopt::J_STATISTICS, Ipopt::J_NONE);
        }
    }

    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);
    switch (exitflag_) {
    case OPTIMAL:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "OPTIMAL");
        break;
    case PRED_REDUCTION_NEGATIVE:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "PRED_REDUCTION_NEGATIVE");
        break;
    case INVALID_NLP :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "INVALID_NLP");
        break;
    case EXCEED_MAX_ITER :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "EXCEED_MAX_ITER");
        break;
    case EXCEED_TIME_LIMITS :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "EXCEED_TIME_LIMITS");
        break;
    case QP_OPTIMAL:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_SOL_IS_NOT_KKT_POINT");
        break;
    case QPERROR_INTERNAL_ERROR :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_INTERNAL_ERROR");
        break;
    case QPERROR_INFEASIBLE :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_INFEASIBLE");
        break;
    case QPERROR_UNBOUNDED :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_UNBOUNDED");
        break;
    case QPERROR_EXCEED_MAX_ITER :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_EXCEED_MAX_ITER");
        break;
    case QPERROR_NOTINITIALISED :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QP_NOTINITIALISED");
        break;
    case AUXINPUT_NOT_OPTIMAL :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "AUXINPUT_NOT_OPTIMAL");
        break;
    case CONVERGE_TO_NONOPTIMAL :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "CONVERGE_TO_NONOPTIMAL");
        break;

    case QPERROR_PREPARINGAUXILIARYQP:

        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QPERROR_PREPARINGAUXILIARYQP");
        break;
    case QPERROR_AUXILIARYQPSOLVED:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QPERROR_AUXILIARYQPSOLVED");
        break;
    case QPERROR_PERFORMINGHOMOTOPY  :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QPERROR_PERFORMINGHOMOTOPY");
        break;
    case QPERROR_HOMOTOPYQPSOLVED    :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QPERROR_HOMOTOPYQPSOLVED");
        break;
    case TRUST_REGION_TOO_SMALL:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "TRUST_REGION_TOO_SMALL");
        break;

    case STEP_LARGER_THAN_TRUST_REGION:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "STEP_LARGER_THAN_TRUST_REGION");
        break;
    case QPERROR_UNKNOWN:
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "QPERROR_UNKNOWN");
        break;
    case UNKNOWN :
        jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                       "Exitflag:                                                   %23s\n",
                       "UNKNOWN ERROR");

        break;

    }
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Number of Variables                                         %23i\n",
                   nVar_);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Number of Constraints                                       %23i\n",
                   nCon_);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Iterations:                                                 %23i\n",
                   stats_->iter);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "QP Solver Iterations:                                       %23i\n",
                   stats_->qp_iter);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Final Objectives:                                           %23.16e\n",
                   obj_value_);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Total Times:                                                %23.16e\n",
                   (float)stats_->total_time/CLOCKS_PER_SEC);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Primal Feasibility Violation                                %23.16e\n",
                   opt_status_.primal_violation);

    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Dual Feasibility Violation                                  %23.16e\n",
                   opt_status_.dual_violation);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Complmentarity Violation                                    %23.16e\n",
                   opt_status_.compl_violation);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "Stationarity Violation                                      %23.16e\n",
                   opt_status_.stationarity_violation);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "||p_k||                                                     %23.16e\n",
                   norm_p_k_);
    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN,
                   "||c_k||                                                     %23.16e\n",
                   infea_measure_);

    jnlst_->Printf(Ipopt::J_ITERSUMMARY, Ipopt::J_MAIN, DOUBLE_LONG_DIVIDER);

}

}//END_NAMESPACE_SQPHOTSTART




