#include "collective.h"

/**************************************************
 * Locality-Aware Point-to-Point Alltoallv
 * Same as PMPI_Alltoall (no load balancing)
 *  - Aggregates messages locally to reduce 
 *      non-local communciation
 *  - First redistributes on-node so that each
 *      process holds all data for a subset
 *      of other nodes
 *  - Then, performs inter-node communication
 *      during which each process exchanges
 *      data with their assigned subset of nodes
 *  - Finally, redistribute received data
 *      on-node so that each process holds
 *      the correct final data
 *  - To be used when sizes are relatively balanced
 *  - For load balacing, use persistent version
 *      - Load balacing is too expensive for 
 *          non-persistent Alltoallv
 *************************************************/
int MPIX_Alltoallv(const void* sendbuf,
        const int sendcounts[],
        const int sdispls[],
        MPI_Datatype sendtype,
        void* recvbuf,
        const int recvcounts[],
        const int rdispls[],
        MPI_Datatype recvtype,
        MPI_Comm comm)
{    
    MPIX_Comm* mpi_comm;
    MPIX_Comm_init(&mpi_comm, comm);

    int rank, num_procs;
    MPI_Comm_rank(mpi_comm->global_comm, &rank);
    MPI_Comm_size(mpi_comm->global_comm, &num_procs);

    // Create shared-memory (local) communicator
    int local_rank, PPN;
    MPI_Comm_rank(mpi_comm->local_comm, &local_rank);
    MPI_Comm_size(mpi_comm->local_comm, &PPN);

    // Calculate shared-memory (local) variables
    int num_nodes = num_procs / PPN;
    int local_node = rank / PPN;

    // Local rank x sends to nodes [x:x/PPN] etc
    // Which local rank from these nodes sends to my node?
    // E.g. if local_rank 0 sends to nodes 0,1,2 and 
    // my rank is on node 2, I want to receive from local_rank 0
    // regardless of the node I talk to
    int local_idx = -1;

    const char* send_buffer = (char*) sendbuf;
    char* recv_buffer = (char*) recvbuf;
    int send_size, recv_size;
    MPI_Type_size(sendtype, &send_size);
    MPI_Type_size(recvtype, &recv_size);

    /************************************************
     * Setup : determine message sizes and displs for
     *      intermediate (aggregated) steps
     ************************************************/
    int proc, node;
    int tag = 923812;
    int local_tag = 728401;
    int start, end;
    int ctr, next_ctr;

    int num_msgs = num_nodes / PPN; // TODO : this includes talking to self
    int extra = num_nodes % PPN;
    int local_num_msgs = num_msgs;
    if (local_rank < extra) local_num_msgs++;

    int send_msg_size = sendcount*PPN;
    int recv_msg_size = recvcount*PPN;
    int* local_send_displs = (int*)malloc((PPN+1)*sizeof(int));
    local_send_displs[0] = 0;
    for (int i = 0; i < PPN; i++)
    {
        ctr = num_msgs;
        if (i < extra) ctr++;
        local_send_displs[i+1] = local_send_displs[i] + ctr;
        if (local_send_displs[i] <= local_node && local_send_displs[i+1] > local_node)
            local_idx = i;
    }
    int first_msg = local_send_displs[local_rank];
    int n_msgs;

    int bufsize = (num_msgs+1)*recv_msg_size*PPN*recv_size;
    char* tmpbuf = (char*)malloc(bufsize*sizeof(char));
    char* contig_buf = (char*)malloc(bufsize*sizeof(char));
    MPI_Request* local_requests = (MPI_Request*)malloc(2*PPN*sizeof(MPI_Request));
    MPI_Request* nonlocal_requests = (MPI_Request*)malloc(2*local_num_msgs*sizeof(MPI_Request));

     /************************************************
     * Step 1 : local Alltoall
     *      Redistribute data so that local rank x holds
     *      all data that needs to be send to any
     *      node with which local rank x communicates
     ************************************************/
    // Alltoall to distribute message sizes 
    int* node_sizes = (int*)malloc(num_nodes*sizeof(int));
    for (int i = 0; i < num_nodes; i++)
    {
        size = 0;
        for (int j = 0; j < PPN; j++)
        {
            size += sendcounts[j];
        }
        node_sizes[i] = size;
    }
    int* local_send_sizes = (int*)malloc(PPN*sizeof(int));
    int* local_recv_sizes = (int*)malloc(PPN*sizeof(int));
    int* local_recv_displs = (int*)malloc((PPN+1)*sizeof(int));
    local_send_displs[0] = 0;
    local_recv_displs[0] = 0;
    for (int i = 0; i < PPN; i++)
    {
        start = local_send_displs[i];
        end = local_send_displs[i+1];
        local_send_sizes[i] = end - start;
        local_recv_sizes[i] = local_num_msgs;
        local_recv_displs[i+1] = local_recv_displs[i] + local_recv_sizes[i];
    }
    int* local_S_recv_node_sizes = (int*)malloc(local_num_msgs*PPN*sizeof(int));
    MPI_Alltoallv(node_sizes,
            local_send_sizes,
            local_send_displs,
            MPI_INT,
            local_S_recv_node_sizes, 
            local_recv_sizes,
            local_recv_displs,
            MPI_INT, 
            mpi_comm->local_comm);




    n_msgs = 0;
    ctr = 0;
    for (int i = 0; i < PPN; i++)
    {
        start = local_send_displs[i];
        end = local_send_displs[i+1];
        size = 0;
        for (int node = start; node < end; node++)
        {
            size += node_sizes[node];
        }
        if (size)
        {
            MPI_Isend(&(send_buffer[ctr*send_size]), 
                    size, 
                    sendtype, 
                    i, 
                    tag, 
                    mpi_comm->local_comm, 
                    &(local_requests[n_msgs++]));
        }
        ctr += size;
    }

    ctr = 0;
    for (int i = 0; i < PPN; i++)
    {
        size = 0;
        for (int j = 0; j < local_num_msgs; j++)
        {
            size += local_S_recv_node_sizes[j];
        }
        if (local_num_msgs)
        {
            MPI_Irecv(&(tmpbuf[ctr*send_size]), 
                    size, 
                    sendtype,
                    i, 
                    tag, 
                    mpi_comm->local_comm, 
                    &(local_requests[n_msgs++]));
        }
    }
    if (n_msgs)
        MPI_Waitall(n_msgs, local_requests, MPI_STATUSES_IGNORE);


    free(node_sizes);
    free(local_send_sizes);
    free(local_recv_sizes);
    free(local_recv_displs);

     /************************************************
     * Step 2 : non-local Alltoall
     *      Local rank x exchanges data with 
     *      local rank x on nodes x, PPN+x, 2PPN+x, etc
     ************************************************/
    int* local_S_recv_displs = (int*)malloc((PPN+1)*sizeof(int));
    local_S_recv_displs[0] = 0;
    for (int i = 0; i < PPN; i++)
    {
        size = 0;
        for (int j = 0; j < local_num_msgs; j++)
        {
            size += local_S_recv_node_sizes[i*local_num_msgs+j];
        }
        local_S_recv_displs[i+1] = local_S_recv_displs[i] + size;
    }

        
    free(local_S_recv_node_sizes);

    // update local_R_recv_displs as stepping through tmpbuf...
    ctr = 0;
    next_ctr = ctr;
    n_msgs = 0;
    for (int i = 0; i < local_num_msgs; i++)
    {
        node = first_msg + i;
        proc = node*PPN + local_idx;
        for (int j = 0; j < PPN; j++)
        {
            start = local_S_recv_displs[j];
            size = local_S_recv_node_sizes[j*PPN+i];
            for (int k = 0; k < size; k++)
            {
                for (int l = 0; l < send_size; l++)
                {
                    contig_buf[next_ctr*send_size+l] = tmpbuf[(start+k)*send_size + l];
                }
                next_ctr++;
            }
            local_S_recv_displs[j] = start + size;
        }
        if (next_ctr - ctr)
        {
            MPI_Isend(&(contig_buf[ctr*send_size]), 
                    next_ctr - ctr,
                    sendtype, 
                    proc, 
                    tag, 
                    mpi_comm->global_comm, 
                    &(nonlocal_requests[n_msgs++]));
        }
        ctr = next_ctr;
    }





    // Need to find sizes (opposite of send side performed above!)
    for (int i = 0; i < num_nodes; i++)
    {
        size = 0;
        for (int j = 0; j < PPN; j++)
        {
            size += recvcounts[j];
        }
        node_sizes[i] = size;
    }
    int* local_R_send_node_sizes = (int*)malloc(local_num_msgs*PPN*sizeof(int));
    MPI_Alltoallv(node_sizes,
            local_send_sizes,
            local_send_displs,
            MPI_INT,
            local_S_recv_node_sizes, 
            local_recv_sizes,
            local_recv_displs,
            MPI_INT, 
            mpi_comm->local_comm);

    int* local_R_send_displs = (int*)malloc((PPN+1)*sizeof(int));
    local_R_send_displs[0] = 0;
    for (int i = 0; i < local_num_msgs; i++)
    {
        size = 0;
        for (int j = 0; j < PPN; j++)
        {
            size += local_R_send_node_sizes[i*PPN+j];
        }
        local_R_send_displs[i+1] = local_R_send_displs[i] + size;
    }




    ctr = 0;
    for (int i = 0; i < local_num_msgs; i++)
    {
        node = first_msg + i;
        proc = node*PPN + local_idx;
        start = local_R_send_displs[i];
        end = local_R_send_displs[i+1];
        if (end - start)
        {
            MPI_Irecv(&(tmpbuf[ctr*recv_size]), 
                    end - start, 
                    recvtype, 
                    proc, 
                    tag,
                    mpi_comm->global_comm, 
                    &(nonlocal_requests[n_msgs++]));
        }
        ctr += (end - start);
    }
    if (n_msgs) 
        MPI_Waitall(n_msgs,
                nonlocal_requests,
                MPI_STATUSES_IGNORE);

     /************************************************
     * Step 3 : local Alltoall
     *      Locally redistribute all received data
     ************************************************/
    ctr = 0;
    next_ctr = ctr;
    n_msgs = 0;
    for (int i = 0; i < PPN; i++)
    {
        for (int j = 0; j < local_num_msgs; j++)
        {
            start = local_R_send_displs[k];
            for (int k = 0; k < PPN; k++)
            {
                size = local_R_send_node_sizes[j*PPN+k];
                for (int l = 0; l < size; l++)
                {
                    for (int m = 0; m < recv_size; m++)
                    {
                        contig_buf[next_ctr*recv_size+m] = tmpbuf[(start+l)*recv_size+m];
                    }
                    next_ctr++;
                }
            }
        }
        start = local_send_displs[i];
        end = local_send_displs[i+1];

        if (next_ctr - ctr)
        {
            MPI_Isend(&(contig_buf[ctr*recv_size]), 
                    next_ctr - ctr,
                    recvtype,
                    i,
                    local_tag,
                    mpi_comm->local_comm,
                    &(local_requests[n_msgs++]));
        }

        if (end - start)
        {
            MPI_Irecv(&(recv_buffer[(start*PPN*recvcount)*recv_size]), 
                    (end - start)*PPN*recvcount*recv_size,
                    recvtype, 
                    i, 
                    local_tag, 
                    mpi_comm->local_comm, 
                    &(local_requests[n_msgs++]));
        }
        ctr = next_ctr;
    }
    MPI_Waitall(n_msgs,
            local_requests, 
            MPI_STATUSES_IGNORE);
            

    free(local_send_displs);
    free(tmpbuf);
    free(contig_buf);
    free(local_requests);
    free(nonlocal_requests);

    MPIX_Comm_free(mpi_comm);

    return 0;
}
