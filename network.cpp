#include "network.hpp"

Network::Network()
{
   nlayers   = 0;
   nchannels = 0;
   layers    = NULL;
   endlayer  = NULL;
}

Network::Network(int    nLayers,
                 int    nChannels, 
                 int    nFeatures,
                 int    nClasses,
                 int    Activation,
                 double deltaT,
                 double Weight_init,
                 double Weight_open_init,
                 double Classification_init)
{
   nlayers   = nLayers;
   nchannels = nChannels;
   nclasses  = nClasses;
   
   double (*activ_ptr)(double x);
   double (*dactiv_ptr)(double x);

   /* Set the activation function */
   switch ( Activation )
   {
      case RELU:
          activ_ptr  = &Network::ReLu_act;
          dactiv_ptr = &Network::dReLu_act;
         break;
      case TANH:
         activ_ptr  = &Network::tanh_act;
         dactiv_ptr = &Network::dtanh_act;
         break;
      default:
         printf("ERROR: You should specify an activation function!\n");
         printf("GO HOME AND GET SOME SLEEP!");
   }

   /* Create and initialize the opening layer */
   if (Weight_open_init == 0.0)
   {
      openlayer = new OpenExpandZero(nFeatures, nChannels, deltaT);
   }
   else
   {
      openlayer = new DenseLayer(0, nFeatures, nChannels, deltaT, activ_ptr, dactiv_ptr);
   }
   openlayer->initialize(Weight_open_init);

   /* Create and initialize the intermediate layers */
   layers = new Layer*[nlayers-2];
   for (int ilayer = 1; ilayer < nlayers-1; ilayer++)
   {
      layers[ilayer-1] = new DenseLayer(ilayer, nChannels, nChannels, deltaT, activ_ptr, dactiv_ptr);
      layers[ilayer-1]->initialize(Weight_init);
   }

   /* Create and initialize the end layer */
   endlayer = new ClassificationLayer(nLayers, nChannels, nClasses, deltaT);
   endlayer->initialize(Classification_init);
}              

Network::~Network()
{
    /* Delete the layers */
    delete openlayer;
    for (int ilayer = 1; ilayer < nlayers-1; ilayer++)
    {
       delete layers[ilayer-1];
    }
    delete [] layers;
    delete endlayer;
}

void Network::applyFWD(int      nexamples,
                       double **examples,
                       double **labels)
{
    double* state       = new double[nchannels];
    double* state_old   = new double[nchannels];
    double* state_final = new double[nclasses];

    /* Propagate the example */
    for (int iex = 0; iex < nexamples; iex++)
    {
        /* Map data onto the network width */
        printf("Openinglayer\n");
        openlayer->applyFWD(examples[iex], state);

        for (int ilayer = 1; ilayer < nlayers-1; ilayer++)
        {
            /* Store old state */
            for (int ichannels = 0; ichannels < nchannels; ichannels++)
            {
                state_old[ichannels] = state[ichannels];
            } 
            /* Apply the layer */
            printf("layer %d\n", ilayer);
            layers[ilayer-1]->applyFWD(state_old, state);

        }
        /* classification */
        printf("endlayer\n");
        endlayer->applyFWD(state, state_final);
    }

    delete [] state;
    delete [] state_old;
    delete [] state_final;
}

double Network::ReLu_act(double x)
{
    return std::max(0.0, x);
}


double Network::dReLu_act(double x)
{
    double diff;
    if (x > 0.0) diff = 1.0;
    else         diff = 0.0;

    return diff;
}


double Network::tanh_act(double x)
{
    return tanh(x);
}

double Network::dtanh_act(double x)
{
    double diff = 1.0 - pow(tanh(x),2);

    return diff;
}