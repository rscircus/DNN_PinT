#include <assert.h>
#include "util.hpp"
#include "defs.hpp"
#include "config.hpp"
#include <mpi.h>
#pragma once

class DataSet {


   protected:

      int nelements;         /* Number of data elements */
      int nfeatures;         /* Number of features per element */
      int nlabels;           /* Number of different labels (i.e. classes) per element */

      int epochiter;           /* Number of epochs that have been passed through */
      int batchindex;      /* Current batch index */
      
      MyReal **examples;    /* Array of Feature vectors (dim: nelements x nfeatures) */
      MyReal **labels;      /* Array of Label vectors (dim: nelements x nlabels) */

      int *allIDs;          /* Array of all element IDs */
      int  nbatch;          /* Size of the batch */
      int *batchIDs;        /* Pointer to the current batch indicees */
      int  batch_type;      /* Deterministic or stochastic batch selection */

      int MPIsize;           /* Size of the global communicator */
      int MPIrank;           /* Processors rank */

   public: 

      /* Default constructor */
      DataSet();

      /* Destructor */
      ~DataSet();

      void initialize(int      nElements, 
                      int      nFeatures, 
                      int      nLabels,
                      int      nBatch,
                      int      batchType,
                      MPI_Comm Comm);


      /* Return the batch size*/
      int getnBatch();

      /* Return number of passed epochs */
      int getEpochIter();

      /* Print current batchIDs */
      void printBatch();

      /* Return the feature vector of a certain batchID. If not stored on this processor, return NULL */
      MyReal* getExample(int id);

      /* Return the label vector of a certain batchID. If not stored on this processor, return NULL */
      MyReal* getLabel(int id);

      /* Read data from file */
      void readData(char* datafolder,
                    char* examplefile,
                    char* labelfile);

      /* Select the current batch from all available IDs, either deterministic or stochastic */
      void selectBatch(int      iterindex,
                       MPI_Comm comm);


      /* Shuffle the indices */
      void shuffle(MPI_Comm Comm);

};