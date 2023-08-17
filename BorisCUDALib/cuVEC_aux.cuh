#pragma once

#include "cuVEC.h"
#include "Reduction.cuh"

//------------------------------------------------------------------- ZERO/SET AUX VALUES

template <typename VType>
__global__ void zero_aux_values(VType& aux_value, VType& aux_value2, VType& aux_value3, cuBReal& aux_real, cuBReal& aux_real2, size_t& aux_integer)
{
	if (threadIdx.x == 0) aux_value = VType();
	if (threadIdx.x == 1) aux_value2 = VType();
	if (threadIdx.x == 2) aux_value3 = VType();
	if (threadIdx.x == 3) aux_real = 0.0;
	if (threadIdx.x == 4) aux_real2 = 0.0;
	if (threadIdx.x == 5) aux_integer = 0;
}

template <typename VType>
__global__ void divide_aux_values(VType& aux_value, VType& aux_value2, VType& aux_value3, cuBReal& aux_real, cuBReal& aux_real2, size_t& aux_integer)
{
	if (threadIdx.x == 0 && aux_integer) aux_value /= aux_integer;
	if (threadIdx.x == 1 && aux_integer) aux_value2 /= aux_integer;
	if (threadIdx.x == 2 && aux_integer) aux_value3 /= aux_integer;
	if (threadIdx.x == 3 && aux_integer) aux_real /= aux_integer;
	if (threadIdx.x == 4 && aux_integer) aux_real2 /= aux_integer;
	if (threadIdx.x == 5 && aux_integer) aux_integer /= aux_integer;
}

//------------------------------------------------------------------- COUNT NON-EMPTY CELLS

template <typename VType>
__global__ void count_nonempty_cells_kernel(cuSZ3& n, VType*& quantity, size_t& aux_integer)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	size_t count_point = 0;

	if (idx < n.dim()) {

		if (quantity[idx] != VType()) count_point = 1;
	}

	reduction_sum(0, 1, &count_point, aux_integer);
}

template void cuVEC<float>::count_nonempty_cells(size_t arr_size);
template void cuVEC<double>::count_nonempty_cells(size_t arr_size);

template void cuVEC<cuFLT3>::count_nonempty_cells(size_t arr_size);
template void cuVEC<cuDBL3>::count_nonempty_cells(size_t arr_size);

template void cuVEC<cuFLT4>::count_nonempty_cells(size_t arr_size);
template void cuVEC<cuDBL4>::count_nonempty_cells(size_t arr_size);

//count cells which don't have a null value set : i.e. non-empty.
template <typename VType>
__host__ void cuVEC<VType>::count_nonempty_cells(size_t arr_size)
{
	zero_aux_values <<<1, CUDATHREADS >>> (aux_value, aux_value2, aux_value3, aux_real, aux_real2, aux_integer);

	count_nonempty_cells_kernel <<< (arr_size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (n, quantity, aux_integer);
}