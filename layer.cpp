#include <math.h>
#include "layer.hpp"

Layer::Layer()
{
   dim_In       = 0;
   dim_Out      = 0;
   dim_Bias     = 0;
   ndesign      = 0;

   index        = 0;
   dt           = 0.0;
   weights      = NULL;
   weights_bar  = NULL;
   bias         = NULL;
   bias_bar     = NULL;
   activation   = NULL;
   dactivation  = NULL;
   gamma        = 0.0;
   update       = NULL;
   update_bar   = NULL;
}

Layer::Layer(int     idx,
             int     dimI,
             int     dimO,
             int     dimB,
             double  deltaT,
             double (*Activ)(double x),
             double  (*dActiv)(double x),
             double  Gamma)
{
   index       = idx;
   dim_In      = dimI;
   dim_Out     = dimO;
   dim_Bias    = dimB;
   ndesign     = dimI * dimO + dimB;
   dt          = deltaT;
   activation  = Activ;
   dactivation = dActiv;
   gamma       = Gamma;
   
   update     = new double[dimO];
   update_bar = new double[dimO];
}   
 
// Layer::Layer(0, dimI, dimO, 1)
Layer::Layer(int idx, 
             int dimI, 
             int dimO, 
             int dimB) : Layer(idx, dimI, dimO, dimB, 1.0, NULL, NULL, 0.0) {}         

Layer::~Layer()
{
    delete [] update;
    delete [] update_bar;
}


void Layer::setDt(double DT) { dt = DT; }

double* Layer::getWeights() { return weights; }
double* Layer::getBias()    { return bias; }

double* Layer::getWeightsBar() { return weights_bar; }
double* Layer::getBiasBar()    { return bias_bar; }

int Layer::getDimIn()   { return dim_In;   }
int Layer::getDimOut()  { return dim_Out;  }
int Layer::getDimBias() { return dim_Bias; }
int Layer::getnDesign() { return ndesign; }

void Layer::print_data(double* data)
{
    printf("DATA: ");
    for (int io = 0; io < dim_Out; io++)
    {
        printf("%1.14e ", data[io]);
    }
    printf("\n");
}

void Layer::initialize(double* design_ptr,
                       double* gradient_ptr,
                       double  factor)
{
    /* Set primal and adjoint weights memory locations */
    weights     = design_ptr;
    weights_bar = gradient_ptr;
    
    /* Bias memory locations is a shift by number of weights */
    int nweights = dim_Out * dim_In;
    bias         = design_ptr + nweights;    
    bias_bar     = gradient_ptr + nweights;

    /* Initialize */
    for (int i = 0; i < ndesign - dim_Bias; i++)
    {
        weights[i]     = factor * (double) rand() / ((double) RAND_MAX);
        weights_bar[i] = 0.0;
    }
    for (int i = 0; i < ndesign - nweights; i++)
    {
        bias[i]     = factor * (double) rand() / ((double) RAND_MAX);
        bias_bar[i] = 0.0;
    }
}                   

void Layer::resetBar()
{
    for (int i = 0; i < ndesign - dim_Bias; i++)
    {
        weights_bar[i] = 0.0;
    }
    for (int i = 0; i < ndesign - dim_In * dim_Out; i++)
    {
        bias_bar[i] = 0.0;
    }
}


double Layer::evalTikh()
{
    double tik = 0.0;
    for (int i = 0; i < ndesign - dim_Bias; i++)
    {
        tik += pow(weights[i],2);
    }
    for (int i = 0; i < ndesign - dim_In * dim_Out; i++)
    {
        tik += pow(bias[i],2);
    }

    return gamma / 2.0 * tik;
}

void Layer::evalTikh_diff(double regul_bar)
{
    regul_bar = gamma * regul_bar;

    /* Derivative bias term */
    for (int i = 0; i < ndesign - dim_In * dim_Out; i++)
    {
        bias_bar[i] += bias[i] * regul_bar;
    }
    for (int i = 0; i < ndesign - dim_Bias; i++)
    {
        weights_bar[i] += weights[i] * regul_bar;
    }
}

void Layer::setExample(double* example_ptr) {}

double Layer::evalLoss(double *data_Out, 
                       double *label) { return 0.0; }


void Layer::evalLoss_diff(double *data_Out, 
                          double *data_Out_bar,
                          double *label,
                          double  loss_bar) 
{
    /* dfdu = 0.0 */
    for (int io = 0; io < dim_Out; io++)
    {
        data_Out_bar[io] =  0.0;
    }
}

int Layer::prediction(double* data, double* label) {return 0;}


DenseLayer::DenseLayer(int     idx,
                       int     dimI,
                       int     dimO,
                       double  deltaT,
                       double (*Activ)(double x),
                       double (*dActiv)(double x),
                       double  Gamma) : Layer(idx, dimI, dimO, 1, deltaT, Activ, dActiv, Gamma)
{}
   
DenseLayer::~DenseLayer() {}


void DenseLayer::applyFWD(double* state)
{
   /* Affine transformation */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Apply weights */
      update[io] = vecdot(dim_In, &(weights[io*dim_In]), state);

      /* Add bias */
      update[io] += bias[0];
   }

      /* Apply step */
   for (int io = 0; io < dim_Out; io++)
   {
      state[io] = state[io] + dt * activation(update[io]);
   }
}


void DenseLayer::applyBWD(double* state,
                          double* state_bar)
{

   /* state_bar is the adjoint of the state variable, it contains the 
      old time adjoint informationk, and is modified on the way out to
      contain the update. */

   /* Derivative of the step */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Recompute affine transformation */
        update[io]  = vecdot(dim_In, &(weights[io*dim_In]), state);
        update[io] += bias[0];
        
        /* Derivative: This is the update from old time */
        update_bar[io] = dt * dactivation(update[io]) * state_bar[io];
   }

    /* Derivative of linear transformation */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Derivative of bias addition */
      bias_bar[0] += update_bar[io];

      /* Derivative of weight application */
      for (int ii = 0; ii < dim_In; ii++)
      {
         weights_bar[io*dim_In + ii] += state[ii] * update_bar[io];
         state_bar[ii] += weights[io*dim_In + ii] * update_bar[io]; 
      }
   }
}


OpenDenseLayer::OpenDenseLayer(int     dimI,
                               int     dimO,
                               double (*Activ)(double x),
                               double (*dActiv)(double x), 
                               double  Gamma) : DenseLayer(0, dimI, dimO, 1.0, Activ, dActiv, Gamma) 
{
    example = NULL;
}

OpenDenseLayer::~OpenDenseLayer(){}

void OpenDenseLayer::setExample(double* example_ptr)
{
    example = example_ptr;
}

void OpenDenseLayer::applyFWD(double* state) 
{
   /* affine transformation */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Apply weights */
      update[io] = vecdot(dim_In, &(weights[io*dim_In]), example);

      /* Add bias */
      update[io] += bias[0];
   }

   /* Step */
   for (int io = 0; io < dim_Out; io++)
   {
      state[io] = activation(update[io]);
   }
}

void OpenDenseLayer::applyBWD(double* state,
                              double* state_bar)
{
   /* Derivative of step */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Recompute affine transformation */
      update[io]  = vecdot(dim_In, &(weights[io*dim_In]), example);
      update[io] += bias[0];

      /* Derivative */
      update_bar[io] = dactivation(update[io]) * state_bar[io];
      state_bar[io] = 0.0;
   }

   /* Derivative of affine transformation */
   for (int io = 0; io < dim_Out; io++)
   {
      /* Derivative of bias addition */
      bias_bar[0] += update_bar[io];

      /* Derivative of weight application */
      for (int ii = 0; ii < dim_In; ii++)
      {
         weights_bar[io*dim_In + ii] += example[ii] * update_bar[io];
      }
   }
}                


OpenExpandZero::OpenExpandZero(int dimI,
                               int dimO) : Layer(0, dimI, dimO, 1)
{
    /* this layer doesn't have any design variables. */ 
    ndesign = 0;
}


OpenExpandZero::~OpenExpandZero(){}


void OpenExpandZero::setExample(double* example_ptr)
{
    example = example_ptr;
}


void OpenExpandZero::applyFWD(double* state)
{
   for (int ii = 0; ii < dim_In; ii++)
   {
      state[ii] = example[ii];
   }
   for (int io = dim_In; io < dim_Out; io++)
   {
      state[io] = 0.0;
   }
}                           

void OpenExpandZero::applyBWD(double* state,
                              double* state_bar)
{
   for (int ii = 0; ii < dim_Out; ii++)
   {
      state_bar[ii] = 0.0;
   }
}                           


ClassificationLayer::ClassificationLayer(int    idx,
                                         int    dimI,
                                         int    dimO,
                                         double Gamma) : Layer(idx, dimI, dimO, dimO)
{
    gamma = Gamma;
    /* Allocate the probability vector */
    probability = new double[dimO];
}

ClassificationLayer::~ClassificationLayer()
{
    delete [] probability;
}


void ClassificationLayer::applyFWD(double* state)
{
    /* Compute affine transformation */
    for (int io = 0; io < dim_Out; io++)
    {
        /* Apply weights */
        update[io] = vecdot(dim_In, &(weights[io*dim_In]), state);
        /* Add bias */
        update[io] += bias[io];
    }

    /* Data normalization y - max(y) (needed for stable softmax evaluation */
    normalize(update);

    if (dim_In < dim_Out)
    {
        printf("Error: nchannels < nclasses. Implementation of classification layer doesn't support this setting. Change! \n");
        exit(1);
    }

    /* Apply step */
    for (int io = 0; io < dim_Out; io++)
    {
        state[io] = update[io];
    }
    /* Set remaining to zero */
    for (int ii = dim_Out; ii < dim_In; ii++)
    {
        state[ii] = 0.0;
    }
}                           
      
void ClassificationLayer::applyBWD(double* state,
                                   double* state_bar)
{
    /* Recompute affine transformation */
    for (int io = 0; io < dim_Out; io++)
    {
        update[io] = vecdot(dim_In, &(weights[io*dim_In]), state);
        update[io] += bias[io];
    }        


    /* Derivative of step */
    for (int ii = dim_Out; ii < dim_In; ii++)
    {
        state_bar[ii] = 0.0;
    }
    for (int io = 0; io < dim_Out; io++)
    {
        update_bar[io] = state_bar[io];
        state_bar[io]  = 0.0;
    }
    
    /* Derivative of the normalization */
    normalize_diff(update, update_bar);

    /* Derivatie of affine transformation */
    for (int io = 0; io < dim_Out; io++)
    {
       /* Derivative of bias addition */
        bias_bar[io] += update_bar[io];
  
        /* Derivative of weight application */
        for (int ii = 0; ii < dim_In; ii++)
        {
           weights_bar[io*dim_In + ii] += state[ii] * update_bar[io];
           state_bar[ii] += weights[io*dim_In + ii] * update_bar[io];
        }
    }   
}


void ClassificationLayer::normalize(double* data)
{

   /* Find maximum value */
   double max = vecmax(dim_Out, data);
   /* Shift the data vector */
   for (int io = 0; io < dim_Out; io++)
   {
       data[io] = data[io] - max;
   }
}   

void ClassificationLayer::normalize_diff(double* data, 
                                         double* data_bar)
{
    double max_b = 0.0;
    /* Derivative of the shift */
    for (int io = 0; io < dim_Out; io++)
    {
        max_b -= data_bar[io];
    }
    /* Derivative of the vecmax */
    int i_max = argvecmax(dim_Out, data);
    data_bar[i_max] += max_b;
}                                     

double ClassificationLayer::evalLoss(double *data_Out, 
                                      double *label) 
{
   double label_pr, exp_sum;
   double CELoss;

   /* Label projection */
   label_pr = vecdot(dim_Out, label, data_Out);

   /* Compute sum_i (exp(x_i)) */
   exp_sum = 0.0;
   for (int io = 0; io < dim_Out; io++)
   {
      exp_sum += exp(data_Out[io]);
   }

   /* Cross entropy loss function */
   CELoss = - label_pr + log(exp_sum);

   return CELoss;
}
      
      
void ClassificationLayer::evalLoss_diff(double *data_Out, 
                                        double *data_Out_bar,
                                        double *label,
                                        double  loss_bar)
{
    double exp_sum, exp_sum_bar;
    double label_pr_bar = - loss_bar;

    /* Recompute exp_sum */
    exp_sum = 0.0;
    for (int io = 0; io < dim_Out; io++)
    {
       exp_sum += exp(data_Out[io]);
    }

    /* derivative of log(exp_sum) */
    exp_sum_bar  = 1./exp_sum * loss_bar;
    for (int io = 0; io < dim_Out; io++)
    {
        data_Out_bar[io] = exp(data_Out[io]) * exp_sum_bar;
    }

    /* Derivative of vecdot */
    for (int io = 0; io < dim_Out; io++)
    {
        data_Out_bar[io] +=  label[io] * label_pr_bar;
    }
}                              


int ClassificationLayer::prediction(double* data_Out, 
                                    double* label)
{
   double exp_sum, max;
   int    class_id;
   int    success = 0;

   /* Compute sum_i (exp(x_i)) */
   max = -1.0;
   exp_sum = 0.0;
   for (int io = 0; io < dim_Out; io++)
   {
      exp_sum += exp(data_Out[io]);
   }

   for (int io = 0; io < dim_Out; io++)
   {
       /* Compute class probabilities (Softmax) */
       probability[io] = exp(data_Out[io]) / exp_sum;

      /* Predicted class is the one with maximum probability */ 
      if (probability[io] > max)
      {
          max      = probability[io]; 
          class_id = io; 
      }
   }

  /* Test for successful prediction */
  if ( label[class_id] > 0.99 )  
  {
      success = 1;
  }
   

   return success;
}


ConvLayer::ConvLayer(int     idx,
                     int     dimI,
                     int     dimO,
                     int     csize_in,
                     int     nconv_in,
                     double  deltaT,
                     double (*Activ)(double x),
                     double (*dActiv)(double x),
                     double  Gamma) : Layer(idx, dimI, dimO, 1, deltaT, Activ, dActiv, Gamma)
{
   csize = csize_in;
   nconv = nconv_in;
   ndesign = csize*csize*nconv;
}
   
ConvLayer::~ConvLayer() {}

double ConvLayer::apply_conv(double* state, int i, int j, int k, int img_size_sqrt,bool transpose)
{
   double val = 0.0;
   int idx = i*img_size_sqrt*img_size_sqrt + j*img_size_sqrt + k;
   int fcsize = floor(csize/2.0);
   
   for(int s = -fcsize; s <= fcsize; s++)
   {
      for(int t = -fcsize; t <= fcsize; t++)
      {
         int offset = s*img_size_sqrt + t;
         int wght_idx =  transpose  
                       ? i*csize*csize + (t+fcsize)*csize + (s+fcsize)
                       : i*csize*csize + (s+fcsize)*csize + (t+fcsize);
         if( ((i+s) >= 0) && ((i+s) < img_size_sqrt) && ((j+t) >= 0) && ((j+t) < img_size_sqrt))
         {
            val += state[idx + offset]*weights[wght_idx];
         }
      }
   }
   return val;
}

void ConvLayer::applyFWD(double* state)
{
   /* Affine transformation */
   int img_size = dim_In / nconv;
   int img_size_sqrt = round(sqrt(img_size));

   for(int i = 0; i < nconv; i++)
   {
      for(int j = 0; j < img_size_sqrt; j++)
      {
         for(int k = 0; k < img_size_sqrt; k++)
         {
             update[i*img_size + j*img_size_sqrt + k] = apply_conv(state, i, j, k, img_size_sqrt, false) + bias[0];
             
         }
      }
   }

   /* Apply step */
   for (int io = 0; io < dim_Out; io++)
   {
      state[io] = state[io] + dt * activation(update[io]);
   }
}


void ConvLayer::applyBWD(double* state,
                         double* state_bar)
{
   /* state_bar is the adjoint of the state variable, it contains the 
      old time adjoint informationk, and is modified on the way out to
      contain the update. */

   /* Okay, for my own clarity:
      state       = forward state solution
      state_bar   = backward adjoint solution (in - new time, out - current time)
      update_bar  = update to the bacward solution, this is "double dipped" in that
                    it is used to compute the weight and bias derivative.
                    Note that because this is written as a forward update (the
                    residual is F = u_{n+1} - u_n - dt * sigma(W_n * u_n + b_n)               
                    the adjoint variable is also the derivative of the objective
                    with respect to the solution. 
      weights_bar = Derivative of the objective with respect to the weights
      bias_bar    = Derivative of the objective with respect to the bias

  
      More details: Assume that the objective is 'g', and the constraint in
      residual form is F(u,W). Then
 
        d_{W_n} g = \partial_{u} g * \partial_{W_n} u

      Note that $\partial_{u} g$ only depends on the final layer. Expanding 
      around the constraint then gives
 
        d_{W_n} g = \partial_{u} g * (\partial_{u} F)^{-1} * \partial_{W_n} F

      and now doing the standard adjoint thing we get
      
        d_{W_n} g = (\partial_{u} F)^{-T} * \partial_{u} g ) * \partial_{W_n} F
 
      yielding
         
        d_{W_n} g = state_bar * \partial_{W_n} F

      This is directly 

        weights_bar = state_bar * \partial_{W_n} F

      computed below. Similar for the bias. 
    */

   /* Affine transformation, and derivative of time step */
   int img_size = dim_In / nconv;
   int img_size_sqrt = round(sqrt(img_size));

   /* loop over number convolutions */
   for(int i = 0; i < nconv; i++)
   {
      /* loop over full image */
      for(int j = 0; j < img_size_sqrt; j++)
      {
         for(int k = 0; k < img_size_sqrt; k++)
         {
             int m = i*img_size + j*img_size_sqrt + k;

             /* compute the affine transformation */
             update[m]     = apply_conv(state, i, j, k, img_size_sqrt,true) + bias[0];

             /* derivative of the update, this is the contribution from old time */
             update_bar[m] = dt * dactivation(update[m]) * state_bar[m];
         }
      }
   }

   /* Loop over the output dimensions */

     /* Loop over the input dimensions */

}

