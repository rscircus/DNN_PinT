#ifndef LIB_H_INCLUDED
#define LIB_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "braid_wrapper.h"
#include "codi.hpp"
using namespace codi;


int
opening_expand(double *Y, 
               double *data, 
               int     nelem, 
               int     nchannels, 
               int     nfeatures);


template <typename myDouble>
int 
opening_layer(myDouble *Y,
              myDouble *theta_open, 
              double   *Ydata, 
              int       nelem, 
              int       nchannels, 
              int       nfeatures);

/** 
 * Forward propagation 
 */
template <typename myDouble>
int
take_step(myDouble *Y,
          myDouble *K,
          int          ts,
          double       dt,
          int          nelem,
          int          nchannels,
          int          parabolic);


/**
 * Activation function 
 */
template <typename myDouble>
myDouble 
sigma(myDouble x);        


/**
 * Derivative of activation function 
 */
double
sigma_diff(double x);


/**
 * Return maximum of a vector of size 'size_t' */
template <typename myDouble>
myDouble 
maximum(myDouble *a,
        int       size_t);

/**
 * Evaluate the loss functin
 * Cross entropy loss for softmax function 
 */
template <typename myDouble>
myDouble  
loss(myDouble    *Y,
     double      *Target,
     double      *Ydata,
     myDouble    *classW,
     myDouble    *classMu,
     int          nelem,
     int          nclasses,
     int          nchannels,
     int          nfeatures,
     int          output,
     int         *success_ptr);


/**
 * Tikhonov regularization 
 */
template <typename myDouble>
myDouble
tikhonov_regul(myDouble *variable,
               int       size);

/**
 * Time-derivative regularization term for theta.
 * 
 */
template <typename myDouble>
myDouble
ddt_theta_regul(myDouble* theta,
                int          ts,
                double       dt,
                int          ntime,
                int          nchannels);


/** 
 * Put all gradient information into one global vector and 
 * invoke an MPI_allreduce to collect information from all processors.  
 */
int
collect_gradient(braid_App  app, 
                 MPI_Comm   comm,
                 double    *gradient);


// /** 
//  * Set the gradient to zero 
//  */
// int
// gradient_norm(braid_App app,
//               double   *theta_gnorm_prt,
//               double   *class_gnorm_prt);


/**
 * Updates design into 'direction' using 'stepsize'
 * output: new design
 */
int 
update_design(int       N, 
              double    stepsize,
              double   *direction,
              double   *design);


/**
 * Compute descent direction for theta: - Hessian * gradient
 * return: Wolfe condition: gradient * descentdir
 */
double
compute_descentdir(int     N,
               double* Hessian,
               double* gradient,        
               double* descent_dir);

/**
 * Copy a vector x of size N into a vector x_copy
 */
int
copy_vector(int N, 
            double* u, 
            double* u_copy);


/**
 * Return the norm of a vector of size 'size_t'
 */
double
vector_norm(int    size_t,
            double *vector);


/**
 * Concatenate the four vectors into a global vector
 */
int
concat_4vectors(int     size1,
                double *vec1,
                int     size2,
                double *vec2,
                int     size3,
                double *vec3,
                int     size4,
                double *vec4,
                double *globalvec);

/**
 * Split a global vector into four seperate vectors
 */
int
split_into_4vectors(double *globalvec,
                    int     size1,
                    double *vec1,
                    int     size2,
                    double *vec2,
                    int     size3,
                    double *vec3,
                    int     size4,
                    double *vec4);

/**
 * Read data from file 
 */
template <typename myDouble> 
int
read_data(char   *filename, 
          myDouble *var, 
          int     size);

/**
 * Write data to file
 */
template <typename myDouble>
int
write_data(char   *filename,
           myDouble *var, 
           int     size);

/**
 * CODI TYPE: Get the primal value 
 */
double 
getValue(RealReverse value);

/**
 * DOUBLE TYPE: Return value 
 */
double 
getValue(double value);


#endif // LIB_H_INCLUDED
