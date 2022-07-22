#include "topology.h"

int MPIX_Comm_init(MPIX_Comm** comm_dist_graph_ptr, MPI_Comm global_comm)
{
    int rank, num_procs;
    MPI_Comm_rank(global_comm, &rank);
    MPI_Comm_size(global_comm, &num_procs);

    MPIX_Comm* comm_dist_graph = (MPIX_Comm*)malloc(sizeof(MPIX_Comm));
    comm_dist_graph->global_comm = global_comm;

#ifdef LOCAL_COMM_PPN4
    MPI_Comm_split(comm_dist_graph->global_comm,
            rank/4,
            rank,
            &(comm_dist_graph->local_comm);
#else
    MPI_Comm_split_type(comm_dist_graph->global_comm,
        MPI_COMM_TYPE_SHARED,
        rank,
        MPI_INFO_NULL,
        &(comm_dist_graph->local_comm));
#endif

    MPI_Comm_size(comm_dist_graph->local_comm, &(comm_dist_graph->ppn));
    comm_dist_graph->num_nodes = ((num_procs-1) / comm_dist_graph->ppn) + 1;
    comm_dist_graph->rank_node = get_node(comm_dist_graph, rank);

    comm_dist_graph->neighbor_comm = MPI_COMM_NULL;
    
    *comm_dist_graph_ptr = comm_dist_graph;

    return 0;
}

int MPIX_Comm_free(MPIX_Comm* comm_dist_graph)
{
    if (comm_dist_graph->neighbor_comm != MPI_COMM_NULL)
        MPI_Comm_free(&(comm_dist_graph->neighbor_comm));
    MPI_Comm_free(&(comm_dist_graph->local_comm));

    free(comm_dist_graph);

    return 0;
}

int get_node(const MPIX_Comm* data, const int proc)
{
    return proc / data->ppn;
}

int get_local_proc(const MPIX_Comm* data, const int proc)
{
    return proc % data->ppn;
}

int get_global_proc(const MPIX_Comm* data, const int node, const int local_proc)
{
    return local_proc + (node * data->ppn);
}

