#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

// #include "lib.hpp"
//#include "hessianApprox.hpp"
#include "util.hpp"
#include "layer.hpp"
#include "braid.h"
#include "braid_wrapper.hpp"
#include "parser.h"
#include "network.hpp"

#define MASTER_NODE 0
#define USE_BFGS  1
#define USE_LBFGS 2


int main (int argc, char *argv[])
{
    /* --- Data set --- */
    int      ntraining;               /**< Number of elements in training data */
    int      nvalidation;             /**< Number of elements in validation data */
    int      nfeatures;               /**< Number of features in the data set */
    int      nclasses;                /**< Number of classes / Clabels */
    double **train_examples = NULL;   /**< Traning examples */
    double **train_labels   = NULL;   /**< Training labels*/
    double **val_examples   = NULL;   /**< Validation examples */
    double **val_labels     = NULL;   /**< Validation labels*/
    /* --- Network --- */
    int      nlayers;                  /**< Number of layers / time steps */
    int      nchannels;               /**< Number of channels of the netword (width) */
    double   T;                       /**< Final time */
    int      activation;              /**< Enumerator for the activation function */
    Network *network;                 /**< DNN Network architecture */
    double   loss;
    double   accuracy;
    /* --- Optimization --- */
    int      ndesign;             /**< Number of design variables */
    double  *design;              /**< Pointer to global design vector */
    double  *gradient;            /**< Pointer to global gradient vector */
    double   objective;           /**< Optimization objective */
    double   regul;               /**< Value of the regularization term */
    double   gamma_tik;           /**< Parameter for Tikhonov regularization of the weights and bias*/
    double   gamma_ddt;           /**< Parameter for time-derivative regularization of the weights and bias */
    double   weights_open_init;   /**< Factor for scaling initial opening layer weights and biases */
    double   weights_init;        /**< Factor for scaling initial weights and bias of intermediate layers*/
    double   weights_class_init;  /**< Factor for scaling initial classification weights and biases */
    double   stepsize_init;       /**< Initial stepsize for design updates */
    int      maxoptimiter;        /**< Maximum number of optimization iterations */
    double   gtol;                /**< Stopping Tolerance on norm of gradient */
    int      ls_maxiter;          /**< Max. number of linesearch iterations */
    double   ls_factor;           /**< Reduction factor for linesearch */
    int      hessian_approx;      /**< Hessian approximation (USE_BFGS or L-BFGS) */
    int      lbfgs_stages;        /**< Number of stages of L-bfgs method */
    /* --- PinT --- */
    braid_Core core_train;      /**< Braid core for training data */
    my_App  *app_train;         /**< Braid app for training data */
    int      myid;              /**< Processor rank */
    int      size;              /**< Number of processors */
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


    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    double StartTime = MPI_Wtime();

    

    /* --- Set DEFAULT parameters of the config file options --- */ 

    ntraining          = 5000;
    nvalidation        = 200;
    nfeatures          = 2;
    nclasses           = 5;
    nchannels          = 8;
    nlayers            = 32;
    T                  = 10.0;
    activation         = Network::RELU;
    braid_cfactor      = 4;
    braid_maxlevels    = 10;
    braid_maxiter      = 3;
    braid_abstol       = 1e-10;
    braid_abstoladj    = 1e-06;
    braid_printlevel   = 1;
    braid_accesslevel  = 0;
    braid_setskip      = 0;
    braid_fmg          = 0;
    braid_nrelax       = 1;
    gamma_tik          = 1e-07;
    gamma_ddt          = 1e-07;
    stepsize_init      = 1.0;
    maxoptimiter       = 500;
    gtol               = 1e-08;
    ls_maxiter         = 20;
    ls_factor          = 0.5;
    weights_open_init  = 0.001;
    weights_init       = 0.0;
    weights_class_init = 0.001;
    hessian_approx     = USE_LBFGS;
    lbfgs_stages       = 20;


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
        else if ( strcmp(co->key, "gamma_tik") == 0 )
        {
            gamma_tik = atof(co->value);
        }
        else if ( strcmp(co->key, "gamma_ddt") == 0 )
        {
            gamma_ddt = atof(co->value);
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
        else if ( strcmp(co->key, "weights_open_init") == 0 )
        {
           weights_open_init = atof(co->value);
        }
        else if ( strcmp(co->key, "weights_init") == 0 )
        {
           weights_init = atof(co->value);
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

    /* Set the data file names */
    char train_ex_filename[255];
    char train_lab_filename[255];
    char val_ex_filename[255];
    char val_lab_filename[255];
    sprintf(train_ex_filename,  "data/%s.dat", "Ytrain_orig");
    sprintf(train_lab_filename, "data/%s.dat", "Ctrain_orig");
    sprintf(val_ex_filename,    "data/%s.dat", "Yval_orig");
    sprintf(val_lab_filename,   "data/%s.dat", "Cval_orig");

    /* Read training data */
    train_examples = new double* [ntraining];
    train_labels   = new double* [ntraining];
    for (int ix = 0; ix<ntraining; ix++)
    {
        if (myid == MASTER_NODE) train_examples[ix] = new double[nfeatures];
        if (myid == size-1)      train_labels[ix]   = new double[nclasses];
    }
    if (myid == MASTER_NODE) read_data(train_ex_filename,  train_examples, ntraining, nfeatures);
    if (myid == size-1)      read_data(train_lab_filename, train_labels,   ntraining, nclasses);

    /* Read validation data */
    val_examples = new double* [nvalidation];
    val_labels   = new double* [nvalidation];
    for (int ix = 0; ix<nvalidation; ix++)
    {
        if (myid == MASTER_NODE) val_examples[ix] = new double[nfeatures];
        if (myid == size-1)      val_labels[ix]   = new double[nclasses];
    }
    if (myid == MASTER_NODE) read_data(val_ex_filename,  val_examples, nvalidation, nfeatures);
    if (myid == size - 1)    read_data(val_lab_filename, val_labels,   nvalidation, nclasses);


    /* Create the network */
    network = new Network(nlayers, nchannels, nfeatures, nclasses, activation, T/(double)nlayers, weights_init, weights_open_init, weights_class_init);

    /* Get pointer to gradient and design */
    ndesign  = network->getnDesign();
    design   = network->getDesign();
    gradient = network->getGradient();


    /* Initialize xbraid's app structure */
    app_train = (my_App *) malloc(sizeof(my_App));
    app_train->myid      = myid;
    app_train->network   = network;
    app_train->nexamples  = ntraining;
    app_train->examples  = train_examples;
    app_train->labels    = train_labels;
    app_train->accuracy  = 0.0;
    app_train->loss      = 0.0;
    app_train->gamma_tik = gamma_tik;
    app_train->gamma_ddt = gamma_ddt;

    /* Initializze adjoint XBraid for training data */
    braid_Init(MPI_COMM_WORLD, MPI_COMM_WORLD, 0.0, T, nlayers, app_train, my_Step, my_Init, my_Clone, my_Free, my_Sum, my_SpatialNorm, my_Access, my_BufSize, my_BufPack, my_BufUnpack, &core_train);
    braid_InitAdjoint( my_ObjectiveT, my_ObjectiveT_diff, my_Step_diff,  my_ResetGradient, &core_train);

    /* Set Braid parameters */
    braid_SetMaxLevels(core_train, braid_maxlevels);
    braid_SetPrintLevel( core_train, braid_printlevel);
    braid_SetCFactor(core_train, -1, braid_cfactor);
    braid_SetAccessLevel(core_train, braid_accesslevel);
    braid_SetMaxIter(core_train, braid_maxiter);
    braid_SetSkip(core_train, braid_setskip);
    if (braid_fmg){
        braid_SetFMG(core_train);
    }
    braid_SetNRelax(core_train, -1, braid_nrelax);
    braid_SetAbsTol(core_train, braid_abstol);
    braid_SetAbsTolAdjoint(core_train, braid_abstoladj);



    /* Propagate training data */
    network->applyFWD(ntraining, train_examples, train_labels);

    /* Evaluate objective function */
    loss      = network->getLoss();
    regul     = network->evalRegularization(gamma_tik, gamma_ddt);
    objective = loss + regul;
    printf("Objective0: %1.16e %1.16e %1.16e\n",   objective, loss, regul);
    
    /* Pertub the design */
    double EPS = 1e-6;
    design[43] += EPS;

    /* Evaluate objective function */
    network->applyFWD(ntraining, train_examples, train_labels);
    loss      = network->getLoss();
    regul     = network->evalRegularization(gamma_tik, gamma_ddt);
    double objective1 = loss + regul;

    printf("Objective1: %1.16e %1.16e %1.16e\n",   objective1, loss, regul);

    double diff = (objective1 - objective);
    double fd   = diff / EPS;
    printf("\nDiff: %1.14e\n", diff);
    printf("\nFD:   %1.14e\n",   fd);

//    /* Propagate through braid */
//     // braid_SetObjectiveOnly(core_train, 1);
//     braid_Drive(core_train);
//     braid_GetObjective(core_train, &objective);

//     /* TODO: MPIAllreduce loss and accuracy! or not. */
//     /* TODO: Reduce gradient data (in place on master_node) */

//     write_data("gradient.dat", gradient, ndesign);

//     /* Output */
//     printf("Objective1: %1.16e %1.16e \n",   objective, app_train->loss);



    /* Clean up */
    delete network;
    braid_Destroy(core_train);
    free(app_train);

    for (int ix = 0; ix<ntraining; ix++)
    {
        if (myid == MASTER_NODE) delete [] train_examples[ix];
        if (myid == size-1)      delete [] train_labels[ix];
    }
    delete [] train_examples;
    delete [] train_labels;
    for (int ix = 0; ix<nvalidation; ix++)
    {
        if (myid == MASTER_NODE) delete [] val_examples[ix];
        if (myid == size-1)      delete [] val_labels[ix];
    }
    delete [] val_examples;
    delete [] val_labels;



    MPI_Finalize();
    return 0;
}
