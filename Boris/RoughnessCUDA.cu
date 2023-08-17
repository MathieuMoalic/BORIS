#include "RoughnessCUDA.h"

#if COMPILECUDA == 1

#ifdef MODULE_COMPILATION_ROUGHNESS

#include "BorisCUDALib.cuh"

#include "Mesh_FerromagneticCUDA.h"
#include "MeshDefs.h"

//----------------------- Initialization

__global__ void set_RoughnessCUDA_pointers_kernel(
	ManagedMeshCUDA& cuMesh, cuVEC<cuReal3>& Fmul_rough, cuVEC<cuReal3>& Fomul_rough)
{
	if (threadIdx.x == 0) cuMesh.pFmul_rough = &Fmul_rough;
	if (threadIdx.x == 1) cuMesh.pFomul_rough = &Fomul_rough;
}

void RoughnessCUDA::set_RoughnessCUDA_pointers(void)
{
	for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

		set_RoughnessCUDA_pointers_kernel <<< 1, CUDATHREADS >>>
			(pMeshCUDA->cuMesh.get_deviceobject(mGPU), Fmul_rough.get_deviceobject(mGPU), Fomul_rough.get_deviceobject(mGPU));
	}
}

__global__ void RoughnessCUDA_FM_UpdateField_Kernel(ManagedMeshCUDA& cuMesh, cuVEC<cuReal3>& Fmul_rough, cuVEC<cuReal3>& Fomul_rough, ManagedModulesCUDA& cuModule, bool do_reduction)
{
	cuVEC_VC<cuReal3>& M = *cuMesh.pM;
	cuVEC<cuReal3>& Heff = *cuMesh.pHeff;

	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuBReal energy_ = 0.0;

	if (idx < Heff.linear_size()) {

		cuReal3 Hrough = cuReal3();

		if (M.is_not_empty(idx)) {

			Hrough = cuReal33(
				cuReal3(Fmul_rough[idx].x, Fomul_rough[idx].x, Fomul_rough[idx].y),
				cuReal3(Fomul_rough[idx].x, Fmul_rough[idx].y, Fomul_rough[idx].z),
				cuReal3(Fomul_rough[idx].y, Fomul_rough[idx].z, Fmul_rough[idx].z)) * M[idx];

			if (do_reduction) {

				int non_empty_cells = M.get_nonempty_cells();
				if (non_empty_cells) energy_ = -(cuBReal)MU0 * M[idx] * Hrough / (2 * non_empty_cells);
			}

			if (do_reduction && cuModule.pModule_Heff->linear_size()) (*cuModule.pModule_Heff)[idx] = Hrough;
			if (do_reduction && cuModule.pModule_energy->linear_size()) (*cuModule.pModule_energy)[idx] = -(cuBReal)MU0 * M[idx] * Hrough / 2;
		}

		Heff[idx] += Hrough;
	}

	if (do_reduction) reduction_sum(0, 1, &energy_, *cuModule.penergy);
}

__global__ void RoughnessCUDA_AFM_UpdateField_Kernel(ManagedMeshCUDA& cuMesh, cuVEC<cuReal3>& Fmul_rough, cuVEC<cuReal3>& Fomul_rough, ManagedModulesCUDA& cuModule, bool do_reduction)
{
	cuVEC_VC<cuReal3>& M = *cuMesh.pM;
	cuVEC<cuReal3>& Heff = *cuMesh.pHeff;

	cuVEC_VC<cuReal3>& M2 = *cuMesh.pM2;
	cuVEC<cuReal3>& Heff2 = *cuMesh.pHeff2;

	int idx = blockIdx.x * blockDim.x + threadIdx.x;

	cuBReal energy_ = 0.0;

	if (idx < Heff.linear_size()) {

		cuReal3 Hrough = cuReal3();

		if (M.is_not_empty(idx)) {

			Hrough = cuReal33(
				cuReal3(Fmul_rough[idx].x, Fomul_rough[idx].x, Fomul_rough[idx].y),
				cuReal3(Fomul_rough[idx].x, Fmul_rough[idx].y, Fomul_rough[idx].z),
				cuReal3(Fomul_rough[idx].y, Fomul_rough[idx].z, Fmul_rough[idx].z)) * (M[idx] + M2[idx]) / 2;

			if (do_reduction) {

				int non_empty_cells = M.get_nonempty_cells();
				if (non_empty_cells) energy_ = -(cuBReal)MU0 * (M[idx] + M2[idx]) * Hrough / (4 * non_empty_cells);
			}

			if (do_reduction && cuModule.pModule_Heff->linear_size()) (*cuModule.pModule_Heff)[idx] = Hrough;
			if (do_reduction && cuModule.pModule_Heff2->linear_size()) (*cuModule.pModule_Heff2)[idx] = Hrough;
			if (do_reduction && cuModule.pModule_energy->linear_size()) (*cuModule.pModule_energy)[idx] = -MU0 * M[idx] * Hrough / 2;
			if (do_reduction && cuModule.pModule_energy2->linear_size()) (*cuModule.pModule_energy2)[idx] = -MU0 * M2[idx] * Hrough / 2;
		}

		Heff[idx] += Hrough;
		Heff2[idx] += Hrough;
	}

	if (do_reduction) reduction_sum(0, 1, &energy_, *cuModule.penergy);
}

void RoughnessCUDA::UpdateField(void)
{
	if (pMeshCUDA->GetMeshType() == MESH_FERROMAGNETIC) {

		if (pMeshCUDA->CurrentTimeStepSolved()) {

			ZeroEnergy();

			for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

				RoughnessCUDA_FM_UpdateField_Kernel <<< (pMeshCUDA->M.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> 
					(pMeshCUDA->cuMesh.get_deviceobject(mGPU), Fmul_rough.get_deviceobject(mGPU), Fomul_rough.get_deviceobject(mGPU), cuModule.get_deviceobject(mGPU), true);
			}
		}
		else {

			for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

				RoughnessCUDA_FM_UpdateField_Kernel <<< (pMeshCUDA->M.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> 
					(pMeshCUDA->cuMesh.get_deviceobject(mGPU), Fmul_rough.get_deviceobject(mGPU), Fomul_rough.get_deviceobject(mGPU), cuModule.get_deviceobject(mGPU), false);
			}
		}
	}

	else if (pMeshCUDA->GetMeshType() == MESH_ANTIFERROMAGNETIC) {

		if (pMeshCUDA->CurrentTimeStepSolved()) {

			ZeroEnergy();

			for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

				RoughnessCUDA_AFM_UpdateField_Kernel <<< (pMeshCUDA->M.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> 
					(pMeshCUDA->cuMesh.get_deviceobject(mGPU), Fmul_rough.get_deviceobject(mGPU), Fomul_rough.get_deviceobject(mGPU), cuModule.get_deviceobject(mGPU), true);
			}
		}
		else {

			for (mGPU.device_begin(); mGPU != mGPU.device_end(); mGPU++) {

				RoughnessCUDA_AFM_UpdateField_Kernel <<< (pMeshCUDA->M.device_size(mGPU) + CUDATHREADS) / CUDATHREADS, CUDATHREADS >>> 
					(pMeshCUDA->cuMesh.get_deviceobject(mGPU), Fmul_rough.get_deviceobject(mGPU), Fomul_rough.get_deviceobject(mGPU), cuModule.get_deviceobject(mGPU), false);
			}
		}
	}
}

#endif

#endif