#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "lib.hpp"
//#include "hessianApprox.hpp"
#include "layer.hpp"
//#include "braid.h"
//#include "braid_wrapper.hpp"
#include "parser.h"
#include "network.hpp"

#define MASTER_NODE 0
#define USE_BFGS  1
#define USE_LBFGS 2


int main (int argc, char *argv[])
{
    double  *Ytrain = NULL;    /**< Traning data set */
    double  *Ctrain = NULL;    /**< Classes of the training data set */
    double  *Yval = NULL;      /**< Validation data set */
    double  *Cval = NULL;      /**< Classes of the validation data set */
    double   theta_init;       /**< Factor to scale the initial theta weights and biases */
    double   theta_open_init;  /**< Factor to scale the initial opening layer weights and biases */
    double   weights_class_init;       /**< Factor to scale the initial classification weights and biases */
    double   gamma_theta_tik;  /**< Relaxation parameter for theta tikhonov */
    double   gamma_theta_ddt;  /**< Relaxation parameter for theta time-derivative */
    double   gamma_class;       /**< Relaxation parameter for the classification weights and bias */
    int      nclasses;          /**< Number of classes / Clabels */
    int      ntraining;         /**< Number of examples in the training data */
    int      nvalidation;       /**< Number of examples in the validation data */
    int      nfeatures;         /**< Number of features in the data set */
    int      nlayers;            /**< Number of layers / time steps */
    int      nchannels;         /**< Number of channels of the netword (width) */
    double   T;                 /**< Final time */
    int      myid;              /**< Processor rank */
    int      size;              /**< Number of processors */
    double   stepsize_init;     /**< Initial stepsize for theta updates */
    int      maxoptimiter;      /**< Maximum number of optimization iterations */
    double   gtol;              /**< Stopping Tolerance on norm of gradient */
    int      ls_maxiter;        /**< Max. number of linesearch iterations */
    double   ls_factor;         /**< Reduction factor for linesearch */
    int      hessian_approx;     /**< Hessian approximation (USE_BFGS or L-BFGS) */
    int      lbfgs_stages;       /**< Number of stages of the L-bfgs method */
    int      braid_maxlevels;   /**< max. levels of temporal refinement */
    int      braid_printlevel;  /**< print level of xbraid */
    int      braid_cfactor;     /**< temporal coarsening factor */
    int      braid_accesslevel; /**< braid access level */
    int      braid_maxiter;     /**< max. iterations of xbraid */ 
    int      braid_setskip;     /**< braid: skip work on first level */
    int      braid_fmg;         /**< braid: V-cycle or full multigrid */
    int      braid_nrelax;      /**< braid: number of CF relaxation sweeps */
    double   braid_abstol;      /**< tolerance for primal braid */
    double   braid_abstoladj;   /**< tolerance for adjoint braid */
    int      apply_openlayer;   /**< Flag: apply opening layer (1) or just expand data with zero (0) */
    int      activation;        /**< Determin the activation function */
    Network *network;           /**< DNN Network architecture */

    char     Ytrain_file[255];
    char     Ctrain_file[255];
    char     Yval_file[255];
    char     Cval_file[255];
    double StartTime;

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    StartTime = MPI_Wtime();

    /* Input data file names */
    sprintf(Ytrain_file, "data/%s.dat", "Ytrain_orig");
    sprintf(Ctrain_file, "data/%s.dat", "Ctrain_orig");
    sprintf(Yval_file,   "data/%s.dat", "Yval_orig");
    sprintf(Cval_file,   "data/%s.dat", "Cval_orig");
    

    /* --- Set DEFAULT parameters for the config option --- */ 

    ntraining         = 5000;
    nvalidation       = 200;
    nfeatures         = 2;
    nclasses          = 5;
    nchannels         = 8;
    nlayers           = 32;
    T                 = 10.0;
    activation        = Network::RELU;
    apply_openlayer   = 1;
    braid_cfactor     = 4;
    braid_maxlevels   = 10;
    braid_maxiter     = 3;
    braid_abstol      = 1e-10;
    braid_abstoladj   = 1e-06;
    braid_printlevel  = 1;
    braid_accesslevel = 0;
    braid_setskip     = 0;
    braid_fmg         = 0;
    braid_nrelax      = 1;
    gamma_theta_tik   = 1e-07;
    gamma_theta_ddt   = 1e-07;
    gamma_class       = 1e-05;
    stepsize_init     = 1.0;
    maxoptimiter      = 500;
    gtol              = 1e-08;
    ls_maxiter        = 20;
    ls_factor         = 0.5;
    theta_open_init   = 0.001;
    theta_init        = 0.0;
    weights_class_init        = 0.001;
    hessian_approx    = USE_LBFGS;
    lbfgs_stages      = 20;


    /* --- Read the config file (overwrite default values) --- */

    /* Get config filename from command line argument */
    if (argc != 2)
    {
       if ( myid == MASTER_NODE )
       {
          printf("\n");
          printf("USAGE: ./main </path/to/configfile> \n");
       }
       MPI_Finalize();
       return (0);
    }
    /* Parse the config file */
    config_option_t co;
    if ((co = read_config_file(argv[1])) == NULL) {
        perror("read_config_file()");
        return -1;
    }
    while(1) {

        if ( strcmp(co->key, "ntraining") == 0 )
        {
            ntraining = atoi(co->value);
        }
        else if ( strcmp(co->key, "nvalidation") == 0 )
        {
            nvalidation = atoi(co->value);
        }
        else if ( strcmp(co->key, "nfeatures") == 0 )
        {
            nfeatures = atoi(co->value);
        }
        else if ( strcmp(co->key, "nchannels") == 0 )
        {
            nchannels = atoi(co->value);
        }
        else if ( strcmp(co->key, "nclasses") == 0 )
        {
            nclasses = atoi(co->value);
        }
        else if ( strcmp(co->key, "nlayers") == 0 )
        {
            nlayers = atoi(co->value);
        }
        else if ( strcmp(co->key, "activation") == 0 )
        {
            if ( strcmp(co->value, "ReLu") == 0 )
            {
                activation = Network::RELU;
            }
            else if (strcmp(co->value, "tanh") == 0 )
            {
                activation = Network::TANH;
            }
            else
            {
                printf("Invalid activation function!");
                MPI_Finalize();
                return(0);
            }
        }
        else if ( strcmp(co->key, "apply_openlayer") == 0 )
        {
            if ( strcmp(co->value, "YES") == 0 )
            {
                apply_openlayer = 1;
            }
            else
            {
                apply_openlayer = 0;
            }
        }
        else if ( strcmp(co->key, "T") == 0 )
        {
            T = atof(co->value);
        }
        else if ( strcmp(co->key, "braid_cfactor") == 0 )
        {
           braid_cfactor = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_maxlevels") == 0 )
        {
           braid_maxlevels = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_maxiter") == 0 )
        {
           braid_maxiter = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_abstol") == 0 )
        {
           braid_abstol = atof(co->value);
        }
        else if ( strcmp(co->key, "braid_adjtol") == 0 )
        {
           braid_abstoladj = atof(co->value);
        }
        else if ( strcmp(co->key, "braid_printlevel") == 0 )
        {
           braid_printlevel = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_accesslevel") == 0 )
        {
           braid_accesslevel = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_setskip") == 0 )
        {
           braid_setskip = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_fmg") == 0 )
        {
           braid_fmg = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_nrelax") == 0 )
        {
           braid_nrelax = atoi(co->value);
        }
        else if ( strcmp(co->key, "gamma_theta_tik") == 0 )
        {
            gamma_theta_tik = atof(co->value);
        }
        else if ( strcmp(co->key, "gamma_theta_ddt") == 0 )
        {
            gamma_theta_ddt = atof(co->value);
        }
        else if ( strcmp(co->key, "gamma_class") == 0 )
        {
            gamma_class = atof(co->value);
        }
        else if ( strcmp(co->key, "stepsize") == 0 )
        {
            stepsize_init = atof(co->value);
        }
        else if ( strcmp(co->key, "optim_maxiter") == 0 )
        {
           maxoptimiter = atoi(co->value);
        }
        else if ( strcmp(co->key, "gtol") == 0 )
        {
           gtol = atof(co->value);
        }
        else if ( strcmp(co->key, "ls_maxiter") == 0 )
        {
           ls_maxiter = atoi(co->value);
        }
        else if ( strcmp(co->key, "ls_factor") == 0 )
        {
           ls_factor = atof(co->value);
        }
        else if ( strcmp(co->key, "theta_open_init") == 0 )
        {
           theta_open_init = atof(co->value);
        }
        else if ( strcmp(co->key, "theta_init") == 0 )
        {
           theta_init = atof(co->value);
        }
        else if ( strcmp(co->key, "weights_class_init") == 0 )
        {
           weights_class_init = atof(co->value);
        }
        else if ( strcmp(co->key, "hessian_approx") == 0 )
        {
            if ( strcmp(co->value, "BFGS") == 0 )
            {
                hessian_approx = USE_BFGS;
            }
            else if (strcmp(co->value, "L-BFGS") == 0 )
            {
                hessian_approx = USE_LBFGS;
            }
            else
            {
                printf("Invalid Hessian approximation!");
                MPI_Finalize();
                return(0);
            }
        }
        else if ( strcmp(co->key, "lbfgs_stages") == 0 )
        {
           lbfgs_stages = atoi(co->value);
        }
        if (co->prev != NULL) {
            co = co->prev;
        } else {
            break;
        }
    }


    /*--- INITIALIZATION ---*/

    /* Read the training and validation data  */
    if (myid == MASTER_NODE)  // Input data is only needed on first processor 
    {
        Ytrain = new double [ntraining   * nfeatures];
        Yval   = new double [nvalidation * nfeatures];
        read_data(Ytrain_file, Ytrain, ntraining   * nfeatures);
        read_data(Yval_file,   Yval,   nvalidation * nfeatures);
    }
    if (myid == size - 1) // Labels are only needed on last layer 
    {
        Ctrain = new double [ntraining   * nclasses];
        Cval   = new double [nvalidation * nclasses];
        read_data(Ctrain_file, Ctrain, ntraining   * nclasses);
        read_data(Cval_file,   Cval,   nvalidation * nclasses);
    }

    /* Create the network */
    network = new Network(nlayers, nchannels, nfeatures, nclasses, activation, theta_init, theta_open_init, weights_class_init);


    /* Clean up */
    delete network;
    if (myid == MASTER_NODE)
    {
        delete [] Ytrain;
        delete [] Yval;
    }
    if (myid == size -1)
    {
        delete [] Ctrain;
        delete [] Cval;
    }


    return 0;
}
