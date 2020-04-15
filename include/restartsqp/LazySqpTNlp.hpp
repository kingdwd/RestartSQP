/* Copyright (C) 2019
 * All Rights Reserved.
 *
 * Authors: Xinyi Luo
 * Date:2019-06
 */
#ifndef SQPHOTSTART_SHORTENEDSQPTNLP_HPP
#define SQPHOTSTART_SHORTENEDSQPTNLP_HPP

#include "restartsqp/SqpTNlp.hpp"

namespace RestartSqp {
/**
 * This is part of SQPhotstart
 *
 * This class is an adapter to solve a SqpTNlp with Ipopt.
 */
class LazySqpTNlp : public SqpTNlp
{

public:
  /** @brief Constructor that an instance of Ipopt's TNLP */
  LazySqpTNlp(std::shared_ptr<SqpTNlp> sqp_tnlp);

  /** Destructor*/
  ~LazySqpTNlp();

  /** Set the list of considered constraints.
   *
   *  This specifies the setset of constraints of the SqpTNlp provided in the
   *  constructor to be considered for the next solve. The indices start from 0.
   */
  void set_considered_constraints(int num_constraints, const int* constraint_indices);

  /**
   *@brief Get problem size information
   */
  bool get_nlp_info(int& num_variables, int& num_constraints,
                    int& num_nonzeros_jacobian, int& num_nonzeros_hessian,
                    std::string& nlp_name) override;

  /**
   *@brief get the bounds information from the NLP object
   */
  bool get_bounds_info(int num_variabes, double* variable_lower_bounds,
                       double* variable_upper_bounds, int num_constraints,
                       double* constraint_lower_bounds,
                       double* constraint_upper_bounds) override;

  /*
   * @brief Get the starting point from the NLP object.
   * TODO: add options_ to enable user to choose if to use default input or not
   */
  bool get_starting_point(int num_variables, bool init_primal_variables,
                          double* primal_variables, bool init_bound_multipliers,
                          double* bound_multipliers, int num_constraints,
                          bool init_constraint_multipliers,
                          double* constraint_multipliers) override;

  /**
   *@brief Evaluate the objective value
   */
  bool eval_objective_value(int num_variables, const double* primal_variables,
                            bool new_primal_variables,
                            double& objective_value) override;

  /**
   *@brief Evaluate gradient at point x
   */
  bool eval_objective_gradient(int num_variables,
                               const double* primal_variables,
                               bool new_primal_variables,
                               double* objective_gradient) override;

  /**
   * @brief Evaluate the constraints at point x
   *
   */
  bool eval_constraint_values(int num_variables, const double* primal_variables,
                              bool new_primal_variables, int num_constraints,
                              double* constraint_values) override;

  /**
   * @brief Get the matrix structure of the Jacobian
   * Always call this before the first time using @Eval_Jacobian
   */
  bool eval_constraint_jacobian(int num_variables,
                                const double* primal_variables,
                                bool new_primal_variables, int num_constraints,
                                int num_nonzeros_jacobian, int* row_indices,
                                int* column_indices,
                                double* nonzero_values) override;

  /**
   *@brief Evaluate Hessian of Lagragian function at  (x, lambda)
   */
  bool eval_lagrangian_hessian(
      int num_variables, const double* primal_variables,
      bool new_primal_variables, double objective_scaling_factor,
      int num_constraints, const double* constraint_multipliers,
      bool new_constraint_multipliers, int num_nonzeros_hessian,
      int* row_indices, int* column_indices, double* nonzero_valuess) override;

  /**
   * @brief Return the results of the optimization run to the user.
   */
  void finalize_solution(SqpSolverExitStatus status, int num_variables,
                         const double* primal_solution,
                         const double* bound_multipliers,
                         const ActivityStatus* bound_activity_status,
                         int num_constraints, const double* constraint_values,
                         const double* constraint_multipliers,
                         const ActivityStatus* constraint_activity_status,
                         double objective_value,
                         std::shared_ptr<const Statistics> stats) override;

  /** Method telling SQP solver that a working set is provided. */
  bool use_initial_working_set() const
  {
    return true;
  }

  /** Method initializing the working set. */
  bool get_initial_working_sets(int num_variables,
                                ActivityStatus* bounds_working_set,
                                int num_constraints,
                                ActivityStatus* constraints_working_set);

  /** Add new constraints to the set of considered constraints.  */
  bool add_new_constraints(int num_new_considered_constraints,
                           const int* new_considered_constraints_indices);

private:
  /** @name Hide unused default methods. */
  //@{
  /** Default constructor*/
  LazySqpTNlp();

  /** Copy Constructor */
  LazySqpTNlp(const LazySqpTNlp&);

  /** Overloaded Equals Operator */
  void operator=(const LazySqpTNlp&);
  //@}

  /** Original SqpTNlp of which we solve a version with fewer constraints. */
  std::shared_ptr<SqpTNlp> sqp_tnlp_;

  /** Number of variables. */
  int num_variables_;

  /** Number of constraints for the next solve. */
  int num_constraints_;

  /** Indices of the constraints of the original SqpTNlp that should be
   *  considered in the next solve.  The counting starts at 0. */
  int* constraint_indices_;

  /** Number of nonzeros in the shortened Jacobian. */
  int num_nonzeros_jacobian_;

  /** Number of original constraints. */
  int num_orig_constraints_;

  /** NUmber of original nonzero elements. */
  int num_orig_nonzeros_jacobian_;

  /** Map from the nonzeros of the shortened Jacobian to the full Jacobian.
   *
   *  sqp_jac_map[i] is the position of the i-th nonzero of our Jaocian in the
   * full Jacobian. */
  int* sqp_jac_map_;

  /** Final working set for the bounds from the most recent solve. */
  ActivityStatus* bound_activity_status_;

  /** Final working set for the bounds from the most recent solve. */
  ActivityStatus* constraint_activity_status_;

  /** Flag indicating whether the NLP has been solved before. */
  bool has_been_solved_before_;

  /** Values of the optimal primal variables from the most recent solve. */
  double* previous_optimal_solution_;

  /** Values of the optimal bound multipliers from the most recent solve. */
  double* previous_optimal_bound_multipliers_;

  /** Values of the optimal constraint multipliers from the most recent solve.
   *  There are only set of constraints that have been collected. */
  double* previous_optimal_constraint_multipliers_;
};
}

#endif // SQPHOTSTART_ShortenedSqpTNlp_HPP