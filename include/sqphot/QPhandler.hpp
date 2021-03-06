#ifndef SQPHOTSTART_QPHANDLER_HPP_
#define SQPHOTSTART_QPHANDLER_HPP_


#include <sqphot/Options.hpp>
#include <sqphot/SQPTNLP.hpp>
#include <sqphot/Stats.hpp>
#include <sqphot/Utils.hpp>

#include <sqphot/QPsolverInterface.hpp>
#include <sqphot/qpOASESInterface.hpp>
#include <sqphot/GurobiInterface.hpp>
#include <sqphot/QOREInterface.hpp>
#include <sqphot/CplexInterface.hpp>

namespace SQPhotstart {
/** Forward Declaration */

/**
 *
 * This is a class for setting up and solving the SQP
 * subproblems for Algorithm::Optimize
 *
 * It contains the methods to setup the QP problem in
 * the following format or similar ones,
 *
 * 	minimize  1/2 p_k^T H_k p^k + g_k^T p_k+rho*e^T *(u+v)
 * 	subject to c_l<=c_k+J_k p+u-v<=c_u,
 * 		       x_l<=x_k+p_k<=x_u,
 *		       -delta<=p_i<=delta,
 *		       u,v>=0
 *
 *
 * and transform them into sparse matrix (triplet) and
 * dense vectors' format which can be taken as input
 * of standard QPhandler.
 *
 * It also contains a method which interfaces to the
 * QP solvers that users choose. The interface will pre
 * -process the data required by individual solver.
 */


class QPhandler {

    ///////////////////////////////////////////////////////////
    //                      PUBLIC METHODS                   //
    ///////////////////////////////////////////////////////////
public:


    QPhandler(NLPInfo nlp_info, QPType qptype, Ipopt::SmartPtr<Ipopt::Journalist> jnlst,
              shared_ptr<const Options> options);

    /** Default destructor */
    virtual ~QPhandler();

    /**
     * @brief solve the QP subproblem according to the bounds setup before,
     * assuming the first QP subproblem has been solved.
     * */

    void solveQP(shared_ptr<SQPhotstart::Stats> stats, shared_ptr<Options> options);


    void solveLP(shared_ptr<SQPhotstart::Stats> stats) {
        solverInterface_->optimizeLP(stats);
    }
    /** @name Getters */
    //@{
    /**
     * @brief Get the optimal solution from the QPsolverinterface
     *
     * This is only an interface for user to avoid call interface directly.
     * @param p_k 	the pointer to an empty array with the length equal to the size
     * of the QP subproblem
     */
    double* get_optimal_solution();


    /**
     * @brief Get the infeasibility measure of the quadratic model
     * @return The one norm of the last (2*nConstr) varaibles of the QP solution
     */
    double get_infea_measure_model();
    /**
     *@brief Get the multipliers corresponding to the bound variables
     */
    double* get_multipliers_bounds();

    /**
     * @brief Get the multipliers corresponding to the constraints
     */
    double* get_multipliers_constr();

    /**
     * @brief Get the objective value of the QP
     */
    double get_objective();


    /**
     * @brief manually calculate the active set from the class member solverInterface
     * @param A_c  pointer to the active set corresponding to the constraints
     * @param A_b  pointer to the active set corresponding to the bound constraints
     * @param x    solution for QP problem(optional)
     * @param Ax   constraint evaluation at current QP solution x(optional)
     */
    void get_active_set(ActiveType* A_c, ActiveType* A_b,
                        shared_ptr<Vector> x = nullptr,
                        shared_ptr<Vector> Ax = nullptr);



    /**
     * @brief Get the return status of QPsolver
     */
    Exitflag get_status();



    //@}

    /** @name Setters*/
    //@{
    /**
    *
    * @brief setup the bounds for the QP subproblems
    * according to the information from current iterate
    *
    * @param delta      trust region radius
    * @param x_k 	     current iterate point
    * @param c_k        current constraint value evaluated at x_k
    * @param x_l        the lower bounds for variables
    * @param x_u        the upper bounds for variables
    * @param c_l        the lower bounds for constraints
    * @param c_u        the upper bounds for constraints
    */
    void set_bounds(double delta, shared_ptr<const Vector> x_l,
                    shared_ptr<const Vector> x_u, shared_ptr<const Vector> x_k,
                    shared_ptr<const Vector> c_l, shared_ptr<const Vector> c_u,
                    shared_ptr<const Vector> c_k);


    /**
     * @brief This function sets up the object vector g
     * of the QP problem
     *
     * @param grad 	Gradient vector from nlp class
     * @param rho  	Penalty Parameter
     */
    void set_g(shared_ptr<const Vector> grad, double rho);


    void set_g(double rho);

    /**
     * Set up the H for the first time in the QP
     * problem.
     * It will be concatenated as [H_k 0]
     * 			                  [0   0]
     * where H_k is the Lagragian hessian evaluated at x_k
     * and lambda_k.
     *
     * This method should only be called for once.
     *
     * @param hessian the Lagragian hessian evaluated
     * at x_k and lambda_k from nlp readers.
     * @return
     */

    void set_H(shared_ptr<const SpTripletMat> hessian);


    /** @brief setup the matrix A for the QP subproblems according to the
     * information from current iterate*/
    void set_A(shared_ptr<const SpTripletMat> jacobian);

    //@}

    /** @name Update QPdata */

//@{
    void update_delta(double delta,
                      shared_ptr<const Vector> x_l,
                      shared_ptr<const Vector> x_u,
                      shared_ptr<const Vector> x_k);


    /**
     * @brief This function updates the bounds on x if there is any changes to the
     * values of trust-region or the iterate
     */
    virtual void update_bounds(double delta, shared_ptr<const Vector> x_l,
                               shared_ptr<const Vector> x_u,
                               shared_ptr<const Vector> x_k,
                               shared_ptr<const Vector> c_l,
                               shared_ptr<const Vector> c_u,
                               shared_ptr<const Vector> c_k);


    /**
     * @brief This function updates the vector g in the QP subproblem when there
     * are any change to the values of penalty parameter
     *
     * @param rho		penalty parameter
     */
    virtual void update_penalty(double rho);


    /**
     * @brief This function updates the vector g in the QP subproblem when there
     * are any change to the values of gradient in NLP
     *
     * @param grad		the gradient vector from NLP
     */
    void update_grad(shared_ptr<const Vector> grad);


    /*  @brief Update the SparseMatrix H of the QP
     *  problems when there is any change to the
     *  true function Hessian
     *
     *  */
    void update_H(shared_ptr<const SpTripletMat> Hessian);


    /**
     * @brief Update the Matrix H of the QP problems
     * when there is any change to the Jacobian to the constraints.
     */
    void update_A(shared_ptr<const SpTripletMat> Jacobian);

//@}

    /**
     * @brief Write QP data to a file
     */
    void WriteQPData(const string filename);

    /**
     * @brief Test the KKT conditions for the certain qpsolver
     */
    bool test_optimality(
        shared_ptr<QPSolverInterface> qpsolverInterface,
        Solver qpSolver,
        ActiveType* W_b,
        ActiveType* W_c);

    const OptimalityStatus &get_QpOptimalStatus() const;

#if DEBUG
#if COMPARE_QP_SOLVER


    void set_bounds_debug(double delta, shared_ptr<const Vector> x_k,
                          shared_ptr<const Vector> x_l,
                          shared_ptr<const Vector> x_u,
                          shared_ptr<const Vector> c_k,
                          shared_ptr<const Vector> c_l,
                          shared_ptr<const Vector> c_u);

    bool testQPsolverDifference();


#endif
#endif

    ///////////////////////////////////////////////////////////
    //                      PRIVATE METHODS                  //
    //////////////////////////////////////////////////////////


private:
    //@{

    /** Default constructor */
    QPhandler();


    /** Copy Constructor */
    QPhandler(const QPhandler&);


    /** Overloaded Equals Operator */
    void operator=(const QPhandler&);
    //@}




    /**public class member*/

    /** QP problem will be in the following form
     * min 1/2x^T H x+ g^Tx
     * s.t. lbA <= A x <= ubA,
     *      lb  <=   x <= ub.
     *
     */

    ///////////////////////////////////////////////////////////
    //                      PRIVATE MEMBERS                  //
    ///////////////////////////////////////////////////////////

protected:
    shared_ptr<QPSolverInterface> solverInterface_; /**<an interface to the standard
                                                              QP solver specified by the user*/

    IdentityInfo I_info_A_;

private:
    Solver QPsolverChoice_;
    //bounds that can be represented as vectors
    const NLPInfo nlp_info_;
    int nConstr_QP_;
    int nVar_QP_;
    ActiveType* W_c_;//working set for constraints;
    ActiveType* W_b_;//working set for bounds;

    Ipopt::SmartPtr<Ipopt::Journalist> jnlst_;
    OptimalityStatus qpOptimalStatus_;

#if DEBUG
#if COMPARE_QP_SOLVER
    shared_ptr<qpOASESInterface> qpOASESInterface_;
    shared_ptr<QOREInterface> QOREInterface_;
    ActiveType* W_c_qpOASES_;//working set for constraints;
    ActiveType* W_b_qpOASES_;//working set for bounds;
    ActiveType* W_c_qore_;//working set for constraints;
    ActiveType* W_b_qore_;//working set for bounds;
#endif
#endif

};


} // namespace SQPhotstart
#endif //SQPHOTSTART_QPHANDLER_HPP_
