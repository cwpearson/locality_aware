#ifndef MPI_ADVANCE_COLLECTIVES_H
#define MPI_ADVANCE_COLLECTIVES_H

#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
//#include <mpt.h>
#include "utils.h"
#include "allgather.h"

#ifdef __cplusplus
extern "C"
{
#endif

int PMPI_Alltoall(const void* sendbuf,
        const int sendcount,
        MPI_Datatype sendtype,
        void* recvbuf,
        const int recvcount,
        MPI_Datatype recvtype,
        MPI_Comm comm);

int PMPI_Alltoallv(const void* sendbuf,
        const int sendcounts[],
        const int sdispls[],
        MPI_Datatype sendtype,
        void* recvbuf,
        const int recvcounts[],
        const int rdispls[],
        MPI_Datatype recvtype,
        MPI_Comm comm);

int PMPI_Allgather(const void* sendbuf,
        int sendcount,
        MPI_Datatype sendtype,
        void* recvbuf,
        int recvcount,
        MPI_Datatype recvtype,
        MPI_Comm comm);

#ifdef __cplusplus
}
#endif


#endif
