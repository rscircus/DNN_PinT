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
      int nweights;                        /* Number of weights */
      int ndesign;                         /* Total number of design variables */

      int     index;                       /* Number of the layer */
      double  dt;                          /* Step size for Layer update */
      double* weights;                     /* Weight matrix, flattened as a vector */
      double* weights_bar;                 /* Derivative of the Weight matrix*/
      double* bias;                        /* Bias */
      double* bias_bar;                    /* Derivative of bias */
      double  (*activation)(double x);     /* the activation function */
      double  (*dactivation)(double x);    /* derivative of activation function */
      double  gamma;                       /* Parameter for Tikhonov regularization of weights and bias */

      double *update;                      /* Auxilliary for computing fwd update */
      double *update_bar;                  /* Auxilliary for computing bwd update */

   public:
      Layer();
      Layer(int     idx,
            int     dimI,
            int     dimO,
            int     dimB,
            double  deltaT,
            double (*Activ)(double x),
            double (*dActiv)(double x),
            double  Gamme);

      Layer(int idx, 
             int dimI, 
             int dimO, 
             int dimB);

      virtual ~Layer();

      /* Set time step size */
      void setDt(double DT);

      /* Get pointer to the weights bias*/
      double* getWeights();
      double* getBias();

      /* Get pointer to the weights bias bar */
      double* getWeightsBar();
      double* getBiasBar();

      /* Get the dimensions */
      int getDimIn();
      int getDimOut();
      int getDimBias();
      int getnDesign();

        /* Prints to screen */
      void print_data(double* data_Out);

      /**
       * Initialize the layer primal and adjoint weights and biases
       * In: pointer to the global design and gradient vector memory 
       *     factor for scaling random initialization of primal variables
       */
      void initialize(double* design_ptr,
                      double* gradient_ptr,
                      double  factor);

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
       * In opening layers: set pointer to the current example
       */
      virtual void setExample(double* example_ptr);

      /**
       * Forward propagation of an example 
       * In/Out: vector holding the current propagated example 
       */
      virtual void applyFWD(double* state) = 0;


      /**
       * Backward propagation of an example 
       * In:     data     - current example data
       * In/Out: data_bar - adjoint example data that is to be propagated backwards 
       */
      virtual void applyBWD(double* state,
                            double* state_bar) = 0;

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
       * Compute class probabilities,
       * return 1 if predicted class was correct, else 0.
       */
      virtual int prediction(double* data_Out,
                             double* label);

};

/**
 * Layer using square dense weight matrix K \in R^{nxn}
 * Layer transformation: y = y + dt * sigma(Wy + b)
 * if not openlayer: requires dimI = dimO !
 */
class DenseLayer : public Layer {

  public:
      DenseLayer(int     idx,
                 int     dimI,
                 int     dimO,
                 double  deltaT,
                 double (*Activ)(double x),
                 double (*dActiv)(double x),
                 double  Gamma);     
      ~DenseLayer();

      void applyFWD(double* state);

      void applyBWD(double* state,
                    double* state_bar);
};


/**
 * Opening Layer using dense weight matrix K \in R^{nxn}
 * Layer transformation: y = sigma(W*y_ex + b)  for examples y_ex \in \R^dimI
 */
class OpenDenseLayer : public DenseLayer {

  protected: 
      double* example;    /* Pointer to the current example data */

  public:
      OpenDenseLayer(int     dimI,
                     int     dimO,
                     double (*Activ)(double x),
                     double (*dActiv)(double x),
                     double  Gamma);     
      ~OpenDenseLayer();

      void setExample(double* example_ptr);

      void applyFWD(double* state);

      void applyBWD(double* state,
                    double* state_bar);
};



/*
 * Opening layer that expands the data by zeros
 */
class OpenExpandZero : public Layer 
{
      protected: 
            double* example;    /* Pointer to the current example data */
      public:
            OpenExpandZero(int dimI,
                           int dimO);
            ~OpenExpandZero();

            void setExample(double* example_ptr);
           
            void applyFWD(double* state);
      
            void applyBWD(double* state,
                          double* state_bar);
};


/**
 * Classification layer
 */
class ClassificationLayer : public Layer
{
      protected: 
            double* probability;          /* vector of pedicted class probabilities */
      
      public:
            ClassificationLayer(int    idx,
                                int    dimI,
                                int    dimO,
                                double Gamma);
            ~ClassificationLayer();

            void applyFWD(double* state);
      
            void applyBWD(double* state,
                          double* state_bar);

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
             * return 1 if predicted class was correct, 0 else.
             */
            int prediction(double* data_out, 
                           double* label);

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


/**
 * Layer using a convolution C of size csize X csize, 
 * with nconv total convolutions. 
 * Layer transformation: y = y + dt * sigma(W(C) y + b)
 * if not openlayer: requires dimI = dimO !
 */
class ConvLayer : public Layer {

  protected:
      int csize;
      int nconv;

  public:
      ConvLayer(int     idx,
                int     dimI,
                int     dimO,
                int     csize_in,
                int     nconv_in,
                double  deltaT,
                double (*Activ)(double x),
                double (*dActiv)(double x),
                double  Gamma);
      ~ConvLayer();

      void applyFWD(double* state);

      void applyBWD(double* state,
                    double* state_bar);

      double apply_conv(double* state,        // state vector to apply convolution to 
                      int     i,              // convolution index
                      int     j,              // row index
                      int     k,              // column index
                      int     img_size_sqrt,  // sqrt of the image size
                      bool    transpose);     // apply the tranpose of the kernel

      /** 
       * This method is designed to be used only in the applyBWD. It computes the
       * derivative of the objective with respect to the weights. In particular
       * if you objective is $g$ and your kernel operator has value tau at index
       * a,b then
       *
       *   weights_bar[magic_index] = d_tau [ g] = \sum_{image j,k} tau state_{j+a,k+b} * update_bar_{j,k}
       *
       * Note that we assume that update_bar is 
       *
       *   update_bar = dt * dactivation * state_bar
       *
       * Where state_bar _must_ be at the old time. Note that the adjoint variable
       * state_bar carries withit all the information of the objective derivative.
       *
       * On exit this method modifies weights_bar
       */
      void updateWeightDerivative(
                      double* state,          // state vector
                      double * update_bar,    // combines derivative and adjoint info (see comments)
                      int     i,              // convolution index
                      int     j,              // row index
                      int     k,              // column index
                      int     img_size_sqrt); // sqrt of the image size
};


/**
 * Opening Layer for use with convolutional layers.  Examples are replicated.
 * Layer transformation: y = ([I; I; ... I] y_ex)
 */
class OpenConvLayer : public Layer {

  protected: 
      int   nconv;
      double* example;    /* Pointer to the current example data */

  public:
      OpenConvLayer(int     dimI,
                    int     dimO);
      ~OpenConvLayer();

      void setExample(double* example_ptr);

      void applyFWD(double* state);

      void applyBWD(double* state,
                    double* state_bar);
};

/** 
 * Opening Layer for use with convolutional layers.  Examples are replicated
 * and then have an activation function applied.
 *
 * This layer is specially designed for MNIST
 *
 * Layer transformation: y = sigma([I; I; ... I] y_ex)
 */

class OpenConvLayerMNIST : public OpenConvLayer {

  public:
      OpenConvLayerMNIST(int     dimI,
                         int     dimO);
      ~OpenConvLayerMNIST();

      void applyFWD(double* state);

      void applyBWD(double* state,
                    double* state_bar);
};


