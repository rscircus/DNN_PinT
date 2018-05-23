#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "codi.hpp"


#include "lib.h"
#include "bfgs.h"
#include "braid.h"
#include "braid_test.h"

/* Define the app structure */
typedef struct _braid_App_struct
{
    int      myid;        /* Processor rank*/
    double  *design;      /* Design variables */
    double  *design0;     /* Design variables */
    double  *gradient;    /* Gradient of objective function wrt design */
    double  *gradient0;   /* Gradient of objective function wrt design */
    // double      *Hessian;     /* Hessian matrix */
    int     *batch;       /* List of Indicees of the batch elements */
    int      nbatch;      /* Number of elements in the batch */
    int      nchannels;   /* Width of the network */
    int      ntimes;      /* number of time-steps / layers */
    double   gamma;       /* Relaxation parameter   */
    double   theta0;      /* Initial design value */
    double  *Ytarget;     /* Target data */
    double   deltaT;      /* Time-step size on fine grid */
    double   stepsize;    /* Stepsize for design updates */
} my_App;


/* Define the state vector at one time-step */
typedef struct _braid_Vector_struct
{
   double *Ytrain;            /* Training data */

} my_Vector;


int 
my_Step(braid_App        app,
        braid_Vector     ustop,
        braid_Vector     fstop,
        braid_Vector     u,
        braid_StepStatus status)
{
    int    ts;
    double tstart, tstop;
    double deltaT;
    
    /* Get the time-step size */
    braid_StepStatusGetTstartTstop(status, &tstart, &tstop);
    deltaT = tstop - tstart;
 
    /* Get the current time index */
    braid_StepStatusGetTIndex(status, &ts);
 

    /* Take one step */
    take_step(u->Ytrain, app->design, ts, deltaT, app->batch, app->nbatch, app->nchannels, 0);

 
    /* no refinement */
    braid_StepStatusSetRFactor(status, 1);
 
 
    return 0;
}   


int
my_Init(braid_App     app,
        double        t,
        braid_Vector *u_ptr)
{

    my_Vector *u;
    int nchannels = app->nchannels;
    int nbatch    = app->nbatch;
 
    /* Allocate the vector */
    u = (my_Vector *) malloc(sizeof(my_Vector));
    u->Ytrain = (double*) malloc(nchannels * nbatch *sizeof(double));
 
    /* Initialize the vector */
    if (t == 0.0)
    {
        /* Read training data from file */
        read_data("Ytrain.transpose.dat", u->Ytrain, nchannels * nbatch);
    }
    else
    {
        for (int i = 0; i < nchannels * nbatch; i++)
        {
            u->Ytrain[i] = 0.0;
        }
    }

    *u_ptr = u;

    return 0;
}


int
my_Clone(braid_App     app,
         braid_Vector  u,
         braid_Vector *v_ptr)
{
   my_Vector *v;
   int nchannels = app->nchannels;
   int nbatch    = app->nbatch;
   
   /* Allocate the vector */
   v = (my_Vector *) malloc(sizeof(my_Vector));
   v->Ytrain = (double*) malloc(nchannels * nbatch *sizeof(double));

   /* Clone the values */
    for (int i = 0; i < nchannels * nbatch; i++)
    {
        v->Ytrain[i] = u->Ytrain[i];
    }

   *v_ptr = v;

   return 0;
}


int
my_Free(braid_App    app,
        braid_Vector u)
{
   free(u->Ytrain);
   free(u);

   return 0;
}

int
my_Sum(braid_App     app,
       double        alpha,
       braid_Vector  x,
       double        beta,
       braid_Vector  y)
{
    int nchannels = app->nchannels;
    int nbatch    = app->nbatch;

    for (int i = 0; i < nchannels * nbatch; i++)
    {
       (y->Ytrain)[i] = alpha*(x->Ytrain)[i] + beta*(y->Ytrain)[i];
    }

   return 0;
}

int
my_SpatialNorm(braid_App     app,
               braid_Vector  u,
               double       *norm_ptr)
{
    int nchannels = app->nchannels;
    int nbatch    = app->nbatch;
    double dot;

    dot = 0.0;
    for (int i = 0; i < nchannels * nbatch; i++)
    {
       dot += pow( getValue(u->Ytrain[i]), 2 );
    }
 
   *norm_ptr = sqrt(dot);

   return 0;
}



int
my_Access(braid_App          app,
          braid_Vector       u,
          braid_AccessStatus astatus)
{
    int   idx;
   char  filename[255];

    braid_AccessStatusGetTIndex(astatus, &idx);

    if (idx == app->ntimes)
    {
        sprintf(filename, "%s.%03d", "Yout.pint", app->myid);
        write_data(filename, u->Ytrain, app->nbatch * app->nchannels);
    }

    return 0;
}


int
my_BufSize(braid_App           app,
           int                 *size_ptr,
           braid_BufferStatus  bstatus)
{
    int nchannels = app->nchannels;
    int nbatch    = app->nbatch;
    
    *size_ptr = nchannels*nbatch*sizeof(double);
    return 0;
}



int
my_BufPack(braid_App           app,
           braid_Vector        u,
           void               *buffer,
           braid_BufferStatus  bstatus)
{
    double *dbuffer   = (double*) buffer;
    int          nchannels = app->nchannels;
    int          nbatch    = app->nbatch;
    
    for (int i = 0; i < nchannels * nbatch; i++)
    {
       dbuffer[i] = (u->Ytrain)[i];
    }
 
    braid_BufferStatusSetSize( bstatus,  nchannels*nbatch*sizeof(double));
 
   return 0;
}



int
my_BufUnpack(braid_App           app,
             void               *buffer,
             braid_Vector       *u_ptr,
             braid_BufferStatus  bstatus)
{
    my_Vector   *u         = NULL;
    double *dbuffer   = (double*) buffer;
    int          nchannels = app->nchannels;
    int          nbatch    = app->nbatch;
 
     /* Allocate the vector */
     u = (my_Vector *) malloc(sizeof(my_Vector));
     u->Ytrain = (double*) malloc(nchannels * nbatch *sizeof(double));

    /* Unpack the buffer */
    for (int i = 0; i < nchannels * nbatch; i++)
    {
       (u->Ytrain)[i] = dbuffer[i];
    }
 
    *u_ptr = u;
    return 0;
}


int 
my_ObjectiveT(braid_App              app,
              braid_Vector           u,
              braid_ObjectiveStatus  ostatus,
              double                *objective_ptr)
{
    double tmp;
    double obj = 0.0;
    int    ts;
 
    /* Get the time index*/
    braid_ObjectiveStatusGetTIndex(ostatus, &ts);
 
    if (ts < app->ntimes)
    {
        /* Compute regularization term */
        tmp = app->gamma * regularization(app->design, ts, app->deltaT, app->ntimes, app->nchannels);
        obj = tmp;
    }
    else
    {
        /* Evaluate loss at last layer*/
       tmp  = 1./app->nbatch * loss(u->Ytrain, app->Ytarget, app->batch,  app->nbatch, app->nchannels);
       obj = tmp;
    }

    *objective_ptr = getValue(obj);
    
    return 0;
}


int
my_ObjectiveT_diff(braid_App            app,
                  braid_Vector          u,
                  braid_Vector          u_bar,
                  braid_Real            f_bar,
                  braid_ObjectiveStatus ostatus)
{
    int    nbatch    = app->nbatch;
    int    nchannels = app->nchannels;
    int    ntimes    = app->ntimes;
    int    ndesign   = (nchannels * nchannels + 1 ) * ntimes;
    int    nstate    = nchannels * nbatch;
    int    ts;
    RealReverse obj;
 
    /* Get the time index*/
    braid_ObjectiveStatusGetTIndex(ostatus, &ts); 

    /* Set up CoDiPack */
    RealReverse::TapeType& codiTape = RealReverse::getGlobalTape();
    codiTape.setActive();

    /* Register input */
    RealReverse* Ycodi;  /* CodiType for the state */
    RealReverse* design; /* CodiType for the design */
    Ycodi  = (RealReverse*) malloc(nstate  * sizeof(RealReverse));
    design = (RealReverse*) malloc(ndesign * sizeof(RealReverse));
    for (int i = 0; i < nstate; i++)
    {
        Ycodi[i] = u->Ytrain[i];
        codiTape.registerInput(Ycodi[i]);
    }
    /* Register design as input */
    for (int i = 0; i < ndesign; i++)
    {
        design[i] = app->design[i];
        codiTape.registerInput(design[i]);
    }

    /* Tape the objective function evaluation */
    if (ts < app->ntimes)
    {
        /* Compute regularization term */
        obj = app->gamma * regularization(design, ts, app->deltaT, ntimes, nchannels);
    }
    else
    {
        /* Evaluate loss at last layer*/
       obj = 1./app->nbatch * loss(Ycodi, app->Ytarget, app->batch,  nbatch, nchannels);
    } 

    
    /* Set the seed */
    codiTape.setPassive();
    obj.setGradient(f_bar);

    /* Evaluate the tape */
    codiTape.evaluate();

    /* Update adjoint variables and gradient */
    for (int i = 0; i < nstate; i++)
    {
        u_bar->Ytrain[i] = Ycodi[i].getGradient();
    }
    for (int i = 0; i < ndesign; i++)
    {
        app->gradient[i] += design[i].getGradient();
    }

    /* Reset the codi tape */
    codiTape.reset();

    /* Clean up */
    free(Ycodi);
    free(design);

   return 0;
}

int
my_Step_diff(braid_App         app,
             braid_Vector      ustop,     /**< input, u vector at *tstop* */
             braid_Vector      u,         /**< input, u vector at *tstart* */
             braid_Vector      ustop_bar, /**< input / output, adjoint vector for ustop */
             braid_Vector      u_bar,     /**< input / output, adjoint vector for u */
             braid_StepStatus  status)
{

    double  tstop, tstart, deltaT;
    int     ts;
    int     nchannels = app->nchannels;
    int     nbatch    = app->nbatch;
    int     ntimes    = app->ntimes;
    int     ndesign   = (nchannels * nchannels + 1 ) * ntimes;
    int     nstate    = nchannels * nbatch;

    /* Get time and time step */
    braid_StepStatusGetTstartTstop(status, &tstart, &tstop);
    braid_StepStatusGetTIndex(status, &ts);
    deltaT = tstop - tstart;

    /* Prepare CodiPack Tape */
    RealReverse::TapeType& codiTape = RealReverse::getGlobalTape();
    codiTape.setActive();

    /* Register input */
    RealReverse* Ycodi;  /* CodiType for the state */
    RealReverse* Ynext;  /* CodiType for the state, next time-step */
    RealReverse* design; /* CodiType for the design */
    Ycodi  = (RealReverse*) malloc(nstate  * sizeof(RealReverse));
    Ynext  = (RealReverse*) malloc(nstate  * sizeof(RealReverse));
    design = (RealReverse*) malloc(ndesign * sizeof(RealReverse));
    for (int i = 0; i < nstate; i++)
    {
        Ycodi[i] = u->Ytrain[i];
        codiTape.registerInput(Ycodi[i]);
        Ynext[i] = Ycodi[i];
    }
    /* Register design as input */
    for (int i = 0; i < ndesign; i++)
    {
        design[i] = app->design[i];
        codiTape.registerInput(design[i]);
    }
    
    /* Take one forward step */
    take_step(Ynext, design, ts, deltaT, app->batch, nbatch, nchannels, 0);

    /* Set the adjoint variables */
    codiTape.setPassive();
    for (int i = 0; i < nchannels * nbatch; i++)
    {
        Ynext[i].setGradient(u_bar->Ytrain[i]);
    }

    /* Evaluate the tape */
    codiTape.evaluate();

    /* Update adjoint variables and gradient */
    for (int i = 0; i < nstate; i++)
    {
        u_bar->Ytrain[i] = Ycodi[i].getGradient();
    }
    for (int i = 0; i < ndesign; i++)
    {
        app->gradient[i] += design[i].getGradient();
    }

    /* Reset the codi tape */
    codiTape.reset();

    /* Clean up */
    free(Ycodi);
    free(Ynext);
    free(design);

    return 0;
}
 
// int 
// my_AccessGradient(braid_App app)
// {
//     int g_idx;
//     double nchannels = app->nchannels;
//     double ntimes    = app->ntimes;

//    /* Print the gradient */
//     // printf("Gradient:\n"); 

//     for (int ts = 0; ts < ntimes; ts++)
//     {

//         for (int ichannel = 0; ichannel < nchannels; ichannel++)
//         {
            
//             for (int jchannel = 0; jchannel < nchannels; jchannel++)
//             {
//                 g_idx = ts * (nchannels*nchannels + 1) + ichannel * nchannels + jchannel;
//                 printf("%d %d %d %d %1.14e\n", ts, ichannel, jchannel, g_idx, app->gradient[g_idx]);
//             }
//         }
//         g_idx = ts * (nchannels*nchannels + 1) + nchannels* nchannels;
//         // printf("%d     %d %1.14e\n", ts, g_idx, app->gradient[g_idx]);

//     }

//    return 0;
// }

int
my_AllreduceGradient(braid_App app, 
                     MPI_Comm comm)
{

   /* Collect sensitivities from all time-processors */

   return 0;
}                  


int 
my_ResetGradient(braid_App app)
{
    int ndesign = (app->nchannels * app->nchannels + 1) * app->ntimes;

    /* Set the gradient to zero */
    for (int idesign = 0; idesign < ndesign; idesign++)
    {
        app->gradient[idesign] = 0.0;
    }

    return 0;
}

int
my_GradientNorm(braid_App app,
                double   *gradient_norm_prt)

{
    double ndesign = (app->nchannels * app->nchannels + 1) * app->ntimes;
    double gnorm;

    /* Norm of gradient */
    gnorm = 0.0;
    for (int idesign = 0; idesign < ndesign; idesign++)
    {
        gnorm += pow(getValue(app->gradient[idesign]), 2);
    }
    gnorm = sqrt(gnorm);

    *gradient_norm_prt = gnorm;
    
    return 0;
}
    

// int 
// my_DesignUpdate(braid_App app, 
//                 double    objective,
//                 double    rnorm,
//                 double    rnorm_adj)
// {
//     double upd;
//     int    ndesign = (app->nchannels * app->nchannels + 1) * app->ntimes;
//     double *sk     = (double*)malloc(ndesign*sizeof(double));
//     double *yk     = (double*)malloc(ndesign*sizeof(double));

//     /* Hessian approximation  */
    
//     for (int idesign = 0; idesign < ndesign; idesign++)
//     {
//         sk[idesign] = app->design[idesign] - app->design0[idesign];
//         yk[idesign] = app->gradient[idesign] - app->gradient0[idesign];
//     }
//     bfgs_update(ndesign, sk, yk, app->Hessian);


//     for (int idesign = 0; idesign < ndesign; idesign++)
//     {
//         /* Store old design and gradient */
//         app->design0[idesign]   = app->design[idesign];
//         app->gradient0[idesign] = app->gradient[idesign];

//         /* Design update */
//         upd = 0.0;
//         for (int jdesign = 0; jdesign < ndesign; jdesign++)
//         {
//             upd += app->Hessian[idesign*ndesign + jdesign] * app->gradient[jdesign];
//         }
//         app->design[idesign] -= app->stepsize * upd;
//     }

//     printf("\n");

//     free(sk);
//     free(yk);

//    return 0;
// }             


int main (int argc, char *argv[])
{
    braid_Core core;
    my_App     *app;

    double *design;       /**< Design variables for the network */
    double *design0;      /**< Design variables for the network */
    double       objective;    /**< Objective function */
    double      *gradient;     /**< Gradient of objective function wrt design */
    double      *gradient0;    /**< Gradient of objective function wrt design */
    double       gnorm;        /**< Norm of the gradient */
    double       gamma;        /**< Relaxation parameter */
    double      *Ytarget;      /**< Target data */
    int         *batch;        /**< Contains indicees of the batch elements */
    int          nexamples;    /**< Number of elements in the training data */
    int          nbatch;       /**< Size of a batch */
    int          ndesign;      /**< dimension of the design variables */
    int          ntimes;       /**< Number of layers / time steps */
    int          nchannels;    /**< Number of channels of the netword (width) */
    double       T;            /**< Final time */
    double       theta0;       /**< Initial design value */
    int          myid;         /**< Processor rank */
    double       deltaT;       /**< Time step size */
    double       stepsize;     /**< Stepsize for design updates */
    // double *Hessian;      /**< Hessian matrix */
    double       findiff;      /**< flag: test gradient with finite differences (1) */


    /* Problem setup */ 
    nexamples = 5000;
    nchannels = 4;
    ntimes    = 32;
    deltaT    = 10./32.;     // should be T / ntimes, hard-coded for now due to testing;
    T         = deltaT * ntimes;
    theta0    = 1e-2;
    gamma     = 1e-2;
    stepsize  = 0.1;
    nbatch    = nexamples;
    ndesign   = (nchannels * nchannels + 1 )* ntimes;

    /* Read the target data */
    Ytarget = (double*) malloc(nchannels*nexamples*sizeof(double));
    read_data("Ytarget.transpose.dat", Ytarget, nchannels*nexamples);

    /* Initialize the design and gradient */
    design    = (double*) malloc(ndesign*sizeof(double));
    design0   = (double*) malloc(ndesign*sizeof(double));
    gradient  = (double*) malloc(ndesign*sizeof(double));
    gradient0 = (double*) malloc(ndesign*sizeof(double));
    for (int idesign = 0; idesign < ndesign; idesign++)
    {
        design[idesign]     = theta0; 
        design0[idesign]    = 0.0; 
        gradient[idesign]   = 0.0; 
        gradient0[idesign]  = 1.0; 
    }
    /* Read in optimal theta */
    // read_data("thetaopt.dat", design, ndesign);

    /* Initialize Hessian */
    // Hessian = (double*) malloc(ndesign*ndesign*sizeof(double));
    // set_identity(ndesign, Hessian);


    /* Initialize the batch (same as examples for now) */
    batch   = (int*) malloc(nbatch*sizeof(int));
    for (int ibatch = 0; ibatch < nbatch; ibatch++)
    {
        batch[ibatch] = ibatch;
    }


    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);


    /* Set up the app structure */
    app = (my_App *) malloc(sizeof(my_App));
    app->myid      = myid;
    app->design    = design;
    app->design0   = design0;
    app->gradient  = gradient;
    app->gradient0 = gradient0;
    app->batch     = batch;
    app->nbatch    = nbatch;
    app->nchannels = nchannels;
    app->ntimes    = ntimes;
    app->theta0    = theta0;
    app->deltaT    = deltaT;
    app->Ytarget   = Ytarget;
    app->gamma     = gamma;
    app->stepsize  = stepsize;
    // app->Hessian   = Hessian;

    /* Initialize XBraid */
    braid_Init(MPI_COMM_WORLD, MPI_COMM_WORLD, 0.0, T, ntimes, app, my_Step, my_Init, my_Clone, my_Free, my_Sum, my_SpatialNorm, my_Access, my_BufSize, my_BufPack, my_BufUnpack, &core);

    /* Initialize adjoint XBraid */
    braid_InitAdjoint( my_ObjectiveT, my_ObjectiveT_diff, my_Step_diff,  my_ResetGradient, &core);

    /* Set some Braid parameters */
    braid_SetPrintLevel( core, 1);
    braid_SetMaxLevels(core, 1);
    braid_SetCFactor(core, -1, 2);
    braid_SetAccessLevel(core, 1);
    braid_SetMaxIter(core, 10);
    braid_SetSkip(core, 0);

    /* Switch for finite difference testing */
    findiff = 1;

    /* Run a Braid simulation */
    braid_Drive(core);

    braid_GetObjective(core, &objective);

    /* Get Gradient norm */
    my_GradientNorm(app, &gnorm);

    /* Output */
    printf("\n Loss:         %1.14e", objective);
    printf("\n Gradientnorm: %1.14e", gnorm);
    printf("\n\n");

    /* Print the gradient to file */
    write_data("gradient.dat", app->gradient, ndesign);


    /** ---------------------------------------------------------- 
     * DEBUG: Finite difference testing 
     * ---------------------------------------------------------- */
    if (findiff)
    {
        printf("\n\n------- FINITE DIFFERENCE TESTING --------\n\n");
        double obj_orig, obj_perturb;
        double findiff, relerror, max_err;
        double *err = (double*)malloc(ndesign*sizeof(double));
        double EPS    = 1e-7;
        double tolerr = 1e-0;

        max_err = 0.0;
        for (int idx = 0; idx < ndesign; idx++)
        {
            /* store the original objective */
            obj_orig = _braid_CoreElt(core, optim)->objective;
            
            /* Perturb the design */
            app->design[idx] += EPS;

            /* Reset the gradient from previous run */
            my_ResetGradient(app);
            /* Run a Braid simulation */
            braid_Drive(core);

            /* Get perturbed objective */
            obj_perturb = _braid_CoreElt(core, optim)->objective;

            /* Finite differences */
            findiff  = (obj_perturb - obj_orig) / EPS;
            relerror = (app->gradient[idx] - findiff) / findiff;
            relerror = sqrt(relerror*relerror);
            err[idx] = relerror;
            if (max_err < relerror)
            {
                max_err = relerror;
            }
            printf("\n %d: obj_orig %1.14e, obj_perturb %1.14e\n", idx, obj_orig, obj_perturb );
            printf("     findiff %1.14e, grad %1.14e, -> ERR %1.14e\n\n", findiff, app->gradient[idx], relerror );

            if (fabs(relerror) > tolerr)
            {
                printf("\n\n RELATIVE ERROR TO BIG! DEBUG! \n\n");
                exit(1);
            }

        }
        printf("\n\n MAX. FINITE DIFFERENCES ERROR: %1.14e\n\n", max_err);
        
        free(err);
    }


    /* Clean up */
    // free(Hessian);
    free(design0);
    free(design);
    free(gradient0);
    free(gradient);
    free(batch);
    free(app);

    braid_Destroy(core);
    MPI_Finalize();


    return 0;
}