/*-----------------------
  Author: Anirban166/Ani
  Email:  ac4743@nau.edu
------------------------*/
#include <omp.h>
#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Function to import the data set:
int importDataSet(char *fileName, int lineCount, double **dataSet)
{
    FILE *filePointer = fopen(fileName, "r");
    if(!filePointer) 
    {
        fprintf(stderr, "Unable to open the data set file!\n");
        return(1);
    }
    char buffer[4096];
    int rowCount = 0;
    int columnCount = 0;
    while(fgets(buffer, 4096, filePointer) && rowCount < lineCount) 
    {
        columnCount = 0;
        char *field = strtok(buffer, ",");
        double temp;
        sscanf(field, "%lf", &temp);
        dataSet[rowCount][columnCount] = temp;
        while(field) 
        {
          columnCount++;
          field = strtok(NULL, ",");
          if(field != NULL)
          {
            double temp;
            sscanf(field, "%lf", &temp);
            dataSet[rowCount][columnCount] = temp;
          }   
        }
        rowCount++;
    }
    fclose(filePointer);
    return 0;
}

int main(int argc, char *argv[]) 
{
  // Initializing the MPI execution environment along with the ranks, the count of them and the desired level of thread support:
  int rank, size, provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Pointers for the data set and the distance matrix:
  double **dataSet, **distanceMatrix; 

  // Processing command-line arguments:
  int N, DIM, tileSize, threadCount;
  char inputFileName[500];
  if(argc != 6) 
  {
    fprintf(stderr, "\nPlease provide the following arguments in order via the command line:\nNumber of lines in the file\nDimensionality (Number of coordinates per point)\nTile size\nThread count\nFilename of the dataset\n");
    MPI_Finalize();
    exit(0);
  }
  sscanf(argv[1], "%d", &N);
  sscanf(argv[2], "%d", &DIM);
  sscanf(argv[3], "%d", &tileSize);
  sscanf(argv[4],"%d",&threadCount);
  omp_set_num_threads(threadCount);  
  strcpy(inputFileName, argv[5]);

  // Typical edge case:
  if(N < 1 || DIM < 1)
  {
    fprintf(stderr, "\nInput parameters are invalid!\n");
    MPI_Finalize();
    exit(0);
  }
  else
  {
    // if(!rank) fprintf(stdout, "\nNumber of lines: %d, Dimensionality: %d, Tile size: %d, Thread Count: %d, Filename: %s\n", N, DIM, tileSize, threadCount, inputFileName); 
    // Making all ranks import the dataset and then allocate memory for it:
    dataSet = (double**)malloc(sizeof(double*) * N);
    for(int i = 0; i < N; i++)
    {
      dataSet[i] = (double*)malloc(sizeof(double) * DIM);
    }
    if(importDataSet(inputFileName, N, dataSet))
    {
      MPI_Finalize();
      return 0;
    }
  }
  
  // Initializing variables to hold the timings and make rank 0 collect the start time:
  double startTime, endTime;
  if(!rank) startTime = MPI_Wtime();

  // Initializing variables to hold the range for each rank: (one for sending, one for receiving)
  int *range, *localRange;
  // Allocating memory for and initializing the entire range:
  range = (int *)malloc(sizeof(int) * N);
  if(!rank) 
  {
    for (int i = 0; i < N; ++i) 
      range[i] = i;
  }
  // Assigning row size to each process rank based on the divisibility of the data set size to the number of ranks:
  int localRowSize = N / size;
  // Taking care of the special case for the last rank, and assigning memory for all of them:
  if(rank == (size - 1) && (N % size) != 0) 
  {
    localRowSize = N / size + N % size;
    localRange = (int *)malloc(sizeof(int) * localRowSize);
  } 
  else localRange = (int *)malloc(sizeof(int) * localRowSize);

  // Sending the range to work on for a rank (local to it), distributed accordingly (in order) among all the ranks using a scatter:
  int workloadSize = N / size;  
  MPI_Scatter(range, workloadSize, MPI_INT, localRange, workloadSize, MPI_INT, 0, MPI_COMM_WORLD);
  // Increasing the size of the range for the last rank if not sufficient (for the leftover rows, if N doesn't divide size evenly):
  if(rank == (size - 1) && (N % size) != 0) 
  {
    for(int i = workloadSize; i < localRowSize; i++) 
      localRange[i] = localRange[i - 1] + 1;
  }

  // Allocating memory for and initializing the distance matrix:
  distanceMatrix = (double **)malloc(sizeof(double *) * localRowSize); // Ranks allocate memory only for their portion of the distance matrix.
  for(int i = 0; i < localRowSize; i++) 
  {
    distanceMatrix[i] = (double *)malloc(sizeof(double) * N);
  }
  int stepSize = (localRowSize < tileSize) ? localRowSize : tileSize;

  // Computing the distance matrix in parallel using a tiled approach: 
  int localIndex; double distance;
  #pragma omp parallel for shared(distanceMatrix, N, DIM, dataSet) private(localIndex) reduction(+:distance)
  for(int x = 0; x < localRowSize; x += stepSize)
  {
    for(int y = 0; y < N; y += tileSize) 
    {
      for(int i = x; i < (x + stepSize) && i < localRowSize; i++) 
      {
        for(int j = y; j < (y + tileSize) && j < N; j++) 
        {
          distance = 0;
          for (int k = 0; k < DIM; k++) 
          {
            localIndex = localRange[i];
            distance += (dataSet[localIndex][k] - dataSet[j][k]) * (dataSet[localIndex][k] - dataSet[j][k]);
          }
          distanceMatrix[i][j] = sqrt(distance);
        }
      }
    }
  }

  // Printing the elapsed time for the distance matrix computation:
  if(!rank) 
  {
    endTime = MPI_Wtime();
    fprintf(stdout, "Time taken to compute the distance matrix in parallel: %f seconds\n", endTime - startTime);   
  }

  // Computing the local sum in all ranks and then sending those to rank 0 for a reduction on it:
  double globalSum, localSum = 0;  
  for(int i = 0; i < localRowSize; i++) 
  {
    for(int j = 0; j < N; j++) 
      localSum += distanceMatrix[i][j];
  }
  MPI_Reduce(&localSum, &globalSum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  if(!rank) 
  { // Sanity check:
    fprintf(stdout, "Sum of all the distances computed: %f\n", globalSum);
  }

  // Deallocating memory for the data set, distance matrix and range variables:
  free(range);
  free(localRange);  
  for(int i = 0; i < N; i++)
  {
    free(dataSet[i]);
  }  
  free(dataSet);
  for (int i = 0; i < localRowSize; i++) 
  {
    free(distanceMatrix[i]);
  }  
  free(distanceMatrix);
  
  MPI_Finalize();
  return 0;
}