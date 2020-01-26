/** Copyright (C) 2019
 * All Rights Reserved.
 *
 * Authors: Xinyi Luo
 * Date:2019-07
 */
#include "sqphot/QpHandler.hpp"
#include "IpOptionsList.hpp"
#include "sqphot/QoreInterface.hpp"
#include "sqphot/QpOasesInterface.hpp"
#include "sqphot/SQPDebug.hpp"

using namespace std;
using namespace Ipopt;

namespace RestartSqp {

QpHandler::QpHandler(shared_ptr<const SqpNlpSizeInfo> nlp_sizes, QPType qptype,
                     bool slack_formulation, SmartPtr<Journalist> jnlst,
                     SmartPtr<const OptionsList> options)
 : jnlst_(jnlst)
 , last_penalty_parameter_(-1.)
 , slack_formulation_(slack_formulation)
{
  // Initialize the matrix structure: where in the Jacobian are multiples of the
  // identity
  num_nlp_variables_ = nlp_sizes->get_num_variables();
  num_nlp_constraints_ = nlp_sizes->get_num_constraints();

  if (!slack_formulation_) {
    num_qp_variables_ = num_nlp_variables_ + 2 * num_nlp_constraints_;
    num_qp_constraints_ = num_nlp_constraints_;

    // positive slacks for constraints
    identity_matrix_positions_.add_matrix(1, num_nlp_variables_ + 1,
                                          num_nlp_constraints_, 1.);
    // negative slacks for constraints
    identity_matrix_positions_.add_matrix(1, num_nlp_variables_ +
                                                 num_nlp_constraints_ + 1,
                                          num_nlp_constraints_, -1.);
  } else {
    num_qp_variables_ = 3 * num_nlp_variables_ + 2 * num_nlp_constraints_;
    num_qp_constraints_ = num_nlp_variables_ + num_nlp_constraints_;

    // positive slacks for constraints
    identity_matrix_positions_.add_matrix(1, num_nlp_variables_ + 1,
                                          num_nlp_constraints_, 1.);
    // negative slacks for constraints
    identity_matrix_positions_.add_matrix(1, num_nlp_variables_ +
                                                 num_nlp_constraints_ + 1,
                                          num_nlp_constraints_, -1.);
    // original variables
    identity_matrix_positions_.add_matrix(num_nlp_constraints_ + 1, 1,
                                          num_nlp_variables_, 1.);
    // postitive slacks for variable bounds
    identity_matrix_positions_.add_matrix(num_nlp_constraints_ + 1,
                                          num_nlp_variables_ +
                                              2 * num_nlp_constraints_ + 1,
                                          num_nlp_variables_, 1.);
    // negative slacks for variable bounds
    identity_matrix_positions_.add_matrix(num_nlp_constraints_ + 1,
                                          2 * num_nlp_variables_ +
                                              2 * num_nlp_constraints_ + 1,
                                          num_nlp_variables_, -1);
  }

  // Determine which solver should be used
  int enum_int;
  options->GetEnumValue("qp_solver_choice", enum_int, "");
  qp_solver_choice_ = QpSolver(enum_int);

  // Create solver objects
  switch (qp_solver_choice_) {
    case QPOASES:
      qp_solver_interface_ = make_shared<QpOasesInterface>(
          num_qp_variables_, num_qp_constraints_, qptype, options, jnlst);
      break;
    case QORE:
      qp_solver_interface_ = make_shared<QoreInterface>(
          num_qp_variables_, num_qp_constraints_, qptype, options, jnlst);
      break;
  }

  // Set the bounds for the constraint slack variables; they will not change
  // later
  for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
    qp_solver_interface_->set_lower_variable_bound(i, 0.);
    qp_solver_interface_->set_upper_variable_bound(i, INF);
  }

#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  qpOASESInterface_ =
      make_shared<qpOASESInterface>(nlp_sizes, qptype, options, jnlst);
  QOREInterface_ =
      make_shared<QOREInterface>(nlp_sizes, qptype, options, jnlst);
  W_b_qpOASES_ = new ActivityStatus[num_qp_variables_];
  W_c_qpOASES_ = new ActivityStatus[num_qp_constraints_];
  W_b_qore_ = new ActivityStatus[num_qp_variables_];
  W_c_qore_ = new ActivityStatus[num_qp_constraints_];
#endif
#endif
}

/**
 *Default destructor
 */
QpHandler::~QpHandler()
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  delete[] W_b_qpOASES_;
  delete[] W_c_qpOASES_;
  delete[] W_b_qore_;
  delete[] W_c_qore_;
#endif
#endif
}

/**
 * Setup the bounds for the QP subproblems according to the information from
 * current
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

void QpHandler::set_bounds(double trust_region_radius,
                           shared_ptr<const Vector> x_l,
                           shared_ptr<const Vector> x_u,
                           shared_ptr<const Vector> x_k,
                           shared_ptr<const Vector> c_l,
                           shared_ptr<const Vector> c_u,
                           shared_ptr<const Vector> c_k)
{

#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  set_bounds_debug(delta, x_l, x_u, x_k, c_l, c_u, c_k);
#endif
#endif

  /*-------------------------------------------------------------*/
  /* Set lbA, ubA as well as lb and ub as qpOASES differentiates */
  /*the bound constraints from the linear constraints            */
  /*-------------------------------------------------------------*/

  // TODO: There should be no difference between qpOASES and QORE
  if (!slack_formulation_) {
    // First set the variables bounds for the step p
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bound(
          i, max(x_l_vals[i] - x_k_vals[i], -trust_region_radius));
      qp_solver_interface_->set_upper_variable_bound(
          i, min(x_u_vals[i] - x_k_vals[i], trust_region_radius));
    }

    // The bounds on the slack variables have already been set in the
    // constructor

    // Now set the constraint bounds
    const double* c_k_vals = c_k->get_values();
    const double* c_l_vals = c_l->get_values();
    const double* c_u_vals = c_u->get_values();
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_constraint_bound(i, c_l_vals[i] -
                                                              c_k_vals[i]);
      qp_solver_interface_->set_upper_constraint_bound(i, c_u_vals[i] -
                                                              c_k_vals[i]);
    }
  }

  else {
    // First set the variables bounds for the step p.  This is only the trust
    // region
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bound(i, -trust_region_radius);
      qp_solver_interface_->set_upper_variable_bound(i, trust_region_radius);
    }

    // Now set bounds for the original constraints
    const double* c_k_vals = c_k->get_values();
    const double* c_l_vals = c_l->get_values();
    const double* c_u_vals = c_u->get_values();
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_constraint_bound(i, c_l_vals[i] -
                                                              c_k_vals[i]);
      qp_solver_interface_->set_upper_constraint_bound(i, c_u_vals[i] -
                                                              c_k_vals[i]);
    }

    // ...and the bounds for the variable bounds with slacks
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_constraint_bound(
          num_nlp_constraints_ + i, x_l_vals[i] - x_k_vals[i]);
      qp_solver_interface_->set_upper_constraint_bound(
          num_nlp_constraints_ + i, x_u_vals[i] - x_k_vals[i]);
    }
  }
#if 0
  if (qp_solver_choice_ != QORE) {
#ifdef NEW_FORMULATION
    const double* c_k_vals = c_k->get_values();
    const double* c_l_vals = c_l->get_values();
    const double* c_u_vals = c_u->get_values();
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_constraint_bounds(i, c_l_vals[i] -
                                                               c_k_vals[i]);
      qp_solver_interface_->set_upper_constraint_bounds(i, c_u_vals[i] -
                                                               c_k_vals[i]);
    }
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_constraint_bounds(
          num_nlp_constraints_ + i, x_l_vals[i] - x_k_vals[i]);
      qp_solver_interface_->set_upper_constraint_bounds(
          num_nlp_constraints_ + i, x_u_vals[i] - x_k_vals[i]);
    }

    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(i, -trust_region_radius);
      qp_solver_interface_->set_upper_variable_bounds(i, trust_region_radius);
    }
    for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(i, 0.);
      qp_solver_interface_->set_upper_variable_bounds(i, INF);
    }
#else
    const double* c_k_vals = c_k->get_values();
    const double* c_l_vals = c_l->get_values();
    const double* c_u_vals = c_u->get_values();
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lbA(i, c_l_vals[i] - c_k_vals[i]);
      qp_solver_interface_->set_ubA(i, c_u_vals[i] - c_k_vals[i]);
    }
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lb(i, max(x_l_vals[i] - x_k_vals[i], -delta));
      qp_solver_interface_->set_ub(i, min(x_u_vals[i] - x_k_vals[i], delta));
    }
    /**
     * only set the upper bound for the last half to be infinity(those are slack
     * variables).
     * The lower bounds are initialized as 0
     */
    for (int i = 0; i < num_nlp_constraints_ * 2; i++) {
      qp_solver_interface_->set_lb(num_nlp_variables_ + i, 0.);
      qp_solver_interface_->set_ub(num_nlp_variables_ + i, INF);
    }
#endif
  }
  /*-------------------------------------------------------------*/
  /* Only set lb and ub, where lb = [lbx;lbA]; and ub=[ubx; ubA] */
  /*-------------------------------------------------------------*/
  else {
#ifndef NEW_FORMULATION
    // For p_k
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lb(i, max(x_l_vals[i] - x_k_vals[i], -delta));
      qp_solver_interface_->set_ub(i, min(x_u_vals[i] - x_k_vals[i], delta));
    }

    // For u,v
    for (int i = 0; i < num_nlp_constraints_ * 2; i++) {
      qp_solver_interface_->set_lb(num_nlp_variables_ + i, 0.);
      qp_solver_interface_->set_ub(num_nlp_variables_ + i, INF);
    }
#endif

    const double* c_k_vals = c_k->get_values();
    const double* c_l_vals = c_l->get_values();
    const double* c_u_vals = c_u->get_values();
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(
          num_qp_variables_ + i, c_l_vals[i] - c_k_vals[i]);
      qp_solver_interface_->set_upper_variable_bounds(
          num_qp_variables_ + i, c_u_vals[i] - c_k_vals[i]);
    }

#ifdef NEW_FORMULATION
    const double* x_k_vals = x_k->get_values();
    const double* x_l_vals = x_l->get_values();
    const double* x_u_vals = x_u->get_values();
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(i, -trust_region_radius);
      qp_solver_interface_->set_upper_variable_bounds(i, trust_region_radius);

      qp_solver_interface_->set_lower_variable_bounds(
          num_qp_variables_ + num_nlp_constraints_ + i,
          x_l_vals[i] - x_k_vals[i]);
      qp_solver_interface_->set_upper_variable_bounds(
          num_qp_variables_ + num_nlp_constraints_ + i,
          x_u_vals[i] - x_k_vals[i]);
    }
    for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
      qp_solver_interface_->set_upper_variable_bounds(i, INF);
    }
// DEBUG
//        qp_solver_interface_->getLb()->print("lb");
//        qp_solver_interface_->getUb()->print("ub");
#endif
  }
#endif
}

/**
 * This function sets up the object vector g of the QP problem
 * The (2*nCon+nVar) vector g_^T in QP problem will be the same as
 * [grad_f^T, rho* e^T], where the unit vector is of length (2*nCon).
 * @param grad      Gradient vector from nlp class
 * @param rho       Penalty Parameter
 */

void QpHandler::set_linear_qp_objective_coefficients(
    shared_ptr<const Vector> gradient, double penalty_parameter)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  for (int i = 0; i < num_qp_variables_; i++)
    if (i < num_nlp_variables_) {
      qpOASESInterface_->set_gradient(i, grad->get_value(i));
      QOREInterface_->set_gradient(i, grad->get_value(i));
    } else {
      qpOASESInterface_->set_gradient(i, rho);
      QOREInterface_->set_gradient(i, rho);
    }
#endif
#endif

  // Set the linear coefficients corresponding to the p variables
  for (int i = 0; i < num_nlp_variables_; i++) {
    qp_solver_interface_->set_linear_objective_coefficient(
        i, gradient->get_value(i));
  }

  // Set the linear coefficients corresponding to the penalty variables.  We
  // only need to do this if the penalty value has changed since the last time
  if (penalty_parameter != last_penalty_parameter_)
    for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
      qp_solver_interface_->set_linear_objective_coefficient(i,
                                                             penalty_parameter);
      last_penalty_parameter_ = penalty_parameter;
    }
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
void QpHandler::set_hessian(shared_ptr<const SparseTripletMatrix> hessian)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  qpOASESInterface_->set_hessian(hessian);
  QOREInterface_->set_hessian(hessian);
#endif
#endif
  qp_solver_interface_->set_objective_hessian(hessian);
}

/**
 * @brief This function sets up the matrix A in the QP subproblem
 * The matrix A in QP problem will be concatenate as [J I -I]
 * @param jacobian  the Matrix object for Jacobian from c(x)
 */
void QpHandler::set_jacobian(shared_ptr<const SparseTripletMatrix> jacobian)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  qpOASESInterface_->set_jacobian(jacobian, identity_matrix_positions_);
  QOREInterface_->set_jacobian(jacobian, identity_matrix_positions_);
#endif
#endif
  qp_solver_interface_->set_constraint_jacobian(jacobian,
                                                identity_matrix_positions_);
}

#if 0
/**
 * @brief This function updates the constraint if there is any changes to
 * the iterates
 */
void QpHandler::update_bounds(double delta, shared_ptr<const Vector> x_l,
                              shared_ptr<const Vector> x_u,
                              shared_ptr<const Vector> x_k,
                              shared_ptr<const Vector> c_l,
                              shared_ptr<const Vector> c_u,
                              shared_ptr<const Vector> c_k)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  set_bounds_debug(delta, x_l, x_u, x_k, c_l, c_u, c_k);
#endif
#endif

#ifndef NEW_FORMULATION
  if (qp_solver_choice_ != QORE) {
    if (qp_solver_choice_ == GUROBI || qp_solver_choice_ == CPLEX) {
      qp_solver_interface_->reset_constraints();
    }
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lbA(i, c_l->get_value(i) - c_k->get_value(i));
    }

    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lb(
          i, max(x_l->get_value(i) - x_k->get_value(i), -delta));
      qp_solver_interface_->set_ub(
          i, min(x_u->get_value(i) - x_k->get_value(i), delta));
    }
  } else {
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lb(
          i, max(x_l->get_value(i) - x_k->get_value(i), -delta));
      qp_solver_interface_->set_ub(
          i, min(x_u->get_value(i) - x_k->get_value(i), delta));
    }
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lb(num_nlp_variables_ + 2 * num_nlp_constraints_ +
                                   i,
                               c_l->get_value(i) - c_k->get_value(i));
      qp_solver_interface_->set_ub(num_nlp_variables_ + 2 * num_nlp_constraints_ +
                                   i,
                               c_u->get_value(i) - c_k->get_value(i));
    }
  }
#else
  if (qp_solver_choice_ == QORE) {
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(i, -delta);
      qp_solver_interface_->set_upper_variable_bounds(i, delta);
    }
    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(
          num_qp_variables_ + i, c_l->get_value(i) - c_k->get_value(i));
      qp_solver_interface_->set_upper_variable_bounds(
          num_qp_variables_ + i, c_u->get_value(i) - c_k->get_value(i));
    }

    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(
          num_qp_variables_ + num_nlp_constraints_ + i,
          x_l->get_value(i) - x_k->get_value(i));
      qp_solver_interface_->set_upper_variable_bounds(
          num_qp_variables_ + num_nlp_constraints_ + i,
          x_u->get_value(i) - x_k->get_value(i));
    }
  } else {
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bounds(i, -delta);
      qp_solver_interface_->set_upper_variable_bounds(i, delta);
    }

    for (int i = 0; i < num_nlp_constraints_; i++) {
      qp_solver_interface_->set_lower_constraint_bounds(
          i, c_l->get_value(i) - c_k->get_value(i));
      qp_solver_interface_->set_upper_constraint_bounds(
          i, c_u->get_value(i) - c_k->get_value(i));
    }
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_constraint_bounds(
          num_nlp_constraints_ + i, x_l->get_value(i) - x_k->get_value(i));
      qp_solver_interface_->set_upper_constraint_bounds(
          num_nlp_constraints_ + i, x_u->get_value(i) - x_k->get_value(i));
    }
  }
#endif
}
#endif

/**
 * @brief This function updates the vector g in the QP subproblem when there are
 * any
 * change to the values of penalty parameter
 *
 * @param rho               penalty parameter
 * @param nVar              number of variables in NLP
 */

void QpHandler::update_penalty_parameter(double penalty_parameter)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
    qpOASESInterface_->set_gradient(i, rho);
    QOREInterface_->set_gradient(i, rho);
  }
#endif
#endif
  if (penalty_parameter != last_penalty_parameter_) {
    for (int i = num_nlp_variables_; i < num_qp_variables_; i++)
      qp_solver_interface_->set_linear_objective_coefficient(i,
                                                             penalty_parameter);
  }
}

#if 0
/**
 * @brief This function updates the vector g in the QP subproblem when there are
 * any
 * change to the values of gradient in NLP
 *
 * @param grad              the gradient vector from NLP
 */
void QpHandler::update_grad(shared_ptr<const Vector> grad)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER

  for (int i = 0; i < num_nlp_variables_; i++) {
    qpOASESInterface_->set_gradient(i, grad->get_value(i));
    QOREInterface_->set_gradient(i, grad->get_value(i));
  }
#endif
#endif
  for (int i = 0; i < num_nlp_variables_; i++)
    qp_solver_interface_->set_linear_objective_coefficients(i,
                                                            grad->get_value(i));
}
#endif

/**
 *@brief Solve the QP with objective and constraints defined by its class
 *members
 */
QpSolverExitStatus QpHandler::solve(shared_ptr<Statistics> stats)
{

#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  QOREInterface_->optimizeQP(stats);
  qpOASESInterface_->optimizeQP(stats);
  bool qpOASES_optimal =
      test_optimality(qpOASESInterface_, QPOASES, W_b_qpOASES_, W_c_qpOASES_);
  bool qore_optimal =
      test_optimality(QOREInterface_, QORE, W_b_qore_, W_c_qore_);
  if (!qpOASES_optimal || !qore_optimal) {
    testQPsolverDifference();
  }

#endif
#endif

  qp_solver_interface_->optimize(stats);
  QpSolverExitStatus qp_exit_status = qp_solver_interface_->get_solver_status();

  // Check if the QP solver really returned an optimal solution.
  if (qp_exit_status == QPEXIT_OPTIMAL) {
    double qp_error_tol = 1e-8;
    KktError qp_kkt_error = qp_solver_interface_->calc_kkt_error(J_DETAILED);
    last_kkt_error_ = qp_kkt_error.worst_violation;

    bool isOptimal = (last_kkt_error_ < qp_error_tol);
    if (!isOptimal) {
      jnlst_->Printf(J_WARNING, J_MAIN, "WARNING: QP solver KKT error is %e\n",
                     last_kkt_error_);
    }
    if (jnlst_->ProduceOutput(J_MOREVECTOR, J_MAIN)) {
      jnlst_->Printf(J_MOREVECTOR, J_MAIN, "\nQP solver solution:\n");
      qp_solver_interface_->get_primal_solution()->print(
          "primal solution", jnlst_, J_MOREVECTOR, J_MAIN);
      qp_solver_interface_->get_bounds_multipliers()->print(
          "bound multipliers", jnlst_, J_MOREVECTOR, J_MAIN);
      qp_solver_interface_->get_constraint_multipliers()->print(
          "constraint multipliers", jnlst_, J_MOREVECTOR, J_MAIN);
    }
  } else {
    last_kkt_error_ = 1e20;
    assert(false && "We do not handle failures of the QP solvers yet");
  }

  return qp_exit_status;
}

shared_ptr<const Vector> QpHandler::get_bounds_multipliers() const
{
  shared_ptr<const Vector> retval = make_shared<Vector>(
      num_nlp_variables_,
      qp_solver_interface_->get_bounds_multipliers()->get_values());
  return retval;
}

/**
* @brief Get the multipliers corresponding to the constraints
*/
std::shared_ptr<const Vector> QpHandler::get_constraint_multipliers() const
{
  shared_ptr<const Vector> retval = make_shared<Vector>(
      num_nlp_constraints_,
      qp_solver_interface_->get_constraint_multipliers()->get_values());
  return retval;
}

double QpHandler::get_qp_objective() const
{
  return qp_solver_interface_->get_optimal_objective_value();
}

double QpHandler::get_qp_kkt_error() const
{
  return last_kkt_error_;
}
#if 0
void QpHandler::set_hessian(shared_ptr<const SparseTripletMatrix> Hessian)
{

#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  QOREInterface_->set_hessian(Hessian);
  qpOASESInterface_->set_hessian(Hessian);
#endif
#endif
  qp_solver_interface_->set_objective_hessian(Hessian);
}

void QpHandler::set_jacobian(shared_ptr<const SparseTripletMatrix> Jacobian)
{

#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  QOREInterface_->set_jacobian(Jacobian, identity_matrix_positions_);
  qpOASESInterface_->set_jacobian(Jacobian, identity_matrix_positions_);
#endif
#endif
  qp_solver_interface_->set_constraint_jacobian(Jacobian,
                                                identity_matrix_positions_);
}
#endif

void QpHandler::decrease_trust_region(double trust_region)
{
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
  for (int i = 0; i < num_nlp_variables_; i++) {
    qpOASESInterface_->set_lb(
        i, max(x_l->get_value(i) - x_k->get_value(i), -delta));
    qpOASESInterface_->set_ub(
        i, min(x_u->get_value(i) - x_k->get_value(i), delta));
    QOREInterface_->set_lb(i,
                           max(x_l->get_value(i) - x_k->get_value(i), -delta));
    QOREInterface_->set_ub(i,
                           min(x_u->get_value(i) - x_k->get_value(i), delta));
  }
#endif
#endif

  if (slack_formulation_) {
    for (int i = 0; i < num_nlp_variables_; i++) {
      qp_solver_interface_->set_lower_variable_bound(i, -trust_region);
      qp_solver_interface_->set_upper_variable_bound(i, trust_region);
    }
  } else {
    for (int i = 0; i < num_nlp_variables_; i++) {
      double current_bound = qp_solver_interface_->get_lower_variable_bound(i);
      if (current_bound < -trust_region) {
        qp_solver_interface_->set_lower_variable_bound(i, -trust_region);
      }
      current_bound = qp_solver_interface_->get_upper_variable_bound(i);
      if (current_bound > trust_region) {
        qp_solver_interface_->set_upper_variable_bound(i, trust_region);
      }
    }
  }
}

void QpHandler::write_qp_data(const std::string& filename)
{
  qp_solver_interface_->write_qp_data_to_file(filename);
}

#if 0
Exitflag QpHandler::get_status()
{
  return (qp_solver_interface_->get_status());
}
#endif

#if 0
bool QpHandler::test_optimality(shared_ptr<Qpqp_solver_interface_> qpqp_solver_interface_,
                                QpSolver qpSolver,
                                ActivityStatus* bounds_working_set,
                                ActivityStatus* constraints_working_set)
{
  qpOptimalStatus_ = qpqp_solver_interface_->get_optimality_status();
  return (qpqp_solver_interface_->calc_kkt_error(constraints_working_set,
                                             bounds_working_set));
}
#endif
double QpHandler::get_model_infeasibility() const
{
  // Here we compute the constraint violation from the penatly values.
  // The QP primal variables are ordered so that the SQP primal variables
  // are first, and the remaining ones are the penalty variables.
  return qp_solver_interface_->get_primal_solution()->calc_subvector_one_norm(
      num_nlp_variables_, num_qp_variables_ - num_nlp_variables_);
}

#if 0
void QpHandler::get_active_set(ActivityStatus* A_c, ActivityStatus* A_b,
                               shared_ptr<Vector> x, shared_ptr<Vector> Ax)
{
  // use the class member to get the qp problem information
  auto lb = qp_solver_interface_->get_lower_variable_bounds();
  auto ub = qp_solver_interface_->get_upper_variable_bounds();
  if (x == nullptr) {
    x = make_shared<Vector>(num_qp_variables_);
    x->copy_vector(get_primal_solution());
  }
  if (Ax == nullptr) {
    Ax = make_shared<Vector>(num_qp_constraints_);
    auto A = qp_solver_interface_->get_constraint_jacobian();
    A->multiply(x, Ax);
  }

  for (int i = 0; i < num_qp_variables_; i++) {
    if (abs(x->get_value(i) - lb->get_value(i)) < sqrt_m_eps) {
      if (abs(ub->get_value(i) - x->get_value(i)) < sqrt_m_eps) {
        A_b[i] = ACTIVE_EQUALITY;
      } else {
        A_b[i] = ACTIVE_BELOW;
      }
    } else if (abs(ub->get_value(i) - x->get_value(i)) < sqrt_m_eps) {
      A_b[i] = ACTIVE_ABOVE;
    } else {
      A_b[i] = INACTIVE;
    }
  }
  if (qp_solver_choice_ == QORE) {
    // if no x and Ax are input
    for (int i = 0; i < num_qp_constraints_; i++) {
      if (abs(Ax->get_value(i) - lb->get_value(i + num_qp_variables_)) <
          sqrt_m_eps) {
        if (abs(ub->get_value(i + num_qp_variables_) - Ax->get_value(i)) <
            sqrt_m_eps) {
          A_c[i] = ACTIVE_EQUALITY;
        } else {
          A_c[i] = ACTIVE_BELOW;
        }
      } else if (abs(ub->get_value(i + num_qp_variables_) - Ax->get_value(i)) <
                 sqrt_m_eps) {
        A_c[i] = ACTIVE_ABOVE;
      } else {
        A_c[i] = INACTIVE;
      }
    }
  } else {
    auto lbA = qp_solver_interface_->get_upper_constraint_bounds();
    auto ubA = qp_solver_interface_->get_upper_constraint_bounds();
    for (int i = 0; i < num_qp_constraints_; i++) {
      if (abs(Ax->get_value(i) - lbA->get_value(i)) < sqrt_m_eps) {
        if (abs(ubA->get_value(i) - Ax->get_value(i)) < sqrt_m_eps) {
          A_c[i] = ACTIVE_EQUALITY;
        } else {
          A_c[i] = ACTIVE_BELOW;
        }
      } else if (abs(ubA->get_value(i) - Ax->get_value(i)) < sqrt_m_eps) {
        A_c[i] = ACTIVE_ABOVE;
      } else {
        A_c[i] = INACTIVE;
      }
    }
  }
}
#endif
#if 0
void QpHandler::set_linear_qp_objective_coefficients(double rho)
{
  for (int i = 0; i < num_nlp_variables_; i++) {
    qp_solver_interface_->set_linear_objective_coefficients(i, rho);
  }
  for (int i = num_nlp_variables_; i < num_qp_variables_; i++) {
    qp_solver_interface_->set_linear_objective_coefficients(i, rho);
  }
}
#endif
#ifdef DEBUG
#ifdef COMPARE_QP_SOLVER
void QPhandler::set_bounds_debug(double delta, shared_ptr<const Vector> x_l,
                                 shared_ptr<const Vector> x_u,
                                 shared_ptr<const Vector> x_k,
                                 shared_ptr<const Vector> c_l,
                                 shared_ptr<const Vector> c_u,
                                 shared_ptr<const Vector> c_k)
{
  for (int i = 0; i < num_nlp_constraints_; i++) {
    qpOASESInterface_->set_lbA(i, c_l->get_value(i) - c_k->get_value(i));
    qpOASESInterface_->set_ubA(i, c_u->get_value(i) - c_k->get_value(i));
  }

  for (int i = 0; i < num_nlp_variables_; i++) {
    qpOASESInterface_->set_lb(
        i, max(x_l->get_value(i) - x_k->get_value(i), -delta));
    qpOASESInterface_->set_ub(
        i, min(x_u->get_value(i) - x_k->get_value(i), delta));
  }
  /**
   * only set the upper bound for the last 2*nCon entries (those are slack
   * variables).
   * The lower bounds are initialized as 0
   */
  for (int i = 0; i < num_nlp_constraints_ * 2; i++) {
    qpOASESInterface_->set_ub(num_nlp_variables_ + i, INF);
  }

  /*-------------------------------------------------------------*/
  /* Only set lb and ub, where lb = [lbx;lbA]; and ub=[ubx; ubA] */
  /*-------------------------------------------------------------*/
  for (int i = 0; i < num_nlp_variables_; i++) {
    QOREInterface_->set_lb(i,
                           max(x_l->get_value(i) - x_k->get_value(i), -delta));
    QOREInterface_->set_ub(i,
                           min(x_u->get_value(i) - x_k->get_value(i), delta));
  }

  for (int i = 0; i < num_nlp_constraints_ * 2; i++) {
    QOREInterface_->set_ub(num_nlp_variables_ + i, INF);
  }

  for (int i = 0; i < num_nlp_constraints_; i++) {
    QOREInterface_->set_lb(num_qp_variables_ + i,
                           c_l->get_value(i) - c_k->get_value(i));
    QOREInterface_->set_ub(num_qp_variables_ + i,
                           c_u->get_value(i) - c_k->get_value(i));
  }
}

bool QPhandler::testQPsolverDifference()
{
  shared_ptr<Vector> qpOASESsol = make_shared<Vector>(num_nlp_variables_);
  shared_ptr<Vector> QOREsol = make_shared<Vector>(num_nlp_variables_);
  shared_ptr<Vector> difference = make_shared<Vector>(num_nlp_variables_);
  qpOASESsol->print("qpOASESsol", jnlst_);
  QOREsol->print("QOREsol", jnlst_);
  qpOASESsol->copy_vector(qpOASESInterface_->get_optimal_solution());
  QOREsol->copy_vector(QOREInterface_->get_optimal_solution());
  difference->copy_vector(qpOASESsol);
  difference->add_vector(-1., QOREsol);
  double diff_norm = difference->calc_one_norm();
  if (diff_norm > 1.0e-8) {
    printf("difference is %10e\n", diff_norm);
    qpOASESsol->print("qpOASESsol");
    QOREsol->print("QOREsol");
    // TODO activate this again
    // QOREInterface_->WriteQPDataToFile(jnlst_, J_ALL, J_DBG);
  }
  assert(diff_norm < 1.0e-8);
  return true;
}

#endif
#endif
} // namespace SQPhotstart
