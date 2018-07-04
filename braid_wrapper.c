#include "lib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "braid_wrapper.h"
#include "codi.hpp"


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
    take_step(u->Ytrain, app->theta, ts, deltaT, app->batch, app->nbatch, app->nchannels, 0);

 
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
        /* Initialize with training data */
        for (int i = 0; i < nchannels * nbatch; i++)
        {
            u->Ytrain[i] = app->Ydata[i];
        }
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
        sprintf(filename, "%s.%02d", "Yout.pint.myid", app->myid);
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
    int    nbatch    = app->nbatch;
    int    nchannels = app->nchannels;
    int    ntimes    = app->ntimes;
    int    nclasses  = app->nclasses;
    double tmp;
    double obj = 0.0;
    int    ts;
 
    /* Get the time index*/
    braid_ObjectiveStatusGetTIndex(ostatus, &ts);
 
    if (ts < ntimes)
    {
        /* Compute regularization term for theta */
        tmp = app->gamma_theta * regularization_theta(app->theta, ts, app->deltaT, ntimes, nchannels);
        obj = tmp;
    }
    else
    {
        /* Evaluate loss */
       obj = 1./nbatch * loss(u->Ytrain, app->Clabels, app->batch, nbatch, app->classW, app->classMu, nclasses, nchannels);
    //    printf(" ts = ntimes, my_Obj %1.14e\n", obj);

       /* Add regularization for classifier */
    //    obj += app->gamma_class * regularization_class(app->classW, app->classMu, nclasses, nchannels);
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
    int nbatch    = app->nbatch;
    int nchannels = app->nchannels;
    int ntimes    = app->ntimes;
    int nclasses  = app->nclasses;
    int ntheta    = (nchannels * nchannels + 1 ) * ntimes;
    int nstate    = nchannels * nbatch;
    int nclassW   = nchannels*nclasses;
    int nclassmu  = nclasses;
    int ts;
    RealReverse obj = 0.0;
 
    /* Get the time index*/
    braid_ObjectiveStatusGetTIndex(ostatus, &ts); 

    /* Set up CoDiPack */
    RealReverse::TapeType& codiTape = RealReverse::getGlobalTape();
    codiTape.setActive();

    /* Register input */
    RealReverse* Ycodi;   /* CodiType for the state */
    RealReverse* theta;   /* CodiType for the theta */
    RealReverse* classW; /* CoDiTypye for classW */
    RealReverse* classMu; /* CoDiTypye for classMu */
    Ycodi     = (RealReverse*) malloc(nstate   * sizeof(RealReverse));
    theta     = (RealReverse*) malloc(ntheta   * sizeof(RealReverse));
    classW   = (RealReverse*) malloc(nclassW  * sizeof(RealReverse));
    classMu  = (RealReverse*) malloc(nclassmu * sizeof(RealReverse));
    for (int i = 0; i < nstate; i++)
    {
        Ycodi[i] = u->Ytrain[i];
        codiTape.registerInput(Ycodi[i]);
    }
    for (int i = 0; i < ntheta; i++)
    {
        theta[i] = app->theta[i];
        codiTape.registerInput(theta[i]);
    }
    for (int i = 0; i < nclassW; i++)
    {
        classW[i] = app->classW[i];
        codiTape.registerInput(classW[i]);
    }
    for (int i=0; i<nclassmu; i++)
    {
        classMu[i] = app->classMu[i];
        codiTape.registerInput(classMu[i]);
    }
    

    /* Tape the objective function evaluation */
    if (ts < app->ntimes)
    {
        /* Compute regularization term */
        obj = app->gamma_theta * regularization_theta(theta, ts, app->deltaT, ntimes, nchannels);
    }
    else
    {
        /* Evaluate loss at last layer*/
       obj = 1./app->nbatch * loss(Ycodi, app->Clabels, app->batch,  nbatch, classW, classMu, nclasses, nchannels);
    //    printf(" ts = ntimes, my_Obj_diff %1.14e\n", obj);

       /* Add regularization for classifier */
    //    obj += app->gamma_class * regularization_class(classW, classMu, nclasses, nchannels);
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
    for (int i = 0; i < ntheta; i++)
    {
        app->theta_grad[i] += theta[i].getGradient();
    }
    for (int i = 0; i < nclassW; i++)
    {
        app->classW_grad[i] += classW[i].getGradient();
    }
    for (int i=0; i < nclassmu; i++)
    {
        app->classMu_grad[i] += classMu[i].getGradient();
        // printf("ts %d, iclass %d, gradient %1.14e\n", ts, i, classMu[i].getGradient() );
    }

    /* Reset the codi tape */
    codiTape.reset();

    /* Clean up */
    free(Ycodi);
    free(theta);
    free(classW);
    free(classMu);

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
    int     ntheta   = (nchannels * nchannels + 1 ) * ntimes;
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
    RealReverse* theta; /* CodiType for the theta */
    Ycodi  = (RealReverse*) malloc(nstate  * sizeof(RealReverse));
    Ynext  = (RealReverse*) malloc(nstate  * sizeof(RealReverse));
    theta  = (RealReverse*) malloc(ntheta * sizeof(RealReverse));
    for (int i = 0; i < nstate; i++)
    {
        Ycodi[i] = u->Ytrain[i];
        codiTape.registerInput(Ycodi[i]);
        Ynext[i] = Ycodi[i];
    }
    /* Register theta as input */
    for (int i = 0; i < ntheta; i++)
    {
        theta[i] = app->theta[i];
        codiTape.registerInput(theta[i]);
    }
    
    /* Take one forward step */
    take_step(Ynext, theta, ts, deltaT, app->batch, nbatch, nchannels, 0);

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
    for (int i = 0; i < ntheta; i++)
    {
        app->theta_grad[i] += theta[i].getGradient();

    }

    /* Reset the codi tape */
    codiTape.reset();

    /* Clean up */
    free(Ycodi);
    free(Ynext);
    free(theta);

    return 0;
}

int 
my_ResetGradient(braid_App app)
{
    int ntheta   = (app->nchannels * app->nchannels + 1) * app->ntimes;
    int nclassW  =  app->nchannels * app->nclasses;
    int nclassmu =  app->nclasses;

    /* Set the gradient to zero */
    for (int itheta = 0; itheta < ntheta; itheta++)
    {
        app->theta_grad[itheta] = 0.0;
    }
    for (int i = 0; i < nclassW; i++)
    {
        app->classW_grad[i] = 0.0;
    }
    for (int i = 0; i < nclassmu; i++)
    {
        app->classMu_grad[i] = 0.0;
    }


    return 0;
}