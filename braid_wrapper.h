#include "braid.h"

/* Define the app structure */
typedef struct _braid_App_struct
{
    int      myid;          /* Processor rank*/
    double  *Clabels;       /* Data: Label vectors (C) */
    double  *Ydata;         /* Training data */
    double  *theta;         /* theta variables */
    double  *class_W;       /* Weights of the classification problem (W) */
    double  *class_W_grad;  /* Gradient wrt the classification weights */
    double  *class_mu;      /* Bias of the classification problem (mu) */
    double  *class_mu_grad; /* Gradient wrt the classification bias */
    double  *descentdir;    /* Descent direction (hessian times gradient) */
    double  *theta_grad;      /* Gradient of objective function wrt theta */
    double  *Hessian;       /* Hessian matrix */
    int     *batch;         /* List of Indicees of the batch elements */
    int      nclasses;      /* Number of classes */
    int      nbatch;        /* Number of elements in the batch */
    int      nchannels;     /* Width of the network */
    int      ntimes;        /* number of time-steps / layers */
    double   gamma_theta;   /* Relaxation parameter for theta */
    double   gamma_class;   /* Relaxation parameter for the classification weights W and bias mu */
    double   deltaT;        /* Time-step size on fine grid */
    double   stepsize;      /* Stepsize for theta updates */
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
        braid_StepStatus status);

int
my_Init(braid_App     app,
        double        t,
        braid_Vector *u_ptr);


int
my_Clone(braid_App     app,
         braid_Vector  u,
         braid_Vector *v_ptr);


int
my_Free(braid_App    app,
        braid_Vector u);


int
my_Sum(braid_App     app,
       double        alpha,
       braid_Vector  x,
       double        beta,
       braid_Vector  y);


int
my_SpatialNorm(braid_App     app,
               braid_Vector  u,
               double       *norm_ptr);


int
my_Access(braid_App          app,
          braid_Vector       u,
          braid_AccessStatus astatus);


int
my_BufSize(braid_App           app,
           int                 *size_ptr,
           braid_BufferStatus  bstatus);


int
my_BufPack(braid_App           app,
           braid_Vector        u,
           void               *buffer,
           braid_BufferStatus  bstatus);


int
my_BufUnpack(braid_App           app,
             void               *buffer,
             braid_Vector       *u_ptr,
             braid_BufferStatus  bstatus);


int 
my_ObjectiveT(braid_App              app,
              braid_Vector           u,
              braid_ObjectiveStatus  ostatus,
              double                *objective_ptr);

int
my_ObjectiveT_diff(braid_App            app,
                  braid_Vector          u,
                  braid_Vector          u_bar,
                  braid_Real            f_bar,
                  braid_ObjectiveStatus ostatus);

int
my_Step_diff(braid_App         app,
             braid_Vector      ustop,     /**< input, u vector at *tstop* */
             braid_Vector      u,         /**< input, u vector at *tstart* */
             braid_Vector      ustop_bar, /**< input / output, adjoint vector for ustop */
             braid_Vector      u_bar,     /**< input / output, adjoint vector for u */
             braid_StepStatus  status);

int 
my_ResetGradient(braid_App app);





