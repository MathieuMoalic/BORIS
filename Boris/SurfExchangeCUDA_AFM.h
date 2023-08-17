#pragma once

#include "Boris_Enums_Defs.h"
#if COMPILECUDA == 1

#ifdef MODULE_COMPILATION_SURFEXCHANGE

#include "BorisCUDALib.h"
#include "ModulesCUDA.h"

class MeshCUDA;
class SurfExchange_AFM;
class ManagedMeshCUDA;
class ManagedAtom_MeshCUDA;

class SurfExchangeCUDA_AFM :
	public ModulesCUDA
{

private:

	//pointer to CUDA version of mesh object holding the effective field module holding this CUDA module
	MeshCUDA* pMeshCUDA;

	//pointer to cpu version of SurfExchange
	SurfExchange_AFM* pSurfExch;

	//cu arrays with pointers to other meshes in surface exchange coupling with the mesh holding this module, top and bottom, (ferromagnetic)
	mcu_arr<ManagedMeshCUDA> pMeshFM_Bot;
	mcu_arr<ManagedMeshCUDA> pMeshFM_Top;

	//cu arrays with pointers to other meshes in surface exchange coupling with the mesh holding this module, top and bottom, (two-sublattice model meshes)
	mcu_arr<ManagedMeshCUDA> pMeshAFM_Bot;
	mcu_arr<ManagedMeshCUDA> pMeshAFM_Top;

	//cu arrays with pointers to other meshes in surface exchange coupling with the mesh holding this module, top and bottom, (atomistic)
	mcu_arr<ManagedAtom_MeshCUDA> pMeshAtom_Bot;
	mcu_arr<ManagedAtom_MeshCUDA> pMeshAtom_Top;

private:

	//Set pointers in ManagedMeshCUDA so we can access them in device code. This is used by MonteCarlo algorithm.
	void set_SurfExchangeCUDA_AFM_pointers(void);

public:

	SurfExchangeCUDA_AFM(MeshCUDA* pMeshCUDA_, SurfExchange_AFM* pSurfExch_);
	~SurfExchangeCUDA_AFM();

	//-------------------Abstract base class method implementations

	void Uninitialize(void) { initialized = false; }

	BError Initialize(void);

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage);
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage) {}

	void UpdateField(void);

	//-------------------Torque methods

	cuReal3 GetTorque(cuRect avRect);
};

#else

class SurfExchangeCUDA_AFM
{
};

#endif

#endif