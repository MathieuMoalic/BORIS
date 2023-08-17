#pragma once

#include "cuVEC_VC.h"
#include "cuFuncs_Math.h"
#include "launchers.h"
#include "Reduction.cuh"

//--------------------------------------------MULTIPLE ENTRIES SETTERS - OTHERS

//------------------------------------------------------------------- SETNONEMPTY

template <typename VType>
__global__ void setnonempty_kernel(cuSZ3& n, int*& ngbrFlags, VType*& quantity, VType value)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < n.dim()) {

		if (ngbrFlags[idx] & NF_NOTEMPTY) {

			quantity[idx] = value;
		}
	}
}

template void cuVEC_VC<float>::setnonempty(float value);
template void cuVEC_VC<double>::setnonempty(double value);

template void cuVEC_VC<cuFLT3>::setnonempty(cuFLT3 value);
template void cuVEC_VC<cuDBL3>::setnonempty(cuDBL3 value);

template <typename VType>
__host__ void cuVEC_VC<VType>::setnonempty(VType value)
{
	setnonempty_kernel <<< (get_gpu_value(cuVEC<VType>::n).dim() + CUDATHREADS) / CUDATHREADS, CUDATHREADS >> > (cuVEC<VType>::n, ngbrFlags, cuVEC<VType>::quantity, value);
}

//------------------------------------------------------------------- SETRECTNONEMPTY

template <typename VType>
__global__ void setrectnonempty_kernel(cuSZ3& n, int*& ngbrFlags, VType*& quantity, cuBox box, VType value)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuINT3 ijk = cuINT3(idx % n.x, (idx / n.x) % n.y, idx / (n.x*n.y));

	if (idx < n.dim() && box.contains(ijk)) {

		if (ngbrFlags[idx] & NF_NOTEMPTY) {

			quantity[idx] = value;
		}
	}
}

template void cuVEC_VC<float>::setrectnonempty(const cuRect& rectangle, float value);
template void cuVEC_VC<double>::setrectnonempty(const cuRect& rectangle, double value);

template void cuVEC_VC<cuFLT3>::setrectnonempty(const cuRect& rectangle, cuFLT3 value);
template void cuVEC_VC<cuDBL3>::setrectnonempty(const cuRect& rectangle, cuDBL3 value);

//set value in non-empty cells only in given rectangle (relative coordinates)
template <typename VType>
__host__ void cuVEC_VC<VType>::setrectnonempty(const cuRect& rectangle, VType value)
{
	cuBox box = cuVEC<VType>::box_from_rect_max_cpu(rectangle + get_gpu_value(cuVEC<VType>::rect).s);

	setrectnonempty_kernel <<< (get_gpu_value(cuVEC<VType>::n).dim() + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (cuVEC<VType>::n, ngbrFlags, cuVEC<VType>::quantity, box, value);
}

//------------------------------------------------------------------- RENORMALIZE (cuVEC_VC)

template <typename VType, typename PType>
__global__ void cuvec_vc_renormalize_kernel(cuSZ3& n, int*& ngbrFlags, VType*& quantity, PType new_norm)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < n.dim()) {

		PType curr_norm = cu_GetMagnitude(quantity[idx]);

		if ((ngbrFlags[idx] & NF_NOTEMPTY) && cuIsNZ(curr_norm)) {

			quantity[idx] *= new_norm / curr_norm;
		}
	}
}

template void cuVEC_VC<float>::renormalize(size_t arr_size, float new_norm);
template void cuVEC_VC<double>::renormalize(size_t arr_size, double new_norm);

template void cuVEC_VC<cuFLT3>::renormalize(size_t arr_size, float new_norm);
template void cuVEC_VC<cuDBL3>::renormalize(size_t arr_size, double new_norm);

//re-normalize all non-zero values to have the new magnitude (multiply by new_norm and divide by current magnitude)
template <typename VType>
template <typename PType>
__host__ void cuVEC_VC<VType>::renormalize(size_t arr_size, PType new_norm)
{
	cuvec_vc_renormalize_kernel <<< (arr_size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (cuVEC<VType>::n, ngbrFlags, cuVEC<VType>::quantity, new_norm);
}

//------------------------------------------------------------------- SHIFT : x

template <typename VType>
__global__ void shift_x_left1_kernel(cuRect shift_rect, cuVEC_VC<VType>& cuVEC, VType*& aux_block_values, bool& using_extended_flags, int*& ngbrFlags2, VType*& halo_n, VType*& halo_p)
{
	__shared__ VType shared_memory[CUDATHREADS];

	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuSZ3 n = cuVEC.n;
	cuINT3 ijk = cuINT3(idx % n.x, (idx / n.x) % n.y, idx / (n.x*n.y));

	cuBox shift_box = cuVEC.box_from_rect_min(shift_rect);
	//shift_box.e.x - 2 takes value from shift_box.e.x - 1
	shift_box.e.x--;

	if (idx < n.dim()) {

		shared_memory[threadIdx.x] = cuVEC[idx];
	}
	else shared_memory[threadIdx.x] = VType();

	//all values in this block must be transferred to shared memory before proceeding
	__syncthreads();

	//shift values within the box using shared_memory (both destination and source must not be empty).
	//We cannot shift the first element in this block since it must go to the block before this - store it in aux_block_values for later.
	//Similarly we cannot write the last element in this block since it needs a value from the next block.
	if (threadIdx.x == 0) aux_block_values[blockIdx.x] = shared_memory[0];

	//special treatment for cells at +x boundary, if halos are being used : get value from halo
	if (ijk.i == n.x - 1 && using_extended_flags && (ngbrFlags2[idx] & NF2_HALOPX) && cuVEC.is_not_empty(idx)) {

		//halo index
		int hidx = ((idx / n.x) % n.y) + (idx / (n.x*n.y))*n.y;
		cuVEC[idx] = halo_p[hidx];
	}
	else if (shift_box.contains(ijk) && cuVEC.is_not_empty(idx) && cuVEC.is_not_empty(idx + 1)) {

		if (threadIdx.x < CUDATHREADS - 1) {

			cuVEC[idx] = shared_memory[threadIdx.x + 1];
		}
	}

	//special treatment for cells at -x boundary if halos are being used : shift value into halo
	if (ijk.i == 0 && using_extended_flags && (ngbrFlags2[idx] & NF2_HALONX) && cuVEC.is_not_empty(idx)) {

		//halo index
		int hidx = ((idx / n.x) % n.y) + (idx / (n.x*n.y))*n.y;
		halo_n[hidx] = cuVEC[idx];
	}
}

template <typename VType>
__global__ void shift_x_left1_stitch_kernel(cuRect shift_rect, cuVEC_VC<VType>& cuVEC, VType*& aux_block_values)
{
	//index in aux_block_values
	int aux_blocks_idx = blockIdx.x * blockDim.x + threadIdx.x;

	//index in cuVEC : aux_block_values stored block beginning values which must be shifted to the cell to the left
	int cell_idx = aux_blocks_idx * CUDATHREADS - 1;

	cuSZ3 n = cuVEC.n;
	cuINT3 ijk = cuINT3(cell_idx % n.x, (cell_idx / n.x) % n.y, cell_idx / (n.x*n.y));

	cuBox shift_box = cuVEC.box_from_rect_min(shift_rect);
	shift_box.e.x--;
	
	if (shift_box.contains(ijk) && cuVEC.is_not_empty(cell_idx) && cuVEC.is_not_empty(cell_idx + 1)) {

		cuVEC[cell_idx] = aux_block_values[aux_blocks_idx];
	}
}

template <typename VType>
__global__ void shift_x_right1_kernel(cuRect shift_rect, cuVEC_VC<VType>& cuVEC, VType*& aux_block_values, bool& using_extended_flags, int*& ngbrFlags2, VType*& halo_n, VType*& halo_p)
{
	__shared__ VType shared_memory[CUDATHREADS];

	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuSZ3 n = cuVEC.n;
	cuINT3 ijk = cuINT3(idx % n.x, (idx / n.x) % n.y, idx / (n.x*n.y));

	cuBox shift_box = cuVEC.box_from_rect_min(shift_rect);
	//shift_box.s.x + 1 takes value from shift_box.s.x
	shift_box.s.x++;

	if (idx < n.dim()) {

		shared_memory[threadIdx.x] = cuVEC[idx];
	}
	else shared_memory[threadIdx.x] = VType();

	//all values in this block must be transferred to shared memory before proceeding
	__syncthreads();

	//shift values within the box using shared_memory (both destination and source must not be empty).
	//We cannot shift the last element in this block since it must go to the block after this - store it in aux_block_values for later.
	//Similarly we cannot write the first element in this block since it needs a value from the previous block.
	if (threadIdx.x == CUDATHREADS - 1) aux_block_values[blockIdx.x] = shared_memory[CUDATHREADS - 1];

	//special treatment for cells at -x boundary, if halos are being used : get value from halo
	if (ijk.i == 0 && using_extended_flags && (ngbrFlags2[idx] & NF2_HALONX) && cuVEC.is_not_empty(idx)) {

		//halo index
		int hidx = ((idx / n.x) % n.y) + (idx / (n.x*n.y))*n.y;
		cuVEC[idx] = halo_n[hidx];
	}
	else if (shift_box.contains(ijk) && cuVEC.is_not_empty(idx) && cuVEC.is_not_empty(idx - 1)) {

		if (threadIdx.x > 0) {

			cuVEC[idx] = shared_memory[threadIdx.x - 1];
		}
	}

	//special treatment for cells at +x boundary if halos are being used : shift value into halo
	if (ijk.i == n.x - 1 && using_extended_flags && (ngbrFlags2[idx] & NF2_HALOPX) && cuVEC.is_not_empty(idx)) {

		//halo index
		int hidx = ((idx / n.x) % n.y) + (idx / (n.x*n.y))*n.y;
		halo_p[hidx] = cuVEC[idx];
	}
}

template <typename VType>
__global__ void shift_x_right1_stitch_kernel(cuRect shift_rect, cuVEC_VC<VType>& cuVEC, VType*& aux_block_values)
{
	//index in aux_block_values
	int aux_blocks_idx = blockIdx.x * blockDim.x + threadIdx.x;

	//index in cuVEC : aux_block_values stored block ending values which must be shifted to the cell to the right
	int cell_idx = aux_blocks_idx * CUDATHREADS + CUDATHREADS;

	cuSZ3 n = cuVEC.n;
	cuINT3 ijk = cuINT3(cell_idx % n.x, (cell_idx / n.x) % n.y, cell_idx / (n.x*n.y));

	cuBox shift_box = cuVEC.box_from_rect_min(shift_rect);
	shift_box.s.x++;

	if (shift_box.contains(ijk) && cuVEC.is_not_empty(cell_idx) && cuVEC.is_not_empty(cell_idx - 1)) {

		cuVEC[cell_idx] = aux_block_values[aux_blocks_idx];
	}
}

template void cuVEC_VC<float>::shift_x(size_t size, cuBReal delta, cuRect shift_rect);
template void cuVEC_VC<double>::shift_x(size_t size, cuBReal delta, cuRect shift_rect);

template void cuVEC_VC<cuFLT3>::shift_x(size_t size, cuBReal delta, cuRect shift_rect);
template void cuVEC_VC<cuDBL3>::shift_x(size_t size, cuBReal delta, cuRect shift_rect);

//shift all the values in this VEC by the given delta (units same as h). Shift values in given shift_rect (absolute coordinates).
//Also keep magnitude in each cell (e.g. use for vectorial quantities, such as magnetization, to shift only the direction).
template <typename VType>
__host__ void cuVEC_VC<VType>::shift_x(size_t size, cuBReal delta, cuRect shift_rect)
{
	cuReal3 shift_debt_cpu = get_gpu_value(shift_debt);
	cuReal3 h_cpu = get_gpu_value(cuVEC<VType>::h);

	if (fabs(shift_debt_cpu.x + delta) < h_cpu.x) {

		//total shift not enough : bank it and return
		shift_debt_cpu.x += delta;
		set_gpu_value(shift_debt, shift_debt_cpu);
		return;
	}

	//only shift an integer number of cells : there might be a sub-cellsize remainder so just bank it to be used next time
	int cells_shift = (int)((shift_debt_cpu.x + delta) / h_cpu.x);
	shift_debt_cpu.x -= h_cpu.x * cells_shift - delta;
	set_gpu_value(shift_debt, shift_debt_cpu);
	
	if (cells_shift < 0) {

		//only shift one cell at a time - for a moving mesh algorithm it would be very unusual to have to shift by more than one cell at a time if configured properly (mesh trigger from finest mesh)
		//one-call shift routines for cells_shift > 1 are not straight-forward so not worth implementing for now
		while (cells_shift < 0) {

			shift_x_left1_kernel <<< (size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (shift_rect, *this, cuVEC<VType>::aux_block_values, using_extended_flags, ngbrFlags2, halo_n, halo_p);

			size_t stitch_size = (size + CUDATHREADS) / CUDATHREADS;
			shift_x_left1_stitch_kernel <<< (stitch_size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (shift_rect, *this, cuVEC<VType>::aux_block_values);
			
			cells_shift++;
		}
	}
	else {

		while (cells_shift > 0) {

			shift_x_right1_kernel <<< (size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (shift_rect, *this, cuVEC<VType>::aux_block_values, using_extended_flags, ngbrFlags2, halo_n, halo_p);

			size_t stitch_size = (size + CUDATHREADS) / CUDATHREADS;
			shift_x_right1_stitch_kernel <<< (stitch_size + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> (shift_rect, *this, cuVEC<VType>::aux_block_values);

			cells_shift--;
		}
	}
}

//------------------------------------------------------------------- SHIFT : y