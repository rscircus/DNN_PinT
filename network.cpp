#include "network.hpp"

Network::Network()
{
   nlayers_global = 0;
   nlayers_local  = 0;
   nchannels      = 0;
   dt             = 0.0;
   loss           = 0.0;
   accuracy       = 0.0;
   gamma_tik      = 0.0;
   gamma_ddt      = 0.0;
   gamma_class    = 0.0;
   gradient       = NULL;
   design         = NULL;
   layers         = NULL;
   layer_prev     = NULL;
}

Network::Network(int    nLayersGlobal,
                 int    nChannels, 
                 double deltaT,
                 double Gamma_tik, 
                 double Gamma_ddt,
                 double Gamma_class)
{

    /* Initilizize */
    nlayers_global   = nLayersGlobal;
    nchannels        = nChannels;
    dt               = deltaT;
    loss             = 0.0;
    accuracy         = 0.0;
    gamma_tik        = Gamma_tik;
    gamma_ddt        = Gamma_ddt;
    gamma_class      = Gamma_class;

}             

  



Network::~Network()
{
    /* Delete the layers */
    for (int ilayer = 0; ilayer < nlayers_local; ilayer++)
    {
        delete layers[ilayer];
    }
    delete [] layers;

    if (layer_prev != NULL)
    {
        delete [] layer_prev->getWeights();
        delete [] layer_prev->getWeightsBar();
        delete layer_prev;
    }

    delete [] design;
    delete [] gradient;
}

int Network::getnChannels() { return nchannels; }

int Network::getnLayers() { return nlayers_global; }

double Network::getDT() { return dt; }

int Network::getLocalID(int ilayer) 
{
    int idx = ilayer - startlayerID;
    if (idx < 0 || idx > nlayers_global-1) 
    {
           printf("\n\nERROR! Something went wrong with local storage of layers! \n");
           printf("ilayer %d, startlayerID %d\n\n", ilayer, startlayerID);
    }

    return idx;
}

double Network::getLoss() { return loss; }

double Network::getAccuracy() { return accuracy; }

int Network::getnDesign() { return ndesign; }

double* Network::getDesign() { return design; }
       
double* Network::getGradient() { return gradient; }


Layer* Network::createLayer(int    ilayer, 
                            int    nFeatures,
                            int    nClasses,
                            int    Activation,
                            double Gamma_tik,
                            double Gamma_ddt,
                            double Gamma_class,
                            double Weight_open_init)
{
    Layer* layer;
    if (ilayer == 0)  // Opening layer
        {
            if (Weight_open_init == 0.0)
            {
               layer  = new OpenExpandZero(nFeatures, nchannels);
            }
            else
            {
               layer = new OpenDenseLayer(nFeatures, nchannels, Activation, Gamma_tik);
            }
        }
        else if (ilayer < nlayers_global-1) // Intermediate layer
        {
            layer = new DenseLayer(ilayer, nchannels, nchannels, dt, Activation, Gamma_tik);
        }
        else if (ilayer == nlayers_global-1) // Classification layer 
        {
            layer = new ClassificationLayer(ilayer, nchannels, nClasses, Gamma_class);
        }
        else
        {
            layer = NULL;
        }

    return layer;
}                        



void Network::initialize(int    StartLayerID, 
                           int    EndLayerID, 
                           int    nFeatures,
                           int    nClasses,
                           int    Activation,
                           double Weight_init,
                           double Weight_open_init,
                           double Classification_init)
{

    startlayerID = StartLayerID;
    endlayerID   = EndLayerID;
    nlayers_local = endlayerID - startlayerID + 1;


    /* Sanity check */
    if (nFeatures > nchannels ||
        nClasses  > nchannels)
    {
        printf("ERROR! Choose a wider netword!\n");
        exit(1);
    }

    printf("creating layers startid %d endid %d, nlayer_local %d\n", startlayerID, endlayerID, nlayers_local);

   /* --- Create the layers --- */
    layers  = new Layer*[nlayers_local];
    ndesign = 0;
    for (int ilayer = startlayerID; ilayer <= endlayerID; ilayer++)
    {
        /* Create a layer at time step ilayer. Local storage at ilayer - startlayerID */
        int storeID = getLocalID(ilayer);
        layers[storeID] = createLayer(ilayer, nFeatures, nClasses, Activation, gamma_tik, gamma_ddt, gamma_class, Weight_open_init);
        ndesign += layers[storeID]->getnDesign();
        
    }

    /* Allocate memory for network design and gradient variables */
    design   = new double[ndesign];
    gradient = new double[ndesign];

    /* Initialize  the layer weights and bias */
    int istart = 0;
    for (int ilayer = startlayerID; ilayer <= endlayerID; ilayer++)
    {
        int storeID = getLocalID(ilayer);
        if (ilayer == 0)  // Opening layer
        {
            layers[storeID]->initialize(&(design[istart]), &(gradient[istart]), Weight_open_init);
            istart += layers[storeID]->getnDesign();
        }
        else if (ilayer < nlayers_global-1) // Intermediate layer
        {
            layers[storeID]->initialize(&(design[istart]), &(gradient[istart]), Weight_init);
            istart += layers[storeID]->getnDesign();
        }
        else // Classification layer 
        {
            layers[storeID]->initialize(&(design[istart]), &(gradient[istart]), Classification_init);
        }
    }
}    


double Network::evalRegulDDT(Layer* layer_old, 
                             Layer* layer_curr)
{
    double diff;
    double ddt = 0.0;

    /* Sanity check */
    if (layer_old->getDimIn()    != nchannels ||
        layer_old->getDimOut()   != nchannels ||
        layer_old->getDimBias()  != 1         ||
        layer_curr->getDimIn()   != nchannels ||
        layer_curr->getDimOut()  != nchannels ||
        layer_curr->getDimBias() != 1           )
        {
            printf("ERROR when evaluating ddt-regularization of intermediate Layers.\n"); 
            printf("Dimensions don't match. Check and change this routine.\n");
            exit(1);
        }

    for (int iw = 0; iw < nchannels * nchannels; iw++)
    {
        diff = (layer_curr->getWeights()[iw] - layer_old->getWeights()[iw]) / dt;
        ddt += pow(diff,2);
    }
    diff = (layer_curr->getBias()[0] - layer_old->getBias()[0]) / dt;
    ddt += pow(diff,2);
    
    return gamma_ddt / 2.0 * ddt;
}                

void Network::evalRegulDDT_diff(Layer* layer_old, 
                                Layer* layer_curr,
                                double regul_bar)
{
    double diff;
    regul_bar = gamma_ddt * regul_bar;

    /* Derivative of the bias-term */
    diff = (layer_curr->getBias()[0] - layer_old->getBias()[0]) / pow(dt,2);
    layer_curr->getBiasBar()[0] += diff * regul_bar;
    layer_old->getBiasBar()[0]  -= diff * regul_bar;

    /* Derivative of the weights term */
    for (int iw = 0; iw < nchannels * nchannels; iw++)
    {
        diff = (layer_curr->getWeights()[iw] - layer_old->getWeights()[iw]) / pow(dt,2);
        layer_curr->getWeightsBar()[iw] += diff * regul_bar;
        layer_old->getWeightsBar()[iw]  -= diff * regul_bar;
    }
} 



void Network::MPI_RecvLayerNeighbours(MPI_Comm comm)
{
    int myid, comm_size;
    int idx;
    MPI_Comm_rank(comm, &myid);
    MPI_Comm_size(comm, &comm_size);
    MPI_Status status;

    Layer* recvlayer = layer_prev;

    /* Destroy currently stored layer */
    if (recvlayer != NULL)
    {
        delete [] recvlayer->getWeights();
        delete recvlayer;
        recvlayer = NULL;
    }

    /* Allocate buffers */
    int size           = (8 + (nchannels*nchannels+nchannels));
    double* sendbuffer = new double[size];
    double* recvbuffer = new double[size];

    /* All but the last processor send their last layer to the next neighbour on their right */
    if (myid < comm_size-1)
    {
        int lastlayerIDX = getLocalID(endlayerID);
        Layer* sendlayer = layers[lastlayerIDX];
        int nweights     = sendlayer->getnDesign() - sendlayer->getDimBias();
        int nbias        = sendlayer->getnDesign() - sendlayer->getDimIn() * sendlayer->getDimOut();

        /* Pack the layer into a buffer */

        idx = 0;
        sendbuffer[idx] = sendlayer->getType();       idx++;
        sendbuffer[idx] = sendlayer->getIndex();      idx++;
        sendbuffer[idx] = sendlayer->getDimIn();      idx++;
        sendbuffer[idx] = sendlayer->getDimOut();     idx++;
        sendbuffer[idx] = sendlayer->getDimBias();    idx++;
        sendbuffer[idx] = sendlayer->getActivation(); idx++;
        sendbuffer[idx] = sendlayer->getnDesign();    idx++;
        sendbuffer[idx] = sendlayer->getGamma();      idx++;
        for (int i = 0; i < nweights; i++)
        {
            sendbuffer[idx] = sendlayer->getWeights()[i];     idx++;
        }
        for (int i = 0; i < nbias; i++)
        {
            sendbuffer[idx] = sendlayer->getBias()[i];     idx++;
        }
        /* Set the rest to zero */
        for (int i = idx; i < size; i++)
        {
            sendbuffer[idx] = 0.0;  idx++;
        }

        /* Send the buffer */
        int receiver = myid + 1;
        MPI_Send(sendbuffer, size, MPI_DOUBLE, receiver, 0, comm);
    }

    
    /* All but the first processor receive a layer */
    if (myid > 0)
    {
        /* Receive the buffer */
        int sender = myid - 1;
        MPI_Recv(recvbuffer, size, MPI_DOUBLE, sender, 0, comm, &status);

        int idx = 0;
        int layertype = recvbuffer[idx];  idx++;
        int index     = recvbuffer[idx];  idx++;
        int dimIn     = recvbuffer[idx];  idx++;
        int dimOut    = recvbuffer[idx];  idx++;
        int dimBias   = recvbuffer[idx];  idx++;
        int activ     = recvbuffer[idx];  idx++;
        int nDesign   = recvbuffer[idx];  idx++;
        int gamma     = recvbuffer[idx];  idx++;
        /* Create a new layer */
        switch (layertype)
        {
            case Layer::OPENZERO:
                recvlayer = new OpenExpandZero(dimIn, dimOut);
                break;
            case Layer::OPENDENSE:
                recvlayer = new OpenDenseLayer(dimIn, dimOut, activ, gamma);
                break;
            case Layer::DENSE:
                recvlayer = new DenseLayer(index, dimIn, dimOut, 1.0, activ, gamma);
                break;
            case Layer::CLASSIFICATION:
                recvlayer = new ClassificationLayer(index, dimIn, dimOut, gamma);
                break;
            default: 
                printf("\n\n ERROR while unpacking a buffer: Layertype unknown!!\n\n"); 
        }
        double *design   = new double[nDesign];
        double *gradient = new double[nDesign];
        recvlayer->initialize(design, gradient, 0.0);

        int nweights     = nDesign - dimBias;
        int nbias        = nDesign - dimIn * dimOut;
        for (int i = 0; i < nweights; i++)
        {
            recvlayer->getWeights()[i]    = recvbuffer[idx]; idx++;
            recvlayer->getWeightsBar()[i] = 0.0; 
        }
        for (int i = 0; i < nbias; i++)
        {
            recvlayer->getBias()[i]    = recvbuffer[idx];   idx++;
            recvlayer->getBiasBar()[i] = 0.0;
        }
    }

    /* Free the buffer */
    delete [] sendbuffer;
    delete [] recvbuffer;

}