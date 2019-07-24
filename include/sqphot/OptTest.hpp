/* Copyright (C) 2019
* All Rights Reserved.
*
* Authors: Xinyi Luo
* Date:2019-07-23
*/
#include <sqphot/Utils.hpp>
#include <sqphot/Types.hpp>
#include <coin/IpTNLP.hpp>
#include "Vector.hpp"
#include "Options.hpp"

#ifndef _SQPHOTSTART_OPTTEST_HPP
#define _SQPHOTSTART_OPTTEST_HPP

namespace SQPhotstart {

    /**
     * @brief This is a pure virtual base class for Optimality Test
     * */
    class OptTest {

    public:
        /** Default constructor*/
        OptTest() = default;

        /**@brief Default Destructor*/
        virtual ~OptTest() = default;

        /**
         * @brief Test the KKT conditions
         */
        virtual bool Check_KKTConditions(bool isPointChanged,
                                         bool isConstraintChanged) { return false; }

        /**
         * @brief Test the Second-order optimality conditions
         */
        virtual bool Check_SecondOrder() { return false; }

        /**
         * @brief Check the Feasibility conditions;
         */
        virtual bool Check_Feasibility(double infea_measure) { return false; }

        /**
         * @brief Check the sign of the multipliers
         */
        virtual bool Check_Dual_Feasibility() { return false; }

        /**
         * @brief Check the complementarity condition
         *
         */
        virtual bool Check_Complementarity() { return false; }

        /**
         * @brief Check the Stationarity condition
         */
        virtual bool Check_Stationarity() { return false; }

        /** Public class members*/
    public:
        bool primal_feasibility_ = false;
        bool dual_feasibility_ = false;
        bool complementarity_ = false;
        bool Stationarity_ = false;
        bool Second_order_opt_ = false;

    private:

        //@{
        /** Copy Constructor */
        OptTest(const OptTest&);

        /** Overloaded Equals Operator */
        void operator=(const OptTest&);
        //@}

    };

    /**
     * @brief Derived class for OptTest. Test the optimality condition for a given NLP
     * problem
     */
    class NLP_OptTest : public OptTest {
    public:


        NLP_OptTest(shared_ptr<const Vector> x_k, shared_ptr<const Vector> x_u,
                    shared_ptr<const Vector> x_l, shared_ptr<const Vector> c_k,
                    shared_ptr<const Vector> c_u, shared_ptr<const Vector> c_l,
                    shared_ptr<SQPhotstart::Options> options,
                    const ConstraintType* cons_type,
                    const ConstraintType* bound_cons_type);

        NLP_OptTest() = default;

        ~NLP_OptTest();

        bool IdentifyActiveSet();;

        /**
         * @brief Test the KKT conditions
         */
        bool Check_KKTConditions(bool isPointChanged = false,
                                 bool isConstraintChanged = false) override;

        /**
         * @brief Test the Second-order optimality conditions
         */
        bool Check_SecondOrder() override;

        /**
         * @brief Check the Feasibility conditions;
         */

        bool Check_Feasibility(double infea_measure = 0);

        /**
         * @brief Check the sign of the multipliers
         */
        bool Check_Dual_Feasibility() override;


        bool
        Check_Dual_Feasibility(const double* multiplier, ConstraintType nlp_cons_type);

        /**
         * @brief Check the complementarity condition
         *
         */
        bool Check_Complementarity() override;

        /**
         * @brief Check the Stationarity condition
         */
        bool Check_Stationarity() override;

        /** Setter/Getters*/
        //@{
        void setActiveSetConstraints(int* activeSetConstraints);

        void setActiveSetBounds(int* activeSetBounds);

        int* getActiveSetConstraints() const;

        int* getActiveSetBounds() const;
        //@}
        /** Public class members*/
    public:
        bool primal_feasibility_ = false;
        bool dual_feasibility_ = false;
        bool complementarity_ = false;
        bool stationarity_ = false;
        bool second_order_opt_ = false;


    private:
//        /** Copy Constructor */
//        NLP_OptTest(const NLP_OptTest&);

        /** Overloaded Equals Operator */
        void operator=(const NLP_OptTest&);
        //@}
    private:
        shared_ptr<const Vector> c_u_;
        shared_ptr<const Vector> c_l_;
        shared_ptr<const Vector> x_k_;
        shared_ptr<const Vector> x_u_;
        shared_ptr<const Vector> x_l_;
        shared_ptr<const Vector> c_k_;
        int* Active_Set_bounds_;
        int* Active_Set_constraints_;
        const ConstraintType* cons_type_;
        const ConstraintType* bound_cons_type_;
        double opt_tol_;
        double opt_compl_tol_;
        double opt_dual_fea_tol_;
        double opt_prim_fea_tol_;
        double opt_second_tol_;
        double active_set_tol_;
        int nVar_; /**< number of variables of NLP*/
        int nCon_; /**< number of constraints of NLP*/

    };


    /**
     * @brief Derived class for OptTest. Test the optimality condition for a given QP
     * problem
     */
    class QP_OptTest : public OptTest {
    public:
        QP_OptTest() = default;

        virtual ~QP_OptTest() = default;

        /**
         * @brief Test the KKT conditions
         */
        bool Check_KKTConditions(bool isPointChanged, bool isConstraintChanged);

        /**
         * @brief Test the Second-order optimality conditions
         */
        bool Check_SecondOrder();

        /**
         * @brief Check the Feasibility conditions;
         */
        bool Check_Feasibility();

        /**
         * @brief Check the sign of the multipliers
         */
        bool Check_Dual_Feasibility();

        /**
         * @brief Check the complementarity condition
         *
         */
        bool Check_Complementarity();

        /**
         * @brief Check the Stationarity condition
         */
        bool Check_Stationarity();


        /** Public class members*/
    public:
        bool primal_feasibility_ = false;
        bool dual_feasibility_ = false;
        bool complementarity_ = false;
        bool stationarity_ = false;
        bool second_order_opt_ = false;


    private:

        //@{
        /** Copy Constructor */
        QP_OptTest(const QP_OptTest&);

        /** Overloaded Equals Operator */
        void operator=(const QP_OptTest&);
        //@}
    };
}

#endif //SQPHOTSTART_OPTTEST_HPP
