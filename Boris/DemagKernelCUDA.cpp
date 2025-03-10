#include "stdafx.h"
#include "DemagKernelCUDA.h"

#if COMPILECUDA == 1

#if defined MODULE_COMPILATION_DEMAG || defined MODULE_COMPILATION_SDEMAG

#include "DemagTFunc.h"
#include "DemagTFuncCUDA.h"

//-------------------------- MEMORY ALLOCATION

BError DemagKernelCUDA::AllocateKernelMemory(void)
{
	BError error(__FUNCTION__);

	//now allocate memory
	if (!transpose_xy) {

		if (n.z == 1) {

			Kodiag()->clear();

			if (!K2D_odiag()->resize(cuSZ3(nxRegion, N.y / 2 + 1, 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
			if (!Kdiag()->resize(cuSZ3(nxRegion, N.y / 2 + 1, 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		}
		else {

			K2D_odiag()->clear();

			if (!Kodiag()->resize(cuSZ3(nxRegion, N.y / 2 + 1, N.z / 2 + 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
			if (!Kdiag()->resize(cuSZ3(nxRegion, N.y / 2 + 1, N.z / 2 + 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		}
	}
	else {

		if (n.z == 1) {

			Kodiag()->clear();

			if (!K2D_odiag()->resize(cuSZ3(N.y / 2 + 1, nxRegion, 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
			if (!Kdiag()->resize(cuSZ3(N.y / 2 + 1, nxRegion, 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		}
		else {

			K2D_odiag()->clear();
			
			if (!Kodiag()->resize(cuSZ3(N.y / 2 + 1, nxRegion, N.z / 2 + 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
			if (!Kdiag()->resize(cuSZ3(N.y / 2 + 1, nxRegion, N.z / 2 + 1))) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		}
	}

	return error;
}

//-------------------------- KERNEL CALCULATION

BError DemagKernelCUDA::Calculate_Demag_Kernels_2D(bool include_self_demag, bool initialize_on_gpu)
{
	BError error(__FUNCTION__);
	
	if (initialize_on_gpu) {

		//first attempt to compute kernels on GPU
		error = Calculate_Demag_Kernels_2D_onGPU(include_self_demag);

		if (error) {

			//if it fails (out of GPU memory) then next attempt to run convolution with reduced memory usage...
			error.reset();
			error = Set_Preserve_Zero_Padding(false);
			if (!error) {

				//...and try again
				error = Calculate_Demag_Kernels_2D_onGPU(include_self_demag);
				//if this still fails then finally try to initialize on cpu below
				if (error) { error.reset(); error(BWARNING_NOGPUINITIALIZATION); }
				else return error;
			}
			else return error;
		}
		else return error;
	}

	//-------------- CALCULATE DEMAG TENSOR

	//Demag tensor components
	//
	// D11 D12 0
	// D12 D22 0
	// 0   0   D33

	//Ddiag : D11, D22, D33 are the diagonal tensor elements
	VEC<DBL3> Ddiag;
	if (!Ddiag.resize(N)) return error(BERROR_OUTOFMEMORY_NCRIT);

	//off-diagonal tensor elements
	std::vector<double> Dodiag;
	if (!malloc_vector(Dodiag, N.x*N.y)) return error(BERROR_OUTOFMEMORY_NCRIT);

	//use ratios instead of cellsizes directly - same result but better in terms of floating point errors
	DemagTFunc dtf;

	if (pbc_images.IsNull()) {

		//no pbcs in any dimension -> use these
		if (!dtf.CalcDiagTens2D(Ddiag, n, N, h / maximum(h.x, h.y, h.z), include_self_demag)) return error(BERROR_OUTOFMEMORY_NCRIT);
		if (!dtf.CalcOffDiagTens2D(Dodiag, n, N, h / maximum(h.x, h.y, h.z))) return error(BERROR_OUTOFMEMORY_NCRIT);
	}
	else {

		//pbcs used in at least one dimension
		if (!dtf.CalcDiagTens2D_PBC(
			Ddiag, N, h / maximum(h.x, h.y, h.z), 
			true, ASYMPTOTIC_DISTANCE, 
			pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFMEMORY_NCRIT);

		if (!dtf.CalcOffDiagTens2D_PBC(
			Dodiag, N, h / maximum(h.x, h.y, h.z), 
			true, ASYMPTOTIC_DISTANCE, 
			pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFMEMORY_NCRIT);
	}

	//-------------- DEMAG KERNELS ON CPU-ADDRESSABLE MEMORY

	VEC<DBL3> Kdiag_cpu;
	std::vector<double> K2D_odiag_cpu;

	if (!transpose_xy) {

		if (!Kdiag_cpu.resize(SZ3(nxRegion, N.y / 2 + 1, 1))) return error(BERROR_OUTOFMEMORY_CRIT);
	}
	else {

		if (!Kdiag_cpu.resize(SZ3(N.y / 2 + 1, nxRegion, 1))) return error(BERROR_OUTOFMEMORY_CRIT);
	}

	if (!malloc_vector(K2D_odiag_cpu, nxRegion * (N.y / 2 + 1))) return error(BERROR_OUTOFMEMORY_CRIT);
	
	//-------------- SETUP FFT

	double* pline_real = fftw_alloc_real(maximum(N.x, N.y, N.z) * 3);
	double* pline_real_odiag = fftw_alloc_real(maximum(N.x, N.y, N.z));
	fftw_complex* pline_odiag = fftw_alloc_complex(maximum(N.x / 2 + 1, N.y, N.z));
	fftw_complex* pline = fftw_alloc_complex(maximum(N.x / 2 + 1, N.y, N.z) * 3);

	//make fft plans
	int dims_x[1] = { (int)N.x };
	int dims_y[1] = { (int)N.y };

	fftw_plan plan_fwd_x = fftw_plan_many_dft_r2c(1, dims_x, 3,
		pline_real, nullptr, 3, 1,
		pline, nullptr, 3, 1,
		FFTW_PATIENT);

	fftw_plan plan_fwd_x_odiag = fftw_plan_many_dft_r2c(1, dims_x, 1,
		pline_real_odiag, nullptr, 1, 1,
		pline_odiag, nullptr, 1, 1,
		FFTW_PATIENT);

	fftw_plan plan_fwd_y = fftw_plan_many_dft_r2c(1, dims_y, 3,
		pline_real, nullptr, 3, 1,
		pline, nullptr, 3, 1,
		FFTW_PATIENT);

	fftw_plan plan_fwd_y_odiag = fftw_plan_many_dft_r2c(1, dims_y, 1,
		pline_real_odiag, nullptr, 1, 1,
		pline_odiag, nullptr, 1, 1,
		FFTW_PATIENT);

	//-------------- FFT REAL TENSOR INTO REAL KERNELS

	//1. FFTs along x
	for (int j = 0; j < N.y; j++) {

		//write input into fft line
		for (int i = 0; i < N.x; i++) {

			int idx_in = i + j * N.x;

			*reinterpret_cast<DBL3*>(pline_real + i * 3) = Ddiag[idx_in];
			*reinterpret_cast<double*>(pline_real_odiag + i) = Dodiag[idx_in];
		}

		//fft on line
		fftw_execute(plan_fwd_x);
		fftw_execute(plan_fwd_x_odiag);

		//pack into tensor for next step
		for (int i = 0; i < nxRegion; i++) {

			ReIm3 value = *reinterpret_cast<ReIm3*>(pline + (i + xRegion.i) * 3);
			ReIm value_odiag = *reinterpret_cast<ReIm*>(pline_odiag + i + xRegion.i);

			Ddiag[i + j * nxRegion] = DBL3(value.x.Re, value.y.Re, value.z.Re);
			Dodiag[i + j * nxRegion] = value_odiag.Im;
		}
	}

	//2. FFTs along y
	for (int i = 0; i < nxRegion; i++) {

		//fetch line from array
		for (int j = 0; j < N.y; j++) {

			*reinterpret_cast<DBL3*>(pline_real + j * 3) = Ddiag[i + j * nxRegion];
			*reinterpret_cast<double*>(pline_real_odiag + j) = Dodiag[i + j * nxRegion];
		}

		//fft on line
		fftw_execute(plan_fwd_y);
		fftw_execute(plan_fwd_y_odiag);

		//pack into output real kernels with reduced strides
		for (int j = 0; j < N.y / 2 + 1; j++) {

			ReIm3 value = *reinterpret_cast<ReIm3*>(pline + j * 3);
			ReIm value_odiag = *reinterpret_cast<ReIm*>(pline_odiag + j);

			if (!transpose_xy) {

				//even w.r.t. y so output is purely real
				Kdiag_cpu[i + j * nxRegion] = DBL3(value.x.Re, value.y.Re, value.z.Re);

				//odd w.r.t. y so the purely imaginary input becomes purely real
				//however since we used CopyRealShuffle and RealfromComplexFFT, i.e. treating the input as purely real rather than purely imaginary we need to account for the i * i = -1 term, hence the - sign below
				K2D_odiag_cpu[i + j * nxRegion] = -value_odiag.Im;
			}
			else {
				
				//even w.r.t. y so output is purely real
				Kdiag_cpu[j + i * (N.y / 2 + 1)] = DBL3(value.x.Re, value.y.Re, value.z.Re);

				//odd w.r.t. y so the purely imaginary input becomes purely real
				//however since we used CopyRealShuffle and RealfromComplexFFT, i.e. treating the input as purely real rather than purely imaginary we need to account for the i * i = -1 term, hence the - sign below
				K2D_odiag_cpu[j + i * (N.y / 2 + 1)] = -value_odiag.Im;
			}
		}
	}
	
	//-------------- TRANSFER TO GPU KERNELS

	Kdiag()->copy_from_cpuvec(Kdiag_cpu);
	K2D_odiag()->copy_from_vector(K2D_odiag_cpu);
	
	//-------------- CLEANUP

	fftw_destroy_plan(plan_fwd_x);
	fftw_destroy_plan(plan_fwd_x_odiag);
	fftw_destroy_plan(plan_fwd_y);
	fftw_destroy_plan(plan_fwd_y_odiag);

	fftw_free((double*)pline_real);
	fftw_free((double*)pline_real_odiag);
	fftw_free((fftw_complex*)pline);

	//Done
	return error;
}

BError DemagKernelCUDA::Calculate_Demag_Kernels_2D_onGPU(bool include_self_demag)
{
	BError error(__FUNCTION__);

	//-------------------------------------------

	cu_arr<cufftDoubleReal> cuInx, cuIny, cuInz;
	if (!cuInx.resize(N.x*N.y)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	if (!cuIny.resize(N.x*N.y)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	if (!cuInz.resize(N.x*N.y)) return error(BERROR_OUTOFGPUMEMORY_CRIT);

	cu_arr<cufftDoubleComplex> cuOut;
	if (!cuOut.resize((N.x / 2 + 1)*N.y)) return error(BERROR_OUTOFGPUMEMORY_CRIT);

	//-------------- CALCULATE DEMAG TENSOR on GPU

	//Demag tensor components
	//
	// D11 D12 0
	// D12 D22 0
	// 0   0   D33

	DemagTFuncCUDA dtf_gpu;

	if (pbc_images.IsNull()) {

		if (!dtf_gpu.CalcDiagTens2D(cuInx, cuIny, cuInz, n, N, h / maximum(h.x, h.y, h.z), include_self_demag)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}
	else {

		if (!dtf_gpu.CalcDiagTens2D_PBC(
			cuInx, cuIny, cuInz, N, h / maximum(h.x, h.y, h.z),
			true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}

	//-------------- SETUP FFT

	cufftHandle planTens2D_x, planTens2D_y;

	int embed[1] = { 0 };
	int ndims_x[1] = { (int)N.x };
	int ndims_y[1] = { (int)N.y };

	//Forward fft along x direction (out-of-place):
	cufftResult cuffterr = cufftPlanMany(&planTens2D_x, 1, ndims_x,
		embed, 1, N.x,
		embed, 1, (N.x / 2 + 1),
		CUFFT_D2Z, (int)N.y);

	//Forward fft along y direction (out-of-place):
	cuffterr = cufftPlanMany(&planTens2D_y, 1, ndims_y,
		embed, nxRegion, 1,
		embed, nxRegion, 1,
		CUFFT_D2Z, (int)nxRegion);

	//-------------- FFT REAL TENSOR INTO REAL KERNELS

	auto fftcomponent = [&](cu_arr<cufftDoubleReal>& cuIn, int component) -> void
	{
		//Now FFT along x from input to output
		cufftExecD2Z(planTens2D_x, cuIn, cuOut);

		//extract real parts from cuOut and place in cuIn
		cuOut_to_cuIn_Re(nxRegion * N.y, cuIn, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);

		//Now FFT along y from input to output
		cufftExecD2Z(planTens2D_y, cuIn, cuOut);

		//extract real parts from cuOut and place in kernel
		cuOut_to_Kdiagcomponent(cuOut, component);
	};

	//diagonal components
	fftcomponent(cuInx, 1);
	fftcomponent(cuIny, 2);
	fftcomponent(cuInz, 3);

	//off-diagonal component
	if (pbc_images.IsNull()) {

		if (!dtf_gpu.CalcOffDiagTens2D(cuInx, n, N, h / maximum(h.x, h.y, h.z))) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}
	else {

		if (!dtf_gpu.CalcOffDiagTens2D_PBC(
			cuInx, N, h / maximum(h.x, h.y, h.z), 
			true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}

	//Now FFT along x from input to output
	cufftExecD2Z(planTens2D_x, cuInx, cuOut);

	//extract imaginary parts from cuOut and place in cuIn
	cuOut_to_cuIn_Im(nxRegion * N.y, cuInx, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);

	//Now FFT along y from input to output
	cufftExecD2Z(planTens2D_y, cuInx, cuOut);

	//extract imaginary part from cuOut and place in kernel, also changing sign
	cuOut_to_K2D_odiag(cuOut);

	//-------------- CLEANUP

	cufftDestroy(planTens2D_x);
	cufftDestroy(planTens2D_y);

	//Done
	return error;
}

BError DemagKernelCUDA::Calculate_Demag_Kernels_3D(bool include_self_demag, bool initialize_on_gpu)
{
	BError error(__FUNCTION__);
	
	if (initialize_on_gpu) {

		//first attempt to compute kernels on GPU
		error = Calculate_Demag_Kernels_3D_onGPU(include_self_demag);

		if (error) { 
			
			//if it fails (out of GPU memory) then next attempt to run convolution with reduced memory usage...
			error.reset(); 
			error = Set_Preserve_Zero_Padding(false);
			if (!error) {

				//...and try again
				error = Calculate_Demag_Kernels_3D_onGPU(include_self_demag);
				//if this still fails then finally try to initialize on cpu below
				if (error) { error.reset(); error(BWARNING_NOGPUINITIALIZATION); }
				else return error;
			}
			else return error;
		}
		else return error;
	}

	//-------------- DEMAG TENSOR

	//Demag tensor components
	//
	// D11 D12 D13
	// D12 D22 D23
	// D13 D23 D33

	//D11, D22, D33 are the diagonal tensor elements
	//D12, D13, D23 are the off-diagonal tensor elements
	VEC<DBL3> D;

	if (!D.resize(N)) return error(BERROR_OUTOFMEMORY_NCRIT);
	
	//object used to compute tensor elements
	DemagTFunc dtf;

	//-------------- DEMAG KERNEL ON CPU-ADDRESSABLE MEMORY

	VEC<DBL3> K_cpu;

	if (!transpose_xy) {

		if (!K_cpu.resize(SZ3(nxRegion, N.y / 2 + 1, N.z / 2 + 1))) return error(BERROR_OUTOFMEMORY_CRIT);
	}
	else {

		if (!K_cpu.resize(SZ3(N.y / 2 + 1, nxRegion, N.z / 2 + 1))) return error(BERROR_OUTOFMEMORY_CRIT);
	}

	//-------------- SETUP FFT

	double* pline_real = fftw_alloc_real(maximum(N.x, N.y, N.z) * 3);
	fftw_complex* pline = fftw_alloc_complex(maximum(N.x / 2 + 1, N.y, N.z) * 3);

	//make fft plans
	int dims_x[1] = { (int)N.x };
	int dims_y[1] = { (int)N.y };
	int dims_z[1] = { (int)N.z };

	fftw_plan plan_fwd_x = fftw_plan_many_dft_r2c(1, dims_x, 3,
		pline_real, nullptr, 3, 1,
		pline, nullptr, 3, 1,
		FFTW_PATIENT);

	fftw_plan plan_fwd_y = fftw_plan_many_dft_r2c(1, dims_y, 3,
		pline_real, nullptr, 3, 1,
		pline, nullptr, 3, 1,
		FFTW_PATIENT);

	fftw_plan plan_fwd_z = fftw_plan_many_dft_r2c(1, dims_z, 3,
		pline_real, nullptr, 3, 1,
		pline, nullptr, 3, 1,
		FFTW_PATIENT);

	//-------------- FFT REAL TENSOR INTO REAL KERNELS

	//lambda used to transform an input tensor into an output kernel
	auto tensor_to_kernel = [&](VEC<DBL3>& tensor, VEC<DBL3>& kernel, bool off_diagonal) -> void {

		//1. FFTs along x
		for (int k = 0; k < N.z; k++) {
			for (int j = 0; j < N.y; j++) {

				//write input into fft line
				for (int i = 0; i < N.x; i++) {

					int idx_in = i + j * N.x + k * N.x * N.y;

					*reinterpret_cast<DBL3*>(pline_real + i * 3) = tensor[idx_in];
				}

				//fft on line
				fftw_execute(plan_fwd_x);

				//pack into tensor for next step
				for (int i = 0; i < nxRegion; i++) {

					ReIm3 value = *reinterpret_cast<ReIm3*>(pline + (i + xRegion.i) * 3);

					if (!off_diagonal) {

						//even w.r.t. to x so output is purely real
						tensor[i + j * nxRegion + k * nxRegion * N.y] = DBL3(value.x.Re, value.y.Re, value.z.Re);
					}
					else {

						//Dxy : odd x, Dxz : odd x, Dyz : even x
						tensor[i + j * nxRegion + k * nxRegion * N.y] = DBL3(value.x.Im, value.y.Im, value.z.Re);
					}
				}
			}
		}

		//2. FFTs along y
		for (int k = 0; k < N.z; k++) {
			for (int i = 0; i < nxRegion; i++) {

				//fetch line from fft array
				for (int j = 0; j < N.y; j++) {

					int idx_in = i + j * nxRegion + k * nxRegion * N.y;

					*reinterpret_cast<DBL3*>(pline_real + j * 3) = tensor[idx_in];
				}

				//fft on line
				fftw_execute(plan_fwd_y);

				//pack into lower half of tensor column for next step (keep same row and plane strides)
				for (int j = 0; j < N.y / 2 + 1; j++) {

					ReIm3 value = *reinterpret_cast<ReIm3*>(pline + j * 3);

					if (!off_diagonal) {

						//even w.r.t. to y so output is purely real
						tensor[i + j * nxRegion + k * nxRegion * (N.y / 2 + 1)] = DBL3(value.x.Re, value.y.Re, value.z.Re);
					}
					else {

						//Dxy : odd y, Dxz : even y, Dyz : odd y
						tensor[i + j * nxRegion + k * nxRegion * (N.y / 2 + 1)] = DBL3(value.x.Im, value.y.Re, value.z.Im);
					}
				}
			}
		}

		//3. FFTs along z
		for (int j = 0; j < N.y / 2 + 1; j++) {
			for (int i = 0; i < nxRegion; i++) {

				//fetch line from fft array (zero padding kept)
				for (int k = 0; k < N.z; k++) {

					int idx_in = i + j * nxRegion + k * nxRegion * (N.y / 2 + 1);

					*reinterpret_cast<DBL3*>(pline_real + k * 3) = tensor[idx_in];
				}

				//fft on line
				fftw_execute(plan_fwd_z);

				//pack into output kernels with reduced strides
				for (int k = 0; k < N.z / 2 + 1; k++) {

					ReIm3 value = *reinterpret_cast<ReIm3*>(pline + k * 3);

					if (!off_diagonal) {

						//even w.r.t. to z so output is purely real

						if (!transpose_xy) {
							
							kernel[i + j * nxRegion + k * nxRegion * (N.y / 2 + 1)] = DBL3(value.x.Re, value.y.Re, value.z.Re);
						}
						else {

							kernel[j + i * (N.y / 2 + 1) + k * nxRegion * (N.y / 2 + 1)] = DBL3(value.x.Re, value.y.Re, value.z.Re);
						}
					}
					else {

						//Dxy : even z, Dxz : odd z, Dyz : odd z
						//Also multiply by -1 since all off-diagonal tensor elements have been odd twice
						//The final output is thus purely real but we always treated the input as purely real even when it should have been purely imaginary
						//This means we need to account for i * i = -1 at the end

						if (!transpose_xy) {
							
							kernel[i + j * nxRegion + k * nxRegion * (N.y / 2 + 1)] = DBL3(-value.x.Re, -value.y.Im, -value.z.Im);
						}
						else {

							kernel[j + i * (N.y / 2 + 1) + k * nxRegion * (N.y / 2 + 1)] = DBL3(-value.x.Re, -value.y.Im, -value.z.Im);
						}
					}
				}
			}
		}
	};
	
	//-------------- CALCULATE DIAGONAL TENSOR ELEMENTS THEN TRANSFORM INTO KERNEL

	if (pbc_images.IsNull()) {

		//no pbcs in any dimension -> use these
		if (!dtf.CalcDiagTens3D(D, n, N, h / maximum(h.x, h.y, h.z), include_self_demag)) return error(BERROR_OUTOFMEMORY_NCRIT);
	}
	else {

		//pbcs used in at least one dimension
		if (!dtf.CalcDiagTens3D_PBC(D, N, h / maximum(h.x, h.y, h.z), true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFMEMORY_NCRIT);
	}

	tensor_to_kernel(D, K_cpu, false);

	//transfer to GPU
	Kdiag()->copy_from_cpuvec(K_cpu);

	//-------------- CALCULATE OFF-DIAGONAL TENSOR ELEMENTS THEN TRANSFORM INTO KERNEL

	if (pbc_images.IsNull()) {

		//no pbcs in any dimension -> use these
		if (!dtf.CalcOffDiagTens3D(D, n, N, h / maximum(h.x, h.y, h.z))) return error(BERROR_OUTOFMEMORY_NCRIT);
	}
	else {

		//pbcs used in at least one dimension
		if (!dtf.CalcOffDiagTens3D_PBC(D, N, h / maximum(h.x, h.y, h.z), true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFMEMORY_NCRIT);
	}

	tensor_to_kernel(D, K_cpu, true);

	//transfer to GPU
	Kodiag()->copy_from_cpuvec(K_cpu);
	
	//-------------- CLEANUP
	
	fftw_destroy_plan(plan_fwd_x);
	fftw_destroy_plan(plan_fwd_y);
	fftw_destroy_plan(plan_fwd_z);

	fftw_free((double*)pline_real);
	fftw_free((fftw_complex*)pline);
	
	//Done
	return error;
}

BError DemagKernelCUDA::Calculate_Demag_Kernels_3D_onGPU(bool include_self_demag)
{
	BError error(__FUNCTION__);

	//-------------------------------------------

	cu_arr<cufftDoubleReal> cuInx, cuIny, cuInz;
	if (!cuInx.resize(N.x*N.y*N.z)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	if (!cuIny.resize(N.x*N.y*N.z)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	if (!cuInz.resize(N.x*N.y*N.z)) return error(BERROR_OUTOFGPUMEMORY_CRIT);

	cu_arr<cufftDoubleComplex> cuOut;
	if (!cuOut.resize((N.x / 2 + 1)*N.y*N.z)) return error(BERROR_OUTOFGPUMEMORY_CRIT);

	//-------------- DEMAG TENSOR

	//Demag tensor components
	//
	// D11 D12 D13
	// D12 D22 D23
	// D13 D23 D33

	DemagTFuncCUDA dtf_gpu;
	
	if (pbc_images.IsNull()) {

		if (!dtf_gpu.CalcDiagTens3D(cuInx, cuIny, cuInz, n, N, h / maximum(h.x, h.y, h.z), include_self_demag)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}
	else {

		if (!dtf_gpu.CalcDiagTens3D_PBC(
			cuInx, cuIny, cuInz, N, h / maximum(h.x, h.y, h.z),
			true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}
	
	//-------------- SETUP FFT

	cufftHandle planTens3D_x, planTens3D_y, planTens3D_z;

	int embed[1] = { 0 };
	int ndims_x[1] = { (int)N.x };
	int ndims_y[1] = { (int)N.y };
	int ndims_z[1] = { (int)N.z };

	//Forward fft along x direction (out-of-place):
	cufftResult cuffterr = cufftPlanMany(&planTens3D_x, 1, ndims_x,
		embed, 1, N.x,
		embed, 1, (N.x / 2 + 1),
		CUFFT_D2Z, (int)N.y*N.z);

	//Forward fft along y direction (out-of-place):
	cuffterr = cufftPlanMany(&planTens3D_y, 1, ndims_y,
		embed, nxRegion, 1,
		embed, nxRegion, 1,
		CUFFT_D2Z, (int)nxRegion);

	//Forward fft along z direction (out-of-place):
	cuffterr = cufftPlanMany(&planTens3D_z, 1, ndims_z,
		embed, nxRegion * (N.y / 2 + 1), 1,
		embed, nxRegion * (N.y / 2 + 1), 1,
		CUFFT_D2Z, (int)nxRegion * (N.y / 2 + 1));

	//-------------- FFT REAL TENSOR INTO REAL KERNELS

	auto fftdiagcomponent = [&](cu_arr<cufftDoubleReal>& cuIn, int component) -> void
	{
		//Now FFT along x from input to output
		cufftExecD2Z(planTens3D_x, cuIn, cuOut);

		//extract real parts from cuOut and place in cuIn
		cuOut_to_cuIn_Re(nxRegion * N.y * N.z, cuIn, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);

		//Now FFT along y from input to output
		for (int k = 0; k < N.z; k++) {

			cufftExecD2Z(planTens3D_y, cuIn + k * nxRegion * N.y, cuOut + k * nxRegion * (N.y / 2 + 1));
		}

		//extract real parts from cuOut and place in cuIn
		cuOut_to_cuIn_Re(nxRegion * (N.y / 2 + 1) * N.z, cuIn, cuOut, nxRegion, N.y / 2 + 1, 0, nxRegion);

		//Now FFT along z from input to output
		cufftExecD2Z(planTens3D_z, cuIn, cuOut);

		//extract real parts from cuOut and place in kernel
		cuOut_to_Kdiagcomponent(cuOut, component);
	};

	auto fftodiagcomponent = [&](cu_arr<cufftDoubleReal>& cuIn, int component) -> void
	{
		//Now FFT along x from input to output
		cufftExecD2Z(planTens3D_x, cuIn, cuOut);

		//extract real or imaginary parts from cuOut and place in cuIn, depending on the computed component
		if (component == 1) cuOut_to_cuIn_Im(nxRegion * N.y * N.z, cuIn, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);
		else if (component == 2) cuOut_to_cuIn_Im(nxRegion * N.y * N.z, cuIn, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);
		else if (component == 3) cuOut_to_cuIn_Re(nxRegion * N.y * N.z, cuIn, cuOut, N.x / 2 + 1, N.y, xRegion.i, xRegion.i + nxRegion);

		//Now FFT along y from input to output
		for (int k = 0; k < N.z; k++) {

			cufftExecD2Z(planTens3D_y, cuIn + k * nxRegion * N.y, cuOut + k * nxRegion * (N.y / 2 + 1));
		}

		//extract real or imaginary parts from cuOut and place in cuIn, depending on the computed component
		if (component == 1) cuOut_to_cuIn_Im((N.x / 2 + 1) * (N.y / 2 + 1) * N.z, cuIn, cuOut, nxRegion, N.y / 2 + 1, 0, nxRegion);
		else if (component == 2) cuOut_to_cuIn_Re((N.x / 2 + 1) * (N.y / 2 + 1) * N.z, cuIn, cuOut, nxRegion, N.y / 2 + 1, 0, nxRegion);
		else if (component == 3) cuOut_to_cuIn_Im((N.x / 2 + 1) * (N.y / 2 + 1) * N.z, cuIn, cuOut, nxRegion, N.y / 2 + 1, 0, nxRegion);

		//Now FFT along z from input to output
		cufftExecD2Z(planTens3D_z, cuIn, cuOut);

		//extract -(Re, Im, Im) parts from cuOut and place in kernel
		cuOut_to_Kodiagcomponent(cuOut, component);
	};

	//-------------- CALCULATE DIAGONAL TENSOR ELEMENTS THEN TRANSFORM INTO KERNEL

	//diagonal components

	fftdiagcomponent(cuInx, 1);
	fftdiagcomponent(cuIny, 2);
	fftdiagcomponent(cuInz, 3);

	//-------------- CALCULATE OFF-DIAGONAL TENSOR ELEMENTS THEN TRANSFORM INTO KERNEL
	
	if (pbc_images.IsNull()) {

		//no pbcs in any dimension -> use these
		if (!dtf_gpu.CalcOffDiagTens3D(cuInx, cuIny, cuInz, n, N, h / maximum(h.x, h.y, h.z))) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}
	else {

		//pbcs used in at least one dimension
		if (!dtf_gpu.CalcOffDiagTens3D_PBC(
			cuInx, cuIny, cuInz, N, h / maximum(h.x, h.y, h.z),
			true, ASYMPTOTIC_DISTANCE, pbc_images.x, pbc_images.y, pbc_images.z)) return error(BERROR_OUTOFGPUMEMORY_NCRIT);
	}

	//off-diagonal components
	fftodiagcomponent(cuInx, 1);
	fftodiagcomponent(cuIny, 2);
	fftodiagcomponent(cuInz, 3);

	//-------------- CLEANUP

	cufftDestroy(planTens3D_x);
	cufftDestroy(planTens3D_y);
	cufftDestroy(planTens3D_z);

	return error;
}

#endif

#endif