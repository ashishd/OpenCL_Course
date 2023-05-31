// Example of a global variable
#ifdef __opencl_c_program_scope_global_variables
__global int a_g = 2; 
__global float b_g[2] = {2.0,1.0}; 
#endif

// Example of constant memory
__constant float pi = 3.1415;
__constant float coeffs[] = {1.0, -2.0, 1.0};

// Kernel function to get the start and end values
// for filling a shared memory array
void get_start_end(
    // Number of work-items along a dimension of workgroup
    size_t local_length,
    // Number of items in the array
    size_t array_length,
    // Index of work item along dimension of workgroup
    size_t local_index,
    // Starting position of the copy
    size_t *start,
    // End position of the copy
    size_t *end) {
  
    // Work out the jump size
    size_t jump_size=array_length/local_length;
    if (array_length%local_length) jump_size++;
    
    // Starting position for the copy
    *start=local_index*jump_size;
    // End position for the copy
    *end=(local_index+1)*jump_size;
    // Limit end so we don't go off the end
    *end=min(*end,array_length);
}    

__kernel void c_star_stack (
                        __global float* C_star,
                        __global float* C,
                        unsigned int nchunks, 
                        unsigned int N0_C,
                        unsigned int N1_C) {    

    // C_star is of size (nchunks, N0_C, N1_C) (n, i0, i1)
    // C is of size (N0_C, N1_C) (i0, i1)
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); // Slowest dimension
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    
    // Temporary storage
    float temp=0.0f;
    
    __global float* C_i0 = &C_star[i0*N1_C+i1];

    // It's weird but it's fast
    for (int n=0; n<nchunks; n++) {
        temp+=C_i0[n*N0_C*N1_C];
    }
    
    // Place the accumulated sum into position
    C[i0*N1_C+i1]=temp;
}

// standard matrix multiply kernel 
__kernel void mat_mult_double (
                        __global double* A, 
                        __global double* B, 
                        __global double* C, 
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
            
    // C is of size (N0_C, N1_C)
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Scratch variable
    double temp=0.0;

    __global double* A_i0 = &A[i0*N1_A];
    __global double* B_i1 = &B[i1];

    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C 
    if ((i0<N0_C) && (i1<N1_C)) {
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            
            // Loop across row i0 of A
            // and down column i1 of B
            temp+=A_i0[n]*B_i1[n*N1_C]; 
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
} 

// standard matrix multiply kernel 
__kernel void mat_mult_float (__global float* A, 
                        __global float* B, 
                        __global float* C, 
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
            
    // C is of size (N0_C, N1_C)
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Scratch variable
    float temp=0.0f; 

    __global float* A_i0 = &A[i0*N1_A];
    __global float* B_i1 = &B[i1];
    
    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C 
    if ((i0<N0_C) && (i1<N1_C)) {
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            
            // Loop across row i0 of A
            // and down column i1 of B
            temp+=A_i0[n]*B_i1[n*N1_C]; 
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
} 

// matrix multiply kernel with pre-fetching
__kernel void mat_mult_prefetch (__global float* A, 
                        __global float* B, 
                        __global float* C, 
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
            
    // C is of size (N0_C, N1_C)
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Scratch variable
    float temp=0.0f;
    
    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C 
    if ((i0<N0_C) && (i1<N1_C)) {
        
        // Implement prefetching for A
        __global float* A_i0 = &A[i0*N1_A];
        __global float* B_i1 = &B[i1];
        prefetch(A_i0, (size_t)N1_A);
    
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            
            // Loop across row i0 of A
            // and down column i1 of B
            //temp+=A[i0*N1_A+n]*B[n*N1_C+i1];
            temp += A_i0[n]*B_i1[n*N1_C];
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
}

// Matrix multiply kernel that uses local memory for B
__kernel void mat_mult_local (
                        __global float* A, 
                        __global float* B, 
                        __global float* C,
                        __local  float* shared_B,
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
    
    // A is of size (N0_C, N1_A)
    // B is of size (N1_A, N1_C)
    // shared_B is of size (L1, N1_A)
    // C is of size (N0_C, N1_C)
    
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Location within the workgroup
    size_t s0=get_local_id(1);
    size_t s1=get_local_id(0);
    
    // Local size
    size_t L0=get_local_size(1);
    size_t L1=get_local_size(0);
    
    // start and end
    size_t start, end;
    
    // Fill shared_B
    
    // Get the start and end lengths
    get_start_end(L0, N1_A, s0, &start, &end);
    // Fill the columns of shared with B
    if (i1<N1_C) {
        for (size_t n=start; n<end; n++) {
            shared_B[s1*N1_A+n]=B[i1+n*N1_C]; 
        }
    }
    
    // Enqueue a local barrier to make sure all the work items finish
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // Scratch variable
    float temp=0.0f; 
    
    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C
    if ((i0<N0_C) && (i1<N1_C)) {
        
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            // shared_B is of size (L1, N1_A)
            // C is of size (N0_C, N1_C)
            
            // Loop across row i0 of A
            // and across row s1 of shared_B
            temp+=A[i0*N1_A+n]*shared_B[s1*N1_A+n]; 
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
}

// matrix multiply kernel with pre-fetching
__kernel void mat_mult_BT (__global float* A, 
                        __global float* BT, 
                        __global float* C, 
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
            
    // C is of size (N0_C, N1_C)
    // A is of size (N0_C, N1_A)
    // BT is of size (N1_C, N1_A)
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Scratch variable
    float temp=0.0f;
    
    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C 
    if ((i0<N0_C) && (i1<N1_C)) {
        
        // Implement prefetching for A
        __global float* A_i0 = &A[i0*N1_A];
        //prefetch(A_i0, (size_t)N1_A);

        __global float* BT_i1 = &BT[i1*N1_A];
        //prefetch(B_i1, (size_t)N1_A);
    
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            
            // Loop across row i0 of A
            // and across row i1 of B
            temp += A_i0[n]*BT_i1[n];
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
} 

// matrix multiply kernel with pre-fetching
__kernel void mat_mult_AT (__global float* AT, 
                        __global float* B, 
                        __global float* C, 
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C) { 
            
    // C is of size (N0_C, N1_C)
    // AT is of size (N1_A, N0_C)
    // B is of size (N1_A, N1_C)
    
    // i0 and i1 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    // Scratch variable
    float temp=0.0f;
    
    // Guard mechanism to make sure we do not go
    // outside the boundaries of matrix C 
    if ((i0<N0_C) && (i1<N1_C)) {
        
        // Implement prefetching for A
        __global float* AT_i0 = &AT[i0];
        //prefetch(A_i0, (size_t)N1_A);

        __global float* B_i1 = &B[i1];
        //prefetch(B_i1, (size_t)N1_A);
    
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<N1_A; n++) {
            
            // A is of size (N0_C, N1_A)
            // B is of size (N1_A, N1_C)
            
            // Loop across row i0 of A
            // and across row i1 of B
            temp += AT_i0[n*N0_C]*B_i1[n*N1_C];
        } 
        // Number of rows in C is same as number of rows in A
        C[i0*N1_C+i1]=temp;
    }
} 

// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_BT (
                        __global float* A_star, 
                        __global float* BT_star, 
                        __global float* C,
                        unsigned int N1_A_star, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int chunk_len,
                        unsigned int start_chunk_id,
                        unsigned int end_chunk_id) { 
    
    // A_star is of size (N0_C, N1_A_star), (i0, n)
    // BT_star is of size (N1_C, N1_A_star), (i1, n)
    // C is of size (N0_C, N1_C), (i0, i1)
    
    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); 

    // Scratch variable to accumulate the sum
    float temp1=0.0f, temp2=0.0f;

    // Loop over the chunks
    for (int chunk_id=start_chunk_id; chunk_id<end_chunk_id; chunk_id++) {

        // Starting positions for the copy
        __global float* A_star_i0 = &A_star[i0*N1_A_star+chunk_id*chunk_len];
        __global float* BT_star_i1 = &BT_star[i1*N1_A_star+chunk_id*chunk_len];
        
        temp1=0.0f;
        
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<chunk_len; n++) {
                
            // Loop across row i0 of A
            // and down column i1 of B
            temp1+=A_star_i0[n]*BT_star_i1[n];
        }
        
        temp2+=temp1;
    }

    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp2;
}

// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_vector_BT (
                        __global float8* A_star, 
                        __global float8* BT_star, 
                        __global float* C,
                        unsigned int N1_A_star, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int chunk_len,
                        unsigned int start_chunk_id,
                        unsigned int end_chunk_id) { 
    
    // A_star is of size (N0_C, N1_A_star), (i1, n)
    // BT_star is of size (N1_C, N1_A_star), (i2, n)
    // C is of size (N0_C, N1_C), (i0, i1)
    
    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); 
    
    // Scratch variable to accumulate the sum
    float8 temp1=(float8)0.0f, temp2=(float8)0.0f;

    // Loop over the chunks
    for (int chunk_id=start_chunk_id; chunk_id<end_chunk_id; chunk_id++) {

        // Fetch local memory into shared_A_star and shared_B_star
        
        // Starting positions for the copy
        __global float8* A_star_i0 = &A_star[i0*N1_A_star+chunk_id*chunk_len];
        __global float8* BT_star_i1 = &BT_star[i1*N1_A_star+chunk_id*chunk_len];
          
        temp1=(float8)0.0f;

        // Loop over the elements of a chunk 
        for (size_t n=0; n<chunk_len; n++) {
            temp1+=A_star_i0[n]*BT_star_i1[n];
        }

        // Accumulate the answer
        temp2+=temp1;   
    }

    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp2.s0 + temp2.s1 + temp2.s2 + temp2.s3
        + temp2.s4 + temp2.s5 + temp2.s6 + temp2.s7;
}


// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_AT (
                        __global float* AT_star, 
                        __global float* B_star, 
                        __global float* C,
                        unsigned int N1_A_star, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int chunk_len,
                        unsigned int start_chunk_id,
                        unsigned int end_chunk_id) { 
    
    // AT_star is of size (N1_A_star, N0_C), (n, i0)
    // B_star is of size (N1_A_star, N1_C), (n, i1)
    // C is of size (N0_C, N1_C), (i0, i1)
    
    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); 

    // Scratch variable to accumulate the sum
    float temp1, temp2=0.0f;

    // Loop over the chunks
    for (int chunk_id=start_chunk_id; chunk_id<end_chunk_id; chunk_id++) {

        // Starting positions for the copy
        __global float* AT_star_i0 = &AT_star[chunk_id*chunk_len*N1_A_star+i0];
        __global float* B_star_i1 = &B_star[chunk_id*chunk_len*N1_A_star+i1];
        
        temp1=0.0f;
        
        // Loop over columns of A and rows of B 
        for (size_t n=0; n<chunk_len; n++) {
                
            // Loop across row i0 of A
            // and down column i1 of B
            temp1+=AT_star_i0[n*N1_A_star]*B_star_i1[n*N1_A_star];
        }
        
        temp2+=temp1;
    }

    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp2;
}

// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_local_AT (
                        __global float* AT, 
                        __global float* B, 
                        __global float* C,
                        __local float* shared_AT,
                        __local float* shared_B,
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int nchunks,
                        unsigned int chunk_len) { 
    
    // AT is of size (N0_C, N1_A)
    // B is of size (N1_C, N1_A)
    // C is of size (N0_C, N1_C) 

    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1);

    // Fetch local memory into shared_AT and shared_B
    
    // Index within local memory
    size_t i0_AT = get_local_id(1); // Slowest dimension
    size_t i0_B = get_local_id(0); // fastest dimension
    
    // Start and stop chunks    
    size_t start_A, stop_A;
    size_t start_B, stop_B;

    // shared_AT is of size (L0, chunk_len) (s0, n)
    // shared_B is of size (L1, chunk_len) (s1, n)
    size_t L0 = get_local_size(1); // Slowest dimension
    size_t L1 = get_local_size(0); // Fastest dimension

    get_start_end(L1, chunk_len, i0_B, &start_A, &stop_A);
    get_start_end(L0, chunk_len, i0_AT, &start_B, &stop_B);

    // Scratch variable to accumulate the sum
    float temp=0.0f;

    for (size_t chunkId=0; chunkId<nchunks; chunkId++) {
    
        // Starting positions for the copy
        __global float* AT_i0 = &AT[(chunkId*chunk_len)*N1_A+i0];
        __global float* B_i1 = &B[(chunkId*chunk_len)*N1_A+i1];
  
        // Fill local memory
        for (int n=start_A; n<stop_A; n++) {
            shared_AT[i0_AT*chunk_len+n]=AT_i0[n*N1_A];
        }

        for (int n=start_B; n<stop_B; n++) {
            shared_B[i0_B*chunk_len+n]=B_i1[n*N1_A];
        }

        // Enqueue a local barrier to ensure shared memory is filled
        barrier(CLK_LOCAL_MEM_FENCE);

        // Scratch variable to accumulate the chunk
        float scratch=0.0f;

        // Now perform the dot product 
        for (int n=0; n<chunk_len; n++) {
            scratch+=shared_AT[i0_AT*chunk_len+n]*shared_B[i0_B*chunk_len+n];
        }

        temp+=scratch;
        
        // Enqueue a local barrier to ensure shared memory is filled
        barrier(CLK_LOCAL_MEM_FENCE);

    }
        
    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp;
}

__kernel void mat_mult_tile_local_async_copy_AT (
                        __global float* AT, 
                        __global float* B, 
                        __global float* C,
                        __local float* shared_AT,
                        __local float* shared_B,
                        unsigned int N1_A, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int nchunks,
                        unsigned int chunk_len) { 
    
    // AT is of size (N0_C, N1_A)
    // B is of size (N1_C, N1_A)
    // C is of size (N0_C, N1_C) 

    // i1 and i0 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1);

    // Fetch local memory into shared_AT and shared_B
    
    // Index within local memory
    size_t s1 = get_local_id(0); // fastest dimension
    size_t s0 = get_local_id(1); // Slowest dimension

    // shared_AT is of size (L0, chunk_len) (s0, n)
    // shared_B is of size (L1, chunk_len) (s1, n)
    size_t L1 = get_local_size(0); // Fastest dimension
    size_t L0 = get_local_size(1); // Slowest dimension

    // Scratch variable to accumulate the sum
    float temp=0.0f;

    for (size_t chunkId=0; chunkId<nchunks; chunkId++) {
    
        event_t event=0;

        // Copy from AT
        for (int n=0; n<L0; n++) {
            event = async_work_group_strided_copy(
                &shared_AT[n*chunk_len],
                &AT[(chunkId*chunk_len)*N1_A+i0-s0+n],
                (size_t)chunk_len,
                (size_t)N1_A,
                event
           );
        }

        // Copy from B
        for (int n=0; n<L1; n++) {
            event = async_work_group_strided_copy(
                &shared_B[n*chunk_len],
                &B[(chunkId*chunk_len)*N1_A+i1-s1+n],
                (size_t)chunk_len,
                (size_t)N1_A,
                event
            );
        }

        // Wait for the group event to finish
        wait_group_events(1, &event);

        // Scratch variable to accumulate the chunk
        float scratch=0.0f;

        // Now perform the dot product 
        for (int n=0; n<chunk_len; n++) {
            scratch+=shared_AT[s0*chunk_len+n]*shared_B[s1*chunk_len+n];
        }

        temp+=scratch;
        
        // Enqueue a local barrier to ensure shared memory is filled
        barrier(CLK_LOCAL_MEM_FENCE);

    }
        
    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp;
}


// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_local_BT (
                        __global float* A_star, 
                        __global float* BT_star, 
                        __global float* C,
                        __local float* shared_A_star,
                        __local float* shared_BT_star,
                        unsigned int N1_A_star, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int chunk_len,
                        unsigned int start_chunk_id,
                        unsigned int end_chunk_id) { 
    
    // A_star is of size (N0_C, N1_A_star), (i1, n)
    // BT_star is of size (N1_C, N1_A_star), (i2, n)
    // C is of size (N0_C, N1_C), (i0, i1)
    
    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); 
    
    // shared_A_star is of size (L0, chunk_len) (s0, n)
    // shared_B_star is of size (L1, chunk_len) (s1, n)
    size_t L0 = get_local_size(1); // Slowest dimension
    size_t L1 = get_local_size(0); // Fastest dimension
    
    // index within local memory
    size_t s0 = get_local_id(1); // Slowest dimension
    size_t s1 = get_local_id(0); // fastest dimension
    
    __local float* shared_A_star_s0 = &shared_A_star[s0*chunk_len];
    __local float* shared_BT_star_s1 = &shared_BT_star[s1*chunk_len];

    // Scratch variable to accumulate the sum
    float temp1=0.0f, temp2=0.0f;

    // Start and end positions
    size_t start0, end0, start1, end1;
    get_start_end(L1, chunk_len, s1, &start1, &end1);
    get_start_end(L0, chunk_len, s0, &start0, &end0);

    // Loop over the chunks
    for (int chunk_id=start_chunk_id; chunk_id<end_chunk_id; chunk_id++) {

        // Fetch local memory into shared_A_star and shared_B_star
        
        // Starting positions for the copy
        __global float* A_star_i0 = &A_star[i0*N1_A_star+chunk_id*chunk_len];
        __global float* BT_star_i1 = &BT_star[i1*N1_A_star+chunk_id*chunk_len];
          
        // Fill the rows of shared_A_star and shared_B_star
        // From row i1 of A_star
        for (int n = start1; n<end1; n++) {
            shared_A_star_s0[n] = A_star_i0[n];
        }
        
        // From row i2 of B_star
        for (int n = start0; n<end0; n++) {
            shared_BT_star_s1[n] = BT_star_i1[n];
        }
              
        // Enqueue a local barrier to ensure shared memory is filled
        barrier(CLK_LOCAL_MEM_FENCE);
        
        temp1=0.0f;

        // Loop over columns of A and rows of B 
        for (size_t n=0; n<chunk_len; n++) {
                
            // Loop across row i0 of A
            // and down column i1 of B
            temp1+=shared_A_star_s0[n]*shared_BT_star_s1[n];
        }

        temp2+=temp1;
        
        // Enqueue a local barrier to ensure all work items 
        // are ready for the next tile
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp2;
}

// Matrix multiply kernel that uses local memory
__kernel void mat_mult_tile_local_vector_BT (
                        __global float8* A_star, 
                        __global float8* BT_star, 
                        __global float* C,
                        __local float8* shared_A_star,
                        __local float8* shared_BT_star,
                        unsigned int N1_A_star, 
                        unsigned int N0_C,
                        unsigned int N1_C,
                        unsigned int chunk_len,
                        unsigned int start_chunk_id,
                        unsigned int end_chunk_id) { 
    
    // A_star is of size (N0_C, N1_A_star), (i1, n)
    // BT_star is of size (N1_C, N1_A_star), (i2, n)
    // C is of size (N0_C, N1_C), (i0, i1)
    
    // i1 and i2 represent the coordinates in Matrix C 
    // We assume row-major ordering for the matrices 
    size_t i1=min(get_global_id(0), (size_t)N1_C-1); // Fastest dimension
    size_t i0=min(get_global_id(1), (size_t)N0_C-1); 
    
    // shared_A_star is of size (L0, chunk_len) (s0, n)
    // shared_B_star is of size (L1, chunk_len) (s1, n)
    size_t L0 = get_local_size(1); // Slowest dimension
    size_t L1 = get_local_size(0); // Fastest dimension
    
    // index within local memory
    size_t s0 = get_local_id(1); // Slowest dimension
    size_t s1 = get_local_id(0); // fastest dimension
    
    __local float8* shared_A_star_s0 = &shared_A_star[s0*chunk_len];
    __local float8* shared_BT_star_s1 = &shared_BT_star[s1*chunk_len];

    // Scratch variable to accumulate the sum
    float8 temp1=(float8)0.0f, temp2=(float8)0.0f;

    // Start and end positions to copy within a chunk
    size_t start0, end0, start1, end1;
    get_start_end(L1, chunk_len, s1, &start1, &end1);
    get_start_end(L0, chunk_len, s0, &start0, &end0);

    // Loop over the chunks
    for (int chunk_id=start_chunk_id; chunk_id<end_chunk_id; chunk_id++) {

        // Fetch local memory into shared_A_star and shared_B_star
        
        // Starting positions for the copy
        __global float8* A_star_i0 = &A_star[i0*N1_A_star+chunk_id*chunk_len];
        __global float8* BT_star_i1 = &BT_star[i1*N1_A_star+chunk_id*chunk_len];
          
        // Fill the rows of shared_A_star and shared_B_star
        // From row i1 of A_star
        for (int n = start1; n<end1; n++) {
            shared_A_star_s0[n] = A_star_i0[n];
        }
        
        // From row i2 of B_star
        for (int n = start0; n<end0; n++) {
            shared_BT_star_s1[n] = BT_star_i1[n];
        }
              
        // Enqueue a local barrier to ensure shared memory is filled
        barrier(CLK_LOCAL_MEM_FENCE);
        
        temp1=(float8)0.0f;

        // Loop over columns of A and rows of B 
        for (size_t n=0; n<chunk_len; n++) {
                
            // Loop across row i0 of A
            // and down column i1 of B
            temp1+=shared_A_star_s0[n]*shared_BT_star_s1[n];
        }

        temp2+=temp1;
        
        // Enqueue a local barrier to ensure all work items 
        // are ready for the next tile
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Put the accumulated value into position
    C[i0*N1_C+i1]=temp2.s0 + temp2.s1 + temp2.s2 + temp2.s3
        + temp2.s4 + temp2.s5 + temp2.s6 + temp2.s7;
}

// Local memory matrix multiply kernel 
__kernel void transpose (__global float* src, 
                        __global float* dest, 
                        unsigned int N0_src,
                        unsigned int N1_src) { 
    
    // src is of size (N0_src, N1_src)
    // dest is of size (N1_src, N0_src)
    
    // i0 and i1 represent the coordinates in src 
    // We assume row-major ordering for the matrices 
    size_t i0=get_global_id(1); 
    size_t i1=get_global_id(0); 
    
    if ((i0<N0_src) && (i1<N1_src)) {
        dest[i1*N0_src+i0]=src[i0*N1_src+i1];
    }
}
