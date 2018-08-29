#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "linalg.hpp"


#pragma once

/**
 * Abstract base class for the network layers 
 * Subclasses implement
 *    - applyFWD: Forward propagation of data 
 *    - applyBWD: Backward propagation of data 
 */
class Layer 
{
   protected:
      int dim_In;                          /* Dimension of incoming data */
      int dim_Out;                         /* Dimension of outgoing data */
      int dim_Bias;                        /* Dimension of the bias vector */

      int     index;                       /* Number of the layer */
      double  dt;                          /* Step size for Layer update */
      double* weights;                     /* Weight matrix, flattened as a vector */
      double* weights_bar;                 /* Derivative of the Weight matrix*/
      double* bias;                        /* Bias */
      double* bias_bar;                    /* Derivative of bias */
      double  (*activation)(double x);     /* the activation function */
      double  (*dactivation)(double x);    /* derivative of activation function */

   public:
      Layer();
      Layer(int     idx,
            int     dimI,
            int     dimO,
            int     dimB,
            double  deltaT,
            double (*Activ)(double x),
            double (*dActiv)(double x));     
      ~Layer();

      /* Set time step size */
      void setDt(double DT);

      /* Get a pointer to the weights bias*/
      double* getWeights();
      double* getBias();

      /* Get a pointer to the weights bias bar */
      double* getWeightsBar();
      double* getBiasBar();

      /* Get the dimensions */
      int getDimIn();
      int getDimOut();
      int getDimBias();

      /* Prints to screen */
      void print_data(double* data_Out);

      /**
       * Initialize the layer:
       * sets initial weights and bias randomly, scaled by a factor 
       * sets the bar variables to zero
       */
      void initialize(double factor);

      /**
       * Sets the bar variables to zero 
       */
      void resetBar();

      /**
       * Evaluate Tikhonov Regularization
       * Returns 1/2 * \|weights||^2 + 1/2 * \|bias\|^2
       */
      double evalTikh();

      /**
       * Derivative of Tikhonov Regularization
       */
      void evalTikh_diff(double regul_bar);

      /**
       * Forward propagation of an example 
       * In/Out: vector holding the current example data
       */
      virtual void applyFWD(double* data_In, 
                            double* data_Out) = 0;


      /**
       * Backward propagation of an example 
       * In:     data     - current example data
       * In/Out: data_bar - adjoint example data that is to be propagated backwards 
       */
      virtual void applyBWD(double* data_In,
                            double* data_In_bar,
                            double* data_Out,
                            double* data_Out_bar)=0;

      /**
       * Evaluates the loss function 
       */
      virtual double evalLoss(double *data_Out,
                              double *label);


      /** 
       * Derivative of evaluating the loss function 
       * In: data_Out, can be used to recompute intermediate states
       *     label for the current example 
       *     loss_bar: value holding the outer derivative
       * Out: data_Out_bar holding the derivative of loss wrt data_Out
       */
      virtual void evalLoss_diff(double *data_Out, 
                                 double *data_Out_bar,
                                 double *label,
                                 double  loss_bar);

      /**
       * Compute class probabilities and return predicted class id.
       */
      virtual int prediction(double* data_Out);

};

/**
 * Layer consisting of dense weight matrix K \in R^{nxn}
 * Linear transformation is a matrix multiplication plus 1D bias: Ky + bias
 */
class DenseLayer : public Layer {

  public:
      DenseLayer(int     idx,
                 int     dimI,
                 int     dimO,
                 double  deltaT,
                 double (*Activ)(double x),
                 double (*dActiv)(double x));     
      ~DenseLayer();

      void applyFWD(double* data_In, 
                    double* data_Out);

      void applyBWD(double* data_In,
                    double* data_In_bar,
                    double* data_Out,
                    double* data_Out_bar);
};



/*
 * Opening layer that expands the data by zeros
 */
class OpenExpandZero : public Layer 
{
      public:
            OpenExpandZero(int     dimI,
                           int     dimO,
                           double  deltaT);
            ~OpenExpandZero();

            void applyFWD(double* data_In, 
                          double* data_Out);
      
            void applyBWD(double* data_In,
                          double* data_In_bar,
                          double* data_Out,
                          double* data_Out_bar);
};


/**
 * Classification layer
 */
class ClassificationLayer : public Layer
{
      protected: 
            double* probability;          /* vector of pedicted class probabilities */
      
      public:
            ClassificationLayer(int idx,
                                int dimI,
                                int dimO,
                                double  deltaT);
            ~ClassificationLayer();

            void applyFWD(double* data_In, 
                          double* data_Out);
      
            void applyBWD(double* data_In,
                          double* data_In_bar,
                          double* data_Out,
                          double* data_Out_bar);

            /**
             * Evaluate the loss function 
             */
            double evalLoss(double *finalstate,
                            double *label);

            /** 
             * Algorithmic derivative of evaluating loss
             */
            void evalLoss_diff(double *data_Out, 
                               double *data_Out_bar,
                               double *label,
                               double  loss_bar);

            /**
             * Compute the class probabilities
             * Returns index of predicted class (is index with  max probability)
             */
            int prediction(double* data);

            /**
             * Translate the data: 
             * Substracts the maximum value from all entries
             */
            void normalize(double* data);

            /**
             * Algorithmic derivative of the normalize funciton 
             */ 
            void normalize_diff(double* data, 
                                double* data_bar);
};