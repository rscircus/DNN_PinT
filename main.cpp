#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

// #include "lib.hpp"
#include "defs.hpp"
#include "hessianApprox.hpp"
#include "util.hpp"
#include "layer.hpp"
#include "braid.h"
#include "_braid.h"
#include "braid_wrapper.hpp"
#include "parser.h"
#include "network.hpp"

#define MASTER_NODE 0
#define USE_BFGS  1
#define USE_LBFGS 2
#define USE_IDENTITY  3


int main (int argc, char *argv[])
{
    /* --- Data set --- */
    int      ntraining;               /**< Number of elements in training data */
    int      nvalidation;             /**< Number of elements in validation data */
    int      nfeatures;               /**< Number of features in the data set */
    int      nclasses;                /**< Number of classes / Clabels */
    MyReal **train_examples = NULL;   /**< Traning examples */
    MyReal **train_labels   = NULL;   /**< Training labels*/
    MyReal **val_examples   = NULL;   /**< Validation examples */
    MyReal **val_labels     = NULL;   /**< Validation labels*/
    /* --- Network --- */
    int      nlayers;                 /**< Total number of layers = nhiddenlayers + 2 */
    int      nhiddenlayers;           /**< Number of hidden layers = number of xbraid steps */
    int      nchannels;               /**< Number of channels of the network (width) */
    MyReal   T;                       /**< Final time */
    int      activation;              /**< Enumerator for the activation function */
    int      networkType;             /**< Use a dense or convolutional network */
    int      type_openlayer;          /**< Type of opening layer for Convolutional layer, 0: replicate, 1: tuned for MNIST */ 
    Network *network;                 /**< DNN Network architecture */
    /* --- Optimization --- */
    int      ndesign_local;             /**< Number of local design variables on this processor */
    int      ndesign_global;      /**< Number of global design variables (sum of local)*/
    MyReal  *design=0;            /**< On root process: Global design vector */
    MyReal  *gradient=0;          /**< On root process: Global gradient vector */
    MyReal  *design0=0;           /**< On root process: Old design at last iteration */
    MyReal  *gradient0=0;         /**< On root process: Old gradient at last iteration*/
    MyReal  *descentdir=0;        /**< On root process: Direction for design updates */
    MyReal   objective;           /**< Optimization objective */
    MyReal   wolfe;               /**< Holding the wolfe condition value */
    MyReal   gamma_tik;           /**< Parameter for Tikhonov regularization of the weights and bias*/
    MyReal   gamma_ddt;           /**< Parameter for time-derivative regularization of the weights and bias */
    MyReal   gamma_class;         /**< Parameter for regularization of classification weights and bias*/
    MyReal   weights_open_init;   /**< Factor for scaling initial opening layer weights and biases */
    MyReal   weights_init;        /**< Factor for scaling initial weights and bias of intermediate layers*/
    MyReal   weights_class_init;  /**< Factor for scaling initial classification weights and biases */
    MyReal   stepsize_init;       /**< Initial stepsize for design updates */
    int      maxoptimiter;        /**< Maximum number of optimization iterations */
    MyReal   rnorm;               /**< Space-time Norm of the state variables */
    MyReal   rnorm_adj;           /**< Space-time norm of the adjoint variables */
    MyReal   gnorm;               /**< Norm of the gradient */
    MyReal   gtol;                /**< Stoping tolerance on the gradient norm */
    int      ls_maxiter;          /**< Max. number of linesearch iterations */
    MyReal   ls_factor;           /**< Reduction factor for linesearch */
    MyReal   ls_param;            /**< Parameter in wolfe condition test */
    int      hessian_approx;      /**< Hessian approximation (USE_BFGS or L-BFGS) */
    int      lbfgs_stages;        /**< Number of stages of L-bfgs method */
    int      validationlevel;     /**< level for determine frequency of validation computations */
    /* --- PinT --- */
    braid_Core core_train;      /**< Braid core for training data */
    braid_Core core_val;        /**< Braid core for validation data */
    braid_Core core_adj;        /**< Braid core for adjoint computation */
    my_App  *app_train;         /**< Braid app for training data */
    my_App  *app_val;           /**< Braid app for validation data */
    int      myid;              /**< Processor rank */
    int      size;              /**< Number of processors */
    int      braid_maxlevels;   /**< max. levels of temporal refinement */
    int      braid_mincoarse;   /**< minimum allowed coarse time grid size */
    int      braid_printlevel;  /**< print level of xbraid */
    int      braid_cfactor;     /**< temporal coarsening factor */
    int      braid_cfactor0;    /**< temporal coarsening factor on level 0 */
    int      braid_accesslevel; /**< braid access level */
    int      braid_maxiter;     /**< max. iterations of xbraid */ 
    int      braid_setskip;     /**< braid: skip work on first level */
    int      braid_fmg;         /**< braid: V-cycle or full multigrid */
    int      braid_nrelax0;     /**< braid: number of CF relaxation sweeps on level 0*/
    int      braid_nrelax;      /**< braid: number of CF relaxation sweeps */
    MyReal   braid_abstol;      /**< tolerance for primal braid */
    MyReal   braid_abstoladj;   /**< tolerance for adjoint braid */

    MyReal accur_train = 0.0;   /**< Accuracy on training data */
    MyReal accur_val   = 0.0;   /**< Accuracy on validation data */
    MyReal loss_train  = 0.0;   /**< Loss function on training data */
    MyReal loss_val    = 0.0;   /**< Loss function on validation data */

    int ilower, iupper;         /**< Index of first and last layer stored on this processor */
    struct rusage r_usage;
    MyReal StartTime, StopTime, myMB, globalMB; 
    MyReal UsedTime = 0.0;
    char  optimfilename[255];
    FILE *optimfile = 0;   
    char* activname;
    char *datafolder, *ftrain_ex, *fval_ex, *ftrain_labels, *fval_labels;
    char *weightsopenfile, *weightsclassificationfile;
    MyReal mygnorm, stepsize, ls_objective;
    int nreq = -1;
    int ls_iter;
    braid_BaseVector ubase;
    braid_Vector     u;

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* --- Set DEFAULT parameters of the config file options --- */ 

    ntraining          = 5000;
    nvalidation        = 200;
    nfeatures          = 2;
    nclasses           = 5;
    nchannels          = 8;
    nlayers            = 32;
    T                  = 10.0;
    activation         = Layer::RELU;
    networkType        = Network::DENSE;
    braid_cfactor      = 4;
    braid_cfactor0     = 4;
    braid_maxlevels    = 10;
    braid_mincoarse    = 10;
    braid_maxiter      = 3;
    braid_abstol       = 1e-10;
    braid_abstoladj    = 1e-06;
    braid_printlevel   = 1;
    braid_accesslevel  = 0;
    braid_setskip      = 0;
    braid_fmg          = 0;
    braid_nrelax0      = 1;
    braid_nrelax       = 1;
    gamma_tik          = 1e-07;
    gamma_ddt          = 1e-07;
    gamma_class        = 1e-07;
    stepsize_init      = 1.0;
    maxoptimiter       = 500;
    gtol               = 1e-08;
    ls_maxiter         = 20;
    ls_factor          = 0.5;
    weights_open_init  = 0.001;
    weights_init       = 0.0;
    weights_class_init = 0.001;
    type_openlayer     = 0;
    hessian_approx     = USE_LBFGS;
    lbfgs_stages       = 20;
    validationlevel    = 1;
    datafolder         = "NONE";
    ftrain_ex          = "NONE";
    fval_ex            = "NONE";
    ftrain_labels      = "NONE";
    fval_labels        = "NONE";
    weightsopenfile    = "NONE";
    weightsclassificationfile = "NONE";


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

        if ( strcmp(co->key, "datafolder") == 0 )
        {
            datafolder = co->value;
        }
        else if ( strcmp(co->key, "ftrain_ex") == 0 )
        {
            ftrain_ex = co->value;
        }
        else if ( strcmp(co->key, "ftrain_labels") == 0 )
        {
            ftrain_labels = co->value;
        }
        else if ( strcmp(co->key, "fval_ex") == 0 )
        {
            fval_ex = co->value;
        }
        else if ( strcmp(co->key, "fval_labels") == 0 )
        {
            fval_labels = co->value;
        }
        else if ( strcmp(co->key, "ntraining") == 0 )
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
        if ( strcmp(co->key, "weightsopenfile") == 0 )
        {
            weightsopenfile = co->value;
        }
        if ( strcmp(co->key, "weightsclassificationfile") == 0 )
        {
            weightsclassificationfile = co->value;
        }
        else if ( strcmp(co->key, "nlayers") == 0 )
        {
            nlayers = atoi(co->value);

            if (nlayers < 3)
            {
                printf("\n\n ERROR: nlayers=%d too small! Choose minimum three layers (openlayer, one hidden layer, classification layer)!\n\n", nlayers);
                MPI_Finalize();
                return(0);
            }
        }
        else if ( strcmp(co->key, "activation") == 0 )
        {
            if (strcmp(co->value, "tanh") == 0 )
            {
                activation = Layer::TANH;
                activname  = "tanh";
            }
            else if ( strcmp(co->value, "ReLu") == 0 )
            {
                activation = Layer::RELU;
                activname  = "ReLu";
            }
            else if (strcmp(co->value, "SmoothReLu") == 0 )
            {
                activation = Layer::SMRELU;
                activname  = "SmoothRelu";
            }
            else
            {
                printf("Invalid activation function!");
                MPI_Finalize();
                return(0);
            }
        }
        else if ( strcmp(co->key, "network_type") == 0 )
        {
            if (strcmp(co->value, "dense") == 0 )
            {
                networkType  = Network::DENSE;
            }
            else if (strcmp(co->value, "convolutional") == 0 )
            {
                networkType  = Network::CONVOLUTIONAL;
            }
            else
            {
                printf("Invalid network type !");
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
        else if ( strcmp(co->key, "braid_cfactor0") == 0 )
        {
           braid_cfactor0 = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_maxlevels") == 0 )
        {
           braid_maxlevels = atoi(co->value);
        }
        else if ( strcmp(co->key, "braid_mincoarse") == 0 )
        {
           braid_mincoarse = atoi(co->value);
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
        else if ( strcmp(co->key, "braid_nrelax0") == 0 )
        {
           braid_nrelax0 = atoi(co->value);
        }
        else if ( strcmp(co->key, "gamma_tik") == 0 )
        {
            gamma_tik = atof(co->value);
        }
        else if ( strcmp(co->key, "gamma_ddt") == 0 )
        {
            gamma_ddt = atof(co->value);
        }
        else if ( strcmp(co->key, "gamma_class") == 0 )
        {
            gamma_class= atof(co->value);
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
        else if ( strcmp(co->key, "type_openlayer") == 0 )
        {
            if (strcmp(co->value, "replicate") == 0 )
            {
                type_openlayer = 0; 
            }
            else if ( strcmp(co->value, "activate") == 0 )
            {
                type_openlayer = 1; 
            }
            else
            {
                printf("Invalid type_openlayer!\n");
                MPI_Finalize();
                return(0);
            }
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
            else if (strcmp(co->value, "Identity") == 0 )
            {
                hessian_approx = USE_IDENTITY;
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
        else if ( strcmp(co->key, "validationlevel") == 0 )
        {
           validationlevel = atoi(co->value);
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
    sprintf(train_ex_filename,  "%s/%s", datafolder, ftrain_ex);
    sprintf(train_lab_filename, "%s/%s", datafolder, ftrain_labels);
    sprintf(val_ex_filename,    "%s/%s", datafolder, fval_ex);
    sprintf(val_lab_filename,   "%s/%s", datafolder, fval_labels);

    /* Read training and validation examples */
    if (myid == 0)   // examples are only needed on opening layer, i.e. first proc
    {
        train_examples = new MyReal* [ntraining];
        val_examples   = new MyReal* [nvalidation];
        for (int ix = 0; ix<ntraining; ix++)
        {
            train_examples[ix] = new MyReal[nfeatures];
        }
        for (int ix = 0; ix<nvalidation; ix++)
        {
            val_examples[ix] = new MyReal[nfeatures];
        }
        read_matrix(train_ex_filename, train_examples, ntraining,   nfeatures);
        read_matrix(val_ex_filename,   val_examples,   nvalidation, nfeatures);
    }
    /* Read in training and validation labels */
    if (myid == size - 1)  // labels are only needed on classification layer, i.e. last proc
    {
        train_labels = new MyReal* [ntraining];
        val_labels   = new MyReal* [nvalidation];
        for (int ix = 0; ix<ntraining; ix++)
        {
            train_labels[ix]   = new MyReal[nclasses];
        }
        for (int ix = 0; ix<nvalidation; ix++)
        {
            val_labels[ix]   = new MyReal[nclasses];
        }
        read_matrix(train_lab_filename, train_labels, ntraining,   nclasses);
        read_matrix(val_lab_filename,   val_labels,   nvalidation, nclasses);
    }

    /* Total number of hidden layers is nlayers minus opening layer minus classification layers) */
    nhiddenlayers = nlayers - 2;

    /* Initializze primal and adjoint XBraid for training data */
    app_train = (my_App *) malloc(sizeof(my_App));
    braid_Init(MPI_COMM_WORLD, MPI_COMM_WORLD, 0.0, T, nhiddenlayers, app_train, my_Step, my_Init, my_Clone, my_Free, my_Sum, my_SpatialNorm, my_Access, my_BufSize, my_BufPack, my_BufUnpack, &core_train);
    braid_Init(MPI_COMM_WORLD, MPI_COMM_WORLD, 0.0, T, nhiddenlayers, app_train, my_Step_Adj, my_Init_Adj, my_Clone, my_Free, my_Sum, my_SpatialNorm, my_Access, my_BufSize_Adj, my_BufPack_Adj, my_BufUnpack_Adj, &core_adj);
    braid_SetRevertedRanks(core_adj, 1);

    /* Init XBraid for validation data */
    app_val = (my_App *) malloc(sizeof(my_App));
    braid_Init(MPI_COMM_WORLD, MPI_COMM_WORLD, 0.0, T, nhiddenlayers, app_val, my_Step, my_Init, my_Clone, my_Free, my_Sum, my_SpatialNorm, my_Access, my_BufSize, my_BufPack, my_BufUnpack, &core_val);

    /* Store all points for primal and adjoint */
    braid_SetStorage(core_train, 0);
    braid_SetStorage(core_adj, -1);
    braid_SetStorage(core_val, -1);
    /* Set all Braid parameters */
    braid_SetMaxLevels(core_train, braid_maxlevels);
    braid_SetMaxLevels(core_val,   braid_maxlevels);
    braid_SetMaxLevels(core_adj,   braid_maxlevels);
    braid_SetMinCoarse(core_train, braid_mincoarse);
    braid_SetMinCoarse(core_val,   braid_mincoarse);
    braid_SetMinCoarse(core_adj,   braid_mincoarse);
    braid_SetPrintLevel( core_train, braid_printlevel);
    braid_SetPrintLevel( core_val,   braid_printlevel);
    braid_SetPrintLevel( core_adj,   braid_printlevel);
    braid_SetCFactor(core_train,  0, braid_cfactor0);
    braid_SetCFactor(core_val,    0, braid_cfactor0);
    braid_SetCFactor(core_adj,    0, braid_cfactor0);
    braid_SetCFactor(core_train, -1, braid_cfactor);
    braid_SetCFactor(core_val,   -1, braid_cfactor);
    braid_SetCFactor(core_adj,   -1, braid_cfactor);
    braid_SetAccessLevel(core_train, braid_accesslevel);
    braid_SetAccessLevel(core_val,   braid_accesslevel);
    braid_SetAccessLevel(core_adj,   braid_accesslevel);
    braid_SetMaxIter(core_train, braid_maxiter);
    braid_SetMaxIter(core_val,   braid_maxiter);
    braid_SetMaxIter(core_adj,   braid_maxiter);
    braid_SetSkip(core_train, braid_setskip);
    braid_SetSkip(core_val,   braid_setskip);
    braid_SetSkip(core_adj,   braid_setskip);
    if (braid_fmg){
        braid_SetFMG(core_train);
        braid_SetFMG(core_val);
        braid_SetFMG(core_adj);
    }
    braid_SetNRelax(core_train, -1, braid_nrelax);
    braid_SetNRelax(core_val,   -1, braid_nrelax);
    braid_SetNRelax(core_adj,   -1, braid_nrelax);
    braid_SetNRelax(core_train,  0, braid_nrelax0);
    braid_SetNRelax(core_val,    0, braid_nrelax0);
    braid_SetNRelax(core_adj,    0, braid_nrelax0);
    braid_SetAbsTol(core_train, braid_abstol);
    braid_SetAbsTol(core_val,   braid_abstol);
    braid_SetAbsTol(core_adj,   braid_abstol);


    /* Get xbraid's grid distribution */
    _braid_GetDistribution(core_train, &ilower, &iupper);

    /* Create network and layers */
    network = new Network(nlayers,ilower, iupper, nfeatures, nclasses, nchannels, activation, T/(MyReal)nhiddenlayers, gamma_tik, gamma_ddt, gamma_class, weights_open_init, networkType, type_openlayer);
    ndesign_local  = network->getnDesign();
    MPI_Allreduce(&ndesign_local, &ndesign_global, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    int startid = ilower;
    if (ilower == 0) startid = -1;
    printf("%d: Layer range: [%d, %d] / %d\n", myid, startid, iupper, nlayers);
    printf("%d: Design variables (local/global): %d/%d\n", myid, ndesign_local, ndesign_global);

    /* Initialize design with random numbers (do on one processor and scatter for scaling test) */
    if (myid == MASTER_NODE)
    {
        srand(1.0);
        design = new MyReal[ndesign_global];
        for (int i = 0; i < ndesign_global; i++)
        {
            design[i] = (MyReal) rand() / ((MyReal) RAND_MAX);
        }
    }
    MPI_ScatterVector(design, network->getDesign(), ndesign_local, MASTER_NODE, MPI_COMM_WORLD);
    network->initialize(weights_open_init, weights_init, weights_class_init, datafolder, weightsopenfile, weightsclassificationfile);
    network->MPI_CommunicateNeighbours(MPI_COMM_WORLD);
    MPI_GatherVector(network->getDesign(), ndesign_local, design, MASTER_NODE, MPI_COMM_WORLD);

    /* Initialize xbraid's app structure */
    app_train->primalcore  = core_train;
    app_train->myid        = myid;
    app_train->network     = network;
    app_train->nexamples   = ntraining;
    app_train->examples    = train_examples;
    app_train->labels      = train_labels;
    app_val->primalcore    = core_val;
    app_val->myid          = myid;
    app_val->network       = network;
    app_val->nexamples     = nvalidation;
    app_val->examples      = val_examples;
    app_val->labels        = val_labels;


    /* Initialize hessian approximation on first processor */
    HessianApprox  *hessian = 0;
    if (myid == MASTER_NODE)
    {
        switch (hessian_approx)
        {
            case USE_BFGS:
                hessian = new BFGS(ndesign_global);
                break;
            case USE_LBFGS: 
                hessian = new L_BFGS(ndesign_global, lbfgs_stages);
                break;
            case USE_IDENTITY:
                hessian = new Identity(ndesign_global);
        }
    }

    /* Allocate other optimization vars on first processor */
    if (myid == MASTER_NODE)
    {
        gradient   = new MyReal[ndesign_global];
        design0    = new MyReal[ndesign_global];
        gradient0  = new MyReal[ndesign_global];
        descentdir = new MyReal[ndesign_global];
    }

    /* Initialize optimization parameters */
    ls_param    = 1e-4;
    ls_iter     = 0;
    gnorm       = 0.0;
    objective   = 0.0;
    rnorm       = 0.0;
    rnorm_adj   = 0.0;
    stepsize    = stepsize_init;

    /* Open and prepare optimization output file*/
    if (myid == MASTER_NODE)
    {
        sprintf(optimfilename, "%s.dat", "optim");
        optimfile = fopen(optimfilename, "w");
        fprintf(optimfile, "# Problem setup: ntraining            %d \n", ntraining);
        fprintf(optimfile, "#                nvalidation          %d \n", nvalidation);
        fprintf(optimfile, "#                nfeatures            %d \n", nfeatures);
        fprintf(optimfile, "#                nclasses             %d \n", nclasses);
        fprintf(optimfile, "#                nchannels            %d \n", nchannels);
        fprintf(optimfile, "#                nlayers              %d \n", nlayers);
        fprintf(optimfile, "#                T                    %f \n", T);
        fprintf(optimfile, "#                Activation           %s \n", activname);
        fprintf(optimfile, "#                type openlayer       %d \n", type_openlayer);
        fprintf(optimfile, "# XBraid setup:  max levels           %d \n", braid_maxlevels);
        fprintf(optimfile, "#                min coarse           %d \n", braid_mincoarse);
        fprintf(optimfile, "#                coasening            %d \n", braid_cfactor);
        fprintf(optimfile, "#                coasening (level 0)  %d \n", braid_cfactor0);
        fprintf(optimfile, "#                max. braid iter      %d \n", braid_maxiter);
        fprintf(optimfile, "#                abs. tol             %1.e \n", braid_abstol);
        fprintf(optimfile, "#                abs. toladj          %1.e \n", braid_abstoladj);
        fprintf(optimfile, "#                print level          %d \n", braid_printlevel);
        fprintf(optimfile, "#                access level         %d \n", braid_accesslevel);
        fprintf(optimfile, "#                skip?                %d \n", braid_setskip);
        fprintf(optimfile, "#                fmg?                 %d \n", braid_fmg);
        fprintf(optimfile, "#                nrelax (level 0)     %d \n", braid_nrelax0);
        fprintf(optimfile, "#                nrelax               %d \n", braid_nrelax);
        fprintf(optimfile, "# Optimization:  gamma_tik            %1.e \n", gamma_tik);
        fprintf(optimfile, "#                gamma_ddt            %1.e \n", gamma_ddt);
        fprintf(optimfile, "#                gamma_class          %1.e \n", gamma_class);
        fprintf(optimfile, "#                stepsize             %f \n", stepsize_init);
        fprintf(optimfile, "#                max. optim iter      %d \n", maxoptimiter);
        fprintf(optimfile, "#                gtol                 %1.e \n", gtol);
        fprintf(optimfile, "#                max. ls iter         %d \n", ls_maxiter);
        fprintf(optimfile, "#                ls factor            %f \n", ls_factor);
        fprintf(optimfile, "#                weights_init         %f \n", weights_init);
        fprintf(optimfile, "#                weights_open_init    %f \n", weights_open_init);
        fprintf(optimfile, "#                weights_class_init   %f \n", weights_class_init) ;
        fprintf(optimfile, "#                hessian_approx       %d \n", hessian_approx);
        fprintf(optimfile, "#                lbfgs_stages         %d \n", lbfgs_stages);
        fprintf(optimfile, "#                validationlevel      %d \n", validationlevel);
        fprintf(optimfile, "\n");
    }

    /* Prepare optimization output */
    if (myid == MASTER_NODE)
    {
       /* Screen output */
       printf("\n#    || r ||          || r_adj ||      Objective             Loss                 || grad ||             Stepsize  ls_iter   Accur_train  Accur_val   Time(sec)\n");
       
       fprintf(optimfile, "#    || r ||          || r_adj ||      Objective             Loss                  || grad ||            Stepsize  ls_iter   Accur_train  Accur_val   Time(sec)\n");
    }


#if 1
    /* --- OPTIMIZATION --- */
    StartTime = MPI_Wtime();
    StopTime  = 0.0;
    UsedTime = 0.0;
    for (int iter = 0; iter < maxoptimiter; iter++)
    {

        /* --- Training data: Get objective and gradient ---*/ 

        /* Solve state equation with braid */
        nreq = -1;
        braid_SetPrintLevel(core_train, braid_printlevel);

        /* Apply initial condition if warm_restart (i.e. iter>0)*/
        if (_braid_CoreElt(core_train, warm_restart)) 
        {
            _braid_UGetVectorRef(core_train, 0, 0, &ubase);
            if (ubase != NULL) // only true on first processor 
            {
                u = ubase->userVector;
                Layer* openlayer = app_train->network->getLayer(-1);
                for (int iex = 0; iex < app_train->nexamples; iex++)
                {
                    /* set example */
                    if (app_train->examples !=NULL) openlayer->setExample(app_train->examples[iex]);
                    /* Apply the layer */
                    openlayer->applyFWD(u->state[iex]);
                } 
            }
        }

        braid_Drive(core_train);
        braid_GetRNorms(core_train, &nreq, &rnorm);
        /* Evaluat objective function */
        evalObjective(core_train, app_train, &objective, &loss_train, &accur_train);


        /* Solve adjoint equation with XBraid */
        nreq = -1;
        braid_SetPrintLevel(core_adj, braid_printlevel);
        evalObjectiveDiff(core_adj, app_train);
        braid_Drive(core_adj);
        braid_GetRNorms(core_adj, &nreq, &rnorm_adj);
        /* Derivative of opening layer */
        evalInitDiff(core_adj, app_train);

        /* --- Validation data: Get accuracy --- */

        if ( validationlevel > 0 )
        {
            braid_SetPrintLevel( core_val, 1);
            braid_Drive(core_val);
            /* Get loss and accuracy */
            _braid_UGetLast(core_val, &ubase);
            if (ubase != NULL) // This is true only on last processor
            {
                u = ubase->userVector;
                u->layer->evalClassification(nvalidation, u->state, val_labels, &loss_val, &accur_val, 0);
            }
        }


        /* --- Optimization control and output ---*/

        /* Compute and communicate gradient and its norm */
        MPI_GatherVector(network->getGradient(), ndesign_local, gradient, MASTER_NODE, MPI_COMM_WORLD);
        mygnorm = vec_normsq(ndesign_local, network->getGradient());
        MPI_Allreduce(&mygnorm, &gnorm, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);
        gnorm = sqrt(gnorm);

        // /* Communicate loss and accuracy. This is actually only needed for output. Remove it. */
        MyReal losstrain_out  = 0.0; 
        MyReal lossval_out    = 0.0; 
        MyReal accurtrain_out = 0.0; 
        MyReal accurval_out   = 0.0; 
        MPI_Allreduce(&loss_train, &losstrain_out, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&loss_val, &lossval_out, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&accur_train, &accurtrain_out, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&accur_val, &accurval_out, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);

        /* Output */
        StopTime = MPI_Wtime();
        UsedTime = StopTime-StartTime;
        if (myid == MASTER_NODE)
        {
            printf("%3d  %1.8e  %1.8e  %1.14e  %1.14e  %1.14e  %5f  %2d        %2.2f%%      %2.2f%%    %.1f\n", iter, rnorm, rnorm_adj, objective, losstrain_out, gnorm, stepsize, ls_iter, accurtrain_out, accurval_out, UsedTime);
            fprintf(optimfile,"%3d  %1.8e  %1.8e  %1.14e  %1.14e  %1.14e  %5f  %2d        %2.2f%%      %2.2f%%     %.1f\n", iter, rnorm, rnorm_adj, objective, losstrain_out, gnorm, stepsize, ls_iter, accurtrain_out, accurval_out, UsedTime);
            fflush(optimfile);
        }

        /* Check optimization convergence */
        if (  gnorm < gtol )
        {
            if (myid == MASTER_NODE) 
            {
                printf("Optimization has converged. \n");
                printf("Be happy and go home!       \n");
            }
            break;
        }
        if ( iter == maxoptimiter - 1 )
        {
            if (myid == MASTER_NODE)
            {
                printf("\nMax. optimization iterations reached.\n");
            }
            break;
        }
        /* --- Design update --- */

        stepsize = stepsize_init;
        /* Compute search direction on first processor */
        if (myid == MASTER_NODE)
        {
            /* Update the (L-)BFGS memory */
            hessian->updateMemory(iter, design, design0, gradient, gradient0);
            /* Compute descent direction */
            hessian->computeDescentDir(iter, gradient, descentdir);
            
            // write_vector("gradient.dat", gradient, ndesign_global);
            // write_vector("descentdir.dat", descentdir, ndesign_global);

            /* Store design and gradient into *0 vectors */
            vec_copy(ndesign_global, design, design0);
            vec_copy(ndesign_global, gradient, gradient0);

            /* Compute wolfe condition */
            wolfe = vecdot(ndesign_global, gradient, descentdir);

            /* Update the global design using the initial stepsize */
            for (int id = 0; id < ndesign_global; id++)
            {
                design[id] -= stepsize * descentdir[id];
            }
        }

        /* Broadcast/Scatter the new design and and  wolfe condition to all processors */
        MPI_Bcast(&wolfe, 1, MPI_MyReal, MASTER_NODE, MPI_COMM_WORLD);
        MPI_ScatterVector(design, network->getDesign(), ndesign_local, MASTER_NODE, MPI_COMM_WORLD);
        network->MPI_CommunicateNeighbours(MPI_COMM_WORLD);


        /* --- Backtracking linesearch --- */
        for (ls_iter = 0; ls_iter < ls_maxiter; ls_iter++)
        {
            /* Compute new objective function value for current trial step */
            braid_SetPrintLevel(core_train, 0);
            braid_Drive(core_train);
            evalObjective(core_train, app_train, &ls_objective, &loss_train, &accur_train);

            MyReal test = objective - ls_param * stepsize * wolfe;
            if (myid == MASTER_NODE) printf("ls_iter %d: %1.14e %1.14e\n", ls_iter, ls_objective, test);
            /* Test the wolfe condition */
            if (ls_objective <= objective - ls_param * stepsize * wolfe ) 
            {
                /* Success, use this new design */
                break;
            }
            else
            {
                /* Test for line-search failure */
                if (ls_iter == ls_maxiter - 1)
                {
                    if (myid == MASTER_NODE) printf("\n\n   WARNING: LINESEARCH FAILED! \n\n");
                    break;
                }

                /* Decrease the stepsize */
                stepsize = stepsize * ls_factor;

                /* Compute new design using new stepsize */
                if (myid == MASTER_NODE)
                {
                    /* Go back a portion of the step */
                    for (int id = 0; id < ndesign_global; id++)
                    {
                        design[id] += stepsize * descentdir[id];
                    }
                }
                MPI_ScatterVector(design, network->getDesign(), ndesign_local, MASTER_NODE, MPI_COMM_WORLD);
                network->MPI_CommunicateNeighbours(MPI_COMM_WORLD);
 
            }
 
        }
 
        // /* Print some statistics */
        // StopTime = MPI_Wtime();
        // UsedTime = StopTime-StartTime;
        // getrusage(RUSAGE_SELF,&r_usage);
        // myMB = (MyReal) r_usage.ru_maxrss / 1024.0;
        // MPI_Allreduce(&myMB, &globalMB, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);

        // // printf("%d; Memory Usage: %.2f MB\n",myid, myMB);
        // if (myid == MASTER_NODE)
        // {
        //     printf("\n");
        //     printf(" Used Time:        %.2f seconds\n",UsedTime);
        //     printf(" Global Memory:    %.2f MB\n", globalMB);
        //     printf("\n");
        // }
    }

    /* --- Run final validation and write prediction file --- */
    if (validationlevel > -1)
    {
        if (myid == MASTER_NODE) printf("\n --- Run final validation ---\n");
        braid_SetPrintLevel( core_val, 0);
        braid_Drive(core_val);
        /* Get loss and accuracy */
        _braid_UGetLast(core_val, &ubase);
        if (ubase != NULL) // This is only true on last processor 
        {
            u = ubase->userVector;
            u->layer->evalClassification(nvalidation, u->state, val_labels, &loss_val, &accur_val, 1);
            printf("Final validation accuracy:  %2.2f%%\n", accur_val);
        }
    }

    if (myid == MASTER_NODE) write_vector("gradient.dat", gradient, ndesign_global);
#endif




/** ==================================================================================
 * Adjoint dot test xbarTxdot = ybarTydot
 * where xbar = (dfdx)T ybar
 *       ydot = (dfdx)  xdot
 * choosing xdot to be a vector of all ones, ybar = 1.0;
 * ==================================================================================*/
#if 0
 
   if (size == 1)
    {
         int nconv_size = 3;

         printf("\n\n ============================ \n");
         printf(" Adjoint dot test: \n\n");
         printf("   ndesign   = %d (calc = %d)\n",ndesign,
                                                  nchannels*nclasses+nclasses // class layer
                                                  +(nlayers-2)+(nlayers-2)*(nconv_size*nconv_size*(nchannels/nfeatures)*(nchannels/nfeatures))); // con layers
         printf("   nchannels = %d\n",nchannels);
         printf("   nlayers   = %d\n",nlayers); 
         printf("   conv_size = %d\n",nconv_size);
         printf("   nclasses  = %d\n\n",nclasses);



        //read_vector("somedesign.dat", design, ndesign_global);
        //MPI_ScatterVector(design, network->getDesign(), ndesign_local, MASTER_NODE, MPI_COMM_WORLD);
        //network->MPI_CommunicateNeighbours(MPI_COMM_WORLD);
        
        /* Propagate through braid */ 
        braid_Drive(core_train);
        evalObjective(core_train, app_train, &obj0, &loss_train, &accur_train);

        /* Eval gradient */
        evalObjectiveDiff(core_adj, app_train);
        braid_Drive(core_adj);
        MPI_GatherVector(network->getGradient(), ndesign_local, gradient, MASTER_NODE, MPI_COMM_WORLD);
 


        MyReal xtx = 0.0;
        MyReal EPS = 1e-7;
        for (int i = 0; i < ndesign_global; i++)
        {
            /* Sum up xtx */
            xtx += gradient[i];
            /* perturb into direction "only ones" */
            app_train->network->getDesign()[i] += EPS;
        }


        /* New objective function evaluation */
        braid_Drive(core_train);
        MyReal obj1;
        evalObjective(core_train, app_train, &obj1, &loss_train, &accur_train);

        /* Finite differences */
        MyReal yty = (obj1 - obj0)/EPS;


        /* Print adjoint dot test result */
        printf(" Dot-test: %1.16e  %1.16e\n\n Rel. error  %3.6f %%\n\n", xtx, yty, (yty-xtx)/xtx * 100.);
        printf(" obj0 %1.14e, obj1 %1.14e\n", obj0, obj1);

    }

#endif

/** =======================================
 * Full finite differences 
 * ======================================= */

    // MyReal* findiff = new MyReal[ndesign];
    // MyReal* relerr = new MyReal[ndesign];
    // MyReal errnorm = 0.0;
    // MyReal obj0, obj1, design_store;
    // MyReal EPS;

    // printf("\n--------------------------------\n");
    // printf(" FINITE DIFFERENCE TESTING\n\n");

    // /* Compute baseline objective */
    // // read_vector("design.dat", design, ndesign);
    // braid_SetObjectiveOnly(core_train, 0);
    // braid_Drive(core_train);
    // braid_GetObjective(core_train, &objective);
    // obj0 = objective;

    // EPS = 1e-4;
    // for (int i = 0; i < ndesign; i++)
    // // for (int i = 0; i < 22; i++)
    // // int i=21;
    // {
    //     /* Restore design */
    //     // read_vector("design.dat", design, ndesign);
    
    //     /*  Perturb design */
    //     design_store = design[i];
    //     design[i] += EPS;

    //     /* Recompute objective */
    //     _braid_CoreElt(core_train, warm_restart) = 0;
    //     braid_SetObjectiveOnly(core_train, 1);
    //     braid_SetPrintLevel(core_train, 0);
    //     braid_Drive(core_train);
    //     braid_GetObjective(core_train, &objective);
    //     obj1 = objective;

    //     /* Findiff */
    //     findiff[i] = (obj1 - obj0) / EPS;
    //     relerr[i]  = (gradient[i] - findiff[i]) / findiff[i];
    //     errnorm += pow(relerr[i],2);

    //     printf("\n %4d: % 1.14e % 1.14e, error: % 2.4f",i, findiff[i], gradient[i], relerr[i] * 100.0);

    //     /* Restore design */
    //     design[i] = design_store;
    // }
    // errnorm = sqrt(errnorm);
    // printf("\n FinDiff ErrNorm  %1.14e\n", errnorm);

    // write_vector("findiff.dat", findiff, ndesign); 
    // write_vector("relerr.dat", relerr, ndesign); 
     

 /* ======================================= 
  * check network implementation 
  * ======================================= */
    // network->applyFWD(ntraining, train_examples, train_labels);
    // MyReal accur = network->getAccuracy();
    // MyReal regul = network->evalRegularization();
    // objective = network->getLoss() + regul;
    // printf("\n --- \n");
    // printf(" Network: obj %1.14e \n", objective);
    // printf(" ---\n");

    /* Print some statistics */
    StopTime = MPI_Wtime();
    UsedTime = StopTime-StartTime;
    getrusage(RUSAGE_SELF,&r_usage);
    myMB = (MyReal) r_usage.ru_maxrss / 1024.0;
    MPI_Allreduce(&myMB, &globalMB, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);

    // printf("%d; Memory Usage: %.2f MB\n",myid, myMB);
    if (myid == MASTER_NODE)
    {
        printf("\n");
        printf(" Used Time:        %.2f seconds\n",UsedTime);
        printf(" Global Memory:    %.2f MB\n", globalMB);
        printf(" Processors used:  %d\n", size);
        printf("\n");
    }


    /* Clean up XBraid */
    delete network;
    braid_Destroy(core_train);
    braid_Destroy(core_adj);
    if (validationlevel >= 0) braid_Destroy(core_val);
    free(app_train);
    free(app_val);

    /* Delete optimization vars */
    if (myid == MASTER_NODE)
    {
        delete hessian;
        delete [] design0;
        delete [] gradient0;
        delete [] gradient;
        delete [] design;
        delete [] descentdir;
    }

    /* Delete training and validation examples on first proc */
    if (myid == 0)
    {
        for (int ix = 0; ix<ntraining; ix++)
        {
            delete [] train_examples[ix];
        }
        for (int ix = 0; ix<nvalidation; ix++)
        {
            delete [] val_examples[ix];
        }
        delete [] train_examples;
        delete [] val_examples;
    }
    /* Delete training and validation labels on last proc */
    if (myid == size - 1)
    {
        for (int ix = 0; ix<ntraining; ix++)
        {
            delete [] train_labels[ix];
        }
        for (int ix = 0; ix<nvalidation; ix++)
        {
            delete [] val_labels[ix];
        }
        delete [] train_labels;
        delete [] val_labels;
    }

    /* Close optim file */
    if (myid == MASTER_NODE)
    {
        fclose(optimfile);
        printf("Optimfile: %s\n", optimfilename);
    }

    MPI_Finalize();
    return 0;
}
