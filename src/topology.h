// TODO Currently Assumes SMP Ordering 
//      And equal number of processes per node

#ifndef MPI_ADVANCE_TOPOLOGY_H
#define MPI_ADVANCE_TOPOLOGY_H

#include <mpi.h>
#include "dist_graph.h"

int get_node(MPIX_Comm* data, const int proc);
int get_local_proc(MPIX_Comm* data, const int proc);
int get_global_proc(MPIX_Comm* data, const int node, const int local_proc);


#endif
