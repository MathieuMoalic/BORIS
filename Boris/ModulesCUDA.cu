#include "ModulesCUDA.h"

#if COMPILECUDA == 1

#include "BorisCUDALib.cuh"

__global__ void ZeroEnergy_kernel(cuBReal& energy, cuReal3& torque, size_t& points_count)
{
	if (threadIdx.x == 0) energy = 0.0;
	if (threadIdx.x == 1) torque = 0.0;
	if (threadIdx.x == 2) points_count = 0;
}

void ModulesCUDA::ZeroEnergy(void)
{
	for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

		ZeroEnergy_kernel <<< 1, CUDATHREADS >>> (energy(mGPU), torque(mGPU), points_count(mGPU));
	}
}

__global__ void ZeroModuleVECs_kernel(cuVEC<cuReal3>& Module_Heff, cuVEC<cuReal3>& Module_Heff2, cuVEC<cuBReal>& Module_energy, cuVEC<cuBReal>& Module_energy2)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < Module_Heff.linear_size()) Module_Heff[idx] = cuReal3();
	if (idx < Module_Heff2.linear_size()) Module_Heff2[idx] = cuReal3();
	if (idx < Module_energy.linear_size()) Module_energy[idx] = 0.0;
	if (idx < Module_energy2.linear_size()) Module_energy2[idx] = 0.0;
}

void ModulesCUDA::ZeroModuleVECs(void)
{
	//This method is used at runtime, so better use a single kernel launch rather than zeroing them separately

	for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

		ZeroModuleVECs_kernel <<< (Module_Heff.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>>
			(Module_Heff.get_deviceobject(mGPU), Module_Heff2.get_deviceobject(mGPU), Module_energy.get_deviceobject(mGPU), Module_energy2.get_deviceobject(mGPU));
	}
}

//-------------------------- Effective field and energy VECs

//Make sure memory is allocated correctly for display data if used, else free memory
BError ModulesCUDA::Update_Module_Display_VECs(cuReal3 h, cuRect meshRect, bool Module_Heff_used, bool Module_Energy_used, bool twosublattice)
{
	BError error(CLASS_STR(ModulesCUDA));

	//1. Heff - sub-lattice A

	if (Module_Heff.size_cpu().dim()) {

		if (Module_Heff_used && !Module_Heff.resize(h, meshRect)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		else if (!Module_Heff_used) Module_Heff.clear();	
	}
	else if (Module_Heff_used) {

		if (!Module_Heff.assign(h, meshRect, cuReal3())) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	}

	//2. Heff - sub-lattice B

	if (Module_Heff2.size_cpu().dim()) {

		if (twosublattice && Module_Heff_used && !Module_Heff2.resize(h, meshRect)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		else Module_Heff2.clear();
	}
	else if (twosublattice && Module_Heff_used) {

		if (!Module_Heff2.assign(h, meshRect, cuReal3())) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	}

	//3. Energy Density - sub-lattice A

	if (Module_energy.size_cpu().dim()) {

		if (Module_Energy_used && !Module_energy.resize(h, meshRect)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		else if (!Module_Energy_used) Module_energy.clear();
	}
	else if (Module_Energy_used) {

		if (!Module_energy.assign(h, meshRect, 0.0)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	}

	//4. Energy Density - sub-lattice B

	if (Module_energy2.size_cpu().dim()) {

		if (twosublattice && Module_Energy_used && !Module_energy2.resize(h, meshRect)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		else Module_energy2.clear();
	}
	else if (twosublattice && Module_Energy_used) {

		if (!Module_energy2.assign(h, meshRect, 0.0)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	}

	return error;
}

//-------------------------- Torque

__global__ void CalculateTorque_kernel(cuVEC_VC<cuReal3>& M, cuVEC<cuReal3>& Module_Heff, cuRect avRect, cuReal3& torque, size_t& points_count)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuSZ3& n = M.n;

	cuReal3 torque_ = cuReal3();
	bool include_in_average = false;

	if (idx < M.linear_size()) {

		cuINT3 ijk = cuINT3(idx % n.x, (idx / n.x) % n.y, idx / (n.x*n.y));

		if (M.box_from_rect_max(avRect + M.rect.s).contains(ijk) && M.is_not_empty(ijk)) {

			torque_ = M[ijk] ^ Module_Heff[ijk];
			include_in_average = true;
		}
	}

	//need the idx < n.dim() check before cuvec.is_not_empty(ijk) to avoid bad memory access
	reduction_avg(0, 1, &torque_, torque, points_count, include_in_average);
}

//return cross product of M with Module_Heff, averaged in given rect (relative)
cuReal3 ModulesCUDA::CalculateTorque(mcu_VEC_VC(cuReal3)& M, cuRect& avRect)
{
	if (!Module_Heff.linear_size_cpu()) return cuReal3();

	ZeroEnergy();

	for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

		CalculateTorque_kernel <<< (M.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>>
			(M.get_deviceobject(mGPU), Module_Heff.get_deviceobject(mGPU), avRect, torque(mGPU), points_count(mGPU));
	}

	size_t points_count_cpu = points_count.to_cpu_sum();

	if (points_count_cpu) return torque.to_cpu_sum() / points_count_cpu;
	else return cuReal3();
}

#endif