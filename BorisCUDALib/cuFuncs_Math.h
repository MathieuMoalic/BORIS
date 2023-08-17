//General purpose math functions

#pragma once

#include <math.h>

#include "cuTypes_VAL.h"

///////////////////////////////////////////////////////////////////////////////

//get magnitude for fundamental types - need this to make cuVEC work for both fundamental and composite types
template <typename Type, std::enable_if_t<std::is_integral<Type>::value>* = nullptr>
__host__ __device__ Type cu_GetMagnitude(const Type& V)
{
	return V * (2 * (V >= 0) - 1);
}

//get magnitude for fundamental types - need this to make cuVEC work for both fundamental and composite types
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
__host__ __device__ Type cu_GetMagnitude(const Type& V)
{
	return fabs(V);
}

//get magnitude for fundamental types using 3 components
template <typename Type, std::enable_if_t<std::is_fundamental<Type>::value>* = nullptr>
__host__ __device__ Type cu_GetMagnitude(const Type& Vx, const Type& Vy, const Type& Vz)
{ 
	return sqrt(Vx*Vx + Vy * Vy + Vz * Vz); 
}

//get magnitude for cuVAL3 types (a cuVAL3 type can be identified by checking if an cuINT3 can be converted to it.
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ auto cu_GetMagnitude(const cuVAL3Type& V) -> decltype(std::declval<cuVAL3Type>().z)
{
	return sqrt(V.x*V.x + V.y*V.y + V.z*V.z);
}

//get magnitude for cuVAL4 types (a cuVAL4 type can be identified by checking if an cuINT4 can be converted to it.
template <typename cuVAL4Type, std::enable_if_t<std::is_convertible<cuINT4, cuVAL4Type>::value>* = nullptr>
__host__ __device__ auto cu_GetMagnitude(const cuVAL4Type& V) -> decltype(std::declval<cuVAL4Type>().t)
{
	return sqrt(V.x*V.x + V.y*V.y + V.z*V.z + V.t*V.t);
}

template <typename Type, std::enable_if_t<std::is_same<Type, cuBComplex>::value>* = nullptr>
__host__ __device__ cuBReal cu_GetMagnitude(const Type& V)
{
	return sqrt(V.x*V.x + V.y*V.y);
}

template <typename Type, std::enable_if_t<std::is_same<Type, cuReIm>::value>* = nullptr>
__host__ __device__ cuBReal cu_GetMagnitude(const Type& V)
{
	return sqrt(V.Re*V.Re + V.Im*V.Im);
}

template <typename Type, std::enable_if_t<std::is_same<Type, cuReIm3>::value>* = nullptr>
__host__ __device__ cuReal3 cu_GetMagnitude(const Type& V)
{
	return cuReal3(cu_GetMagnitude(V.x), cu_GetMagnitude(V.y), cu_GetMagnitude(V.z));
}

//obtain distance between two Cartesian coordinates specified as a cuVAL3
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ auto cu_get_distance(const cuVAL3Type& coord1, const cuVAL3Type& coord2) -> decltype(std::declval<cuVAL3Type>().z)
{
	return (coord2 - coord1).norm();
}

///////////////////////////////////////////////////////////////////////////////

template <typename Type, std::enable_if_t<std::is_fundamental<Type>::value>* = nullptr>
__host__ __device__ void cu_NormalizeVector(Type &Vx, Type &Vy, Type &Vz)
{
	Type magnitude = cu_GetMagnitude(Vx, Vy, Vz);

	if (cuIsNZ(magnitude)) {

		Vx /= magnitude;
		Vy /= magnitude;
		Vz /= magnitude;
	}
}

//normalize a VAL3 if possible, else return zero value
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuVAL3Type cu_normalize(const cuVAL3Type& val3) { return (val3 != cuVAL3Type() ? val3.normalized() : cuVAL3Type()); }

//normalize a VAL3 to the norm of another VAL3
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuVAL3Type cu_normalize(const cuVAL3Type& val3, const cuVAL3Type& val3_2) { return (val3_2 != cuVAL3Type() ? val3 / val3_2.norm() : cuVAL3Type()); }

//normalize a VAL33 to the norm of a VAL3
template <typename cuVAL33Type, typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT33, cuVAL33Type>::value>* = nullptr>
__host__ __device__ cuVAL33Type cu_normalize(const cuVAL33Type& val33, const cuVAL3Type& val3) { return (val3 != cuVAL3Type() ? val33 / val3.norm() : cuVAL33Type()); }

///////////////////////////////////////////////////////////////////////////////

template <typename RType>
__device__ RType cu_maximum(RType param1, RType param2)
{
	return (param1 > param2 ? param1 : param2);
}

template <typename RType, typename ... PType>
__device__ RType cu_maximum(RType param, PType ... params)
{
	RType value = cu_maximum(params...);
	return (param > value ? param : value);
}

template <typename RType>
__device__ RType cu_minimum(RType param1, RType param2)
{
	return (param1 < param2 ? param1 : param2);
}

template <typename RType, typename ... PType>
__device__ RType cu_minimum(RType param, PType ... params)
{
	RType value = cu_minimum(params...);
	return (param < value ? param : value);
}

///////////////////////////////////////////////////////////////////////////////

//Note : round is now a standard function since C++14 (in math.h), so no need to define it here as per previous C++ standards
//previous:
//inline int round(float fval) { return (fval < 0.0) ? (int)ceil(fval - 0.5) : (int)floor(fval + 0.5); }
//inline int round(double fval) { return (fval < 0.0) ? (int)ceil(fval - 0.5) : (int)floor(fval + 0.5); }

template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuINT3 cu_round(const cuVAL3Type& val3) { return cuINT3(round(val3.x), round(val3.y), round(val3.z)); }

template <typename cuVAL4Type, std::enable_if_t<std::is_convertible<cuINT4, cuVAL4Type>::value>* = nullptr>
__host__ __device__ cuINT4 cu_round(const cuVAL4Type& val4) { return cuVcuINT4AL4Type(round(val4.x), round(val4.y), round(val4.z), round(val4.t)); }

//"fixed" floor function.
//Note, using just the standard floor is not good enough : if the floating point value is very close to the upper integer value then its value should be equal to it.
//The standard floor function will result in "wrong" behaviour by returning the lower integer.
//This problem typically arises when using floor and ceil on result of arithmetic operations as floating point errors are introduced, and is much worse in single precision.
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
__host__ __device__ int cu_floor_epsilon(const Type& fval) { return floor(fval + CUEPSILON_ROUNDING); }

//as above but with ceil
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
__host__ __device__ int cu_ceil_epsilon(const Type& fval) { return ceil(fval - CUEPSILON_ROUNDING); }

//return "fixed" floor of each VAL3 component
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuINT3 cu_floor(const cuVAL3Type& fval) { return cuINT3(cu_floor_epsilon(fval.x), cu_floor_epsilon(fval.y), cu_floor_epsilon(fval.z)); }

//return "fixed" ceil of each DBL3 component.
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuINT3 cu_ceil(const cuVAL3Type& fval) { return cuINT3(cu_ceil_epsilon(fval.x), cu_ceil_epsilon(fval.y), cu_ceil_epsilon(fval.z)); }

//return "fixed" floor of each VAL4 component
template <typename cuVAL4Type, std::enable_if_t<std::is_convertible<cuINT4, cuVAL4Type>::value>* = nullptr>
__host__ __device__ cuINT4 cu_floor(const cuVAL4Type& fval) { return cuINT4(cu_floor_epsilon(fval.x), cu_floor_epsilon(fval.y), cu_floor_epsilon(fval.z), cu_floor_epsilon(fval.t)); }

//return "fixed" ceil of each DBL4 component.
template <typename cuVAL4Type, std::enable_if_t<std::is_convertible<cuINT4, cuVAL4Type>::value>* = nullptr>
__host__ __device__ cuINT4 cu_ceil(const cuVAL4Type& fval) { return cuINT4(cu_ceil_epsilon(fval.x), cu_ceil_epsilon(fval.y), cu_ceil_epsilon(fval.z), cu_ceil_epsilon(fval.t)); }

//absolute value for a floating point number
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
__host__ __device__ Type cu_mod(const Type& fval) { return fabs(fval); }

//absolute value for an integer
template <typename Type, std::enable_if_t<std::is_integral<Type>::value>* = nullptr>
__host__ __device__ Type cu_mod(const Type& ival) { return ival * (2 * (ival >= 0) - 1); }

//absolute value for a VAL3
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuVAL3Type cu_mod(const cuVAL3Type& fval) { return cuVAL3Type(cu_mod(fval.x), cu_mod(fval.y), cu_mod(fval.z)); }

//absolute value for a VAL4
template <typename cuVAL4Type, std::enable_if_t<std::is_convertible<cuINT4, cuVAL4Type>::value>* = nullptr>
__host__ __device__ cuVAL4Type cu_mod(const cuVAL4Type& fval) { return cuVAL4Type(cu_mod(fval.x), cu_mod(fval.y), cu_mod(fval.z), cu_mod(fval.t)); }

//get sign of integer value or zero : -1, 0, +1
template <typename Type, std::enable_if_t<std::is_integral<Type>::value>* = nullptr>
__host__ __device__ int cu_get_sign(const Type& ival) { return (ival != 0) * (2 * (ival > 0) - 1); }

//get sign of floating point value : -1 or +1
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
__host__ __device__ int cu_get_sign(const Type& fval) { return (2 * (fval > 0) - 1); }

//remainder after division (using floating point numbers) - fixed version of fmod from <cmath>
template <typename Type, std::enable_if_t<std::is_floating_point<Type>::value>* = nullptr>
Type cu_fmod_epsilon(const Type& fval, const Type& denom) { return fval - cu_floor_epsilon(fval / denom) * denom; }

//fixed fmod for a VAL3
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
cuVAL3Type cu_fmod_epsilon(const cuVAL3Type& fval, cuBReal denom) { return cuVAL3Type(cu_fmod_epsilon(fval.x, denom), cu_fmod_epsilon(fval.y, denom), cu_fmod_epsilon(fval.z, denom)); }

///////////////////////////////////////////////////////////////////////////////

//Use linear interpolation/extrapolation to obtain the y value at the given x, where
//p0 = (x0, y0) and p1 = (x1, y1). y = [(x1 - x) * y0 - (x0 - x) * y1] / (x1 - x0)
//p0 and p1 cannot have the same x coordinate - this is not checked here
template <typename cuVAL2Type, std::enable_if_t<std::is_convertible<cuINT2, cuVAL2Type>::value>* = nullptr>
__host__ __device__ auto cu_interpolate(const cuVAL2Type& p0, const cuVAL2Type& p1, decltype(std::declval<cuVAL2Type>().x) x) -> decltype(std::declval<cuVAL2Type>().y)
{
	return ((p1.x - x) * p0.y - (p0.x - x) * p1.y) / (p1.x - p0.x);
}

//parametric interpolation where parameter = 0 gives start, parameter = 1 gives end.
template <typename VType>
__host__ __device__ VType cu_parametric_interpolation(const VType& start, const VType& end, double parameter)
{
	return (start * (1 - parameter) + end * parameter);
}

///////////////////////////////////////////////////////////////////////////////

//solve line equation for line passing through point1 and point2, where the x coordinate has been provided in *pSolutionPoint - fill the other 2 and return true. If no solution then return false.
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ bool cu_solve_line_equation_fixed_x(const cuVAL3Type& point1, const cuVAL3Type& point2, cuVAL3Type* pSolutionPoint)
{
	typedef decltype(std::declval<cuVAL3Type>().z) Type;

	cuVAL3Type direction = point2 - point1;

	if (cuIsNZ(direction.x)) {

		Type t = ((*pSolutionPoint).x - point1.x) / direction.x;
		*pSolutionPoint = direction * t + point1;

		return true;
	}
	else return false;
}

//solve line equation for line passing through point1 and point2, where the y coordinate has been provided in *pSolutionPoint - fill the other 2 and return true. If no solution then return false.
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ bool cu_solve_line_equation_fixed_y(const cuVAL3Type& point1, const cuVAL3Type& point2, cuVAL3Type* pSolutionPoint)
{
	typedef decltype(std::declval<cuVAL3Type>().z) Type;

	cuVAL3Type direction = point2 - point1;

	if (cuIsNZ(direction.y)) {

		Type t = ((*pSolutionPoint).y - point1.y) / direction.y;
		*pSolutionPoint = direction * t + point1;

		return true;
	}
	else return false;
}

//solve line equation for line passing through point1 and point2, where the z coordinate has been provided in *pSolutionPoint - fill the other 2 and return true. If no solution then return false.
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ bool cu_solve_line_equation_fixed_z(const cuVAL3Type& point1, const cuVAL3Type& point2, cuVAL3Type* pSolutionPoint)
{
	typedef decltype(std::declval<cuVAL3Type>().z) Type;

	cuVAL3Type direction = point2 - point1;

	if (cuIsNZ(direction.z)) {

		Type t = ((*pSolutionPoint).z - point1.z) / direction.z;
		*pSolutionPoint = direction * t + point1;

		return true;
	}
	else return false;
}

///////////////////////////////////////////////////////////////////////////////

//return matrix multiplication of rank-3 unit antisymmetric tensor with a VAL3 - return type is a VAL33
template <typename cuVAL3Type, std::enable_if_t<std::is_convertible<cuINT3, cuVAL3Type>::value>* = nullptr>
__host__ __device__ cuVAL3<cuVAL3Type> cu_epsilon3(const cuVAL3Type& val3)
{
	return cuVAL3<cuVAL3Type>(
		cuVAL3Type(0, val3.z, -val3.y),
		cuVAL3Type(-val3.z, 0, val3.x),
		cuVAL3Type(val3.y, -val3.x, 0)
		);
}

///////////////////////////////////////////////////////////////////////////////

//overloaded inverse method so can be used in templated methods
template <typename Type, std::enable_if_t<std::is_same<Type, cuBReal>::value>* = nullptr>
__device__ cuBReal cu_inverse(const cuBReal& m)
{
	if (m) return 1 / m;
	else return 0.0;
}

//3x3 matrix inverse
template <typename Type, std::enable_if_t<std::is_same<Type, cuReal33>::value>* = nullptr>
__device__ cuReal33 cu_inverse(const cuReal33& m)
{
	//in cuReal33 we use row-major notation: first index is the row, the second is the column

	cuBReal d_11 = m.j.j * m.k.k - m.j.k * m.k.j;
	cuBReal d_21 = m.j.k * m.k.i - m.j.i * m.k.k;
	cuBReal d_31 = m.j.i * m.k.j - m.j.j * m.k.i;

	cuBReal det = m.i.i * d_11 + m.i.j * d_21 + m.i.k * d_31;
	if (cuIsZ(det)) return cuReal33();

	cuBReal d_12 = m.i.k * m.k.j - m.i.j * m.k.k;
	cuBReal d_22 = m.i.i * m.k.k - m.i.k * m.k.i;
	cuBReal d_32 = m.i.j * m.k.i - m.i.i * m.k.j;

	cuBReal d_13 = m.i.j * m.j.k - m.i.k * m.j.j;
	cuBReal d_23 = m.i.k * m.j.i - m.i.i * m.j.k;
	cuBReal d_33 = m.i.i * m.j.j - m.i.j * m.j.i;

	return cuReal33(
		cuReal3(d_11, d_12, d_13) / det,
		cuReal3(d_21, d_22, d_23) / det,
		cuReal3(d_31, d_32, d_33) / det);
}

//overloaded so can be used in templated methods : simple identity
template <typename Type, std::enable_if_t<std::is_same<Type, cuBReal>::value>* = nullptr>
__device__ cuBReal cu_ident(void) { return 1.0; }

//3x3 matrix identity
template <typename Type, std::enable_if_t<std::is_same<Type, cuReal33>::value>* = nullptr>
__device__ cuReal33 cu_ident(void)
{
	return cuReal33(
		cuReal3(1.0, 0.0, 0.0),
		cuReal3(0.0, 1.0, 0.0),
		cuReal3(0.0, 0.0, 1.0));
}

///////////////////////////////////////////////////////////////////////////////

//This function returns the solution of s = a * m^s + b * m^m^s + f
//i.e. solve for s, where m, s, f are cuReal3, ^ is the cross product, a and b are constants; moreover m is a unit vector.
__device__ inline cuReal3 solve_crossprod(cuBReal a, cuBReal b, const cuReal3& m, const cuReal3& f)
{
	cuBReal ab = a * a + b + b * b;

	return f + ((a*a + b * b) / (a*a + ab * ab)) * (a * (m ^ f) + ab * (m ^ (m ^ f)));
}

//This function returns the solution of s = a * m^s + b * m^m^s + f
//i.e. solve for s, where m, s, f are cuReal3, ^ is the cross product, a and b are constants; moreover m is a unit vector perpendicular to f, so that m ^ m ^ f = -f
__device__ inline cuReal3 solve_crossprod_perp(cuBReal a, cuBReal b, const cuReal3& m, const cuReal3& f)
{
	cuBReal ab = a * a + b + b * b;

	return ((cuBReal)1.0 / (a*a + ab * ab)) * ((a*a + b * ab) * m + a * (a*a + b * b) * (m ^ f));
}

///////////////////////////////////////////////////////////////////////////////

//rotate the vector r by setting the coordinate system x axis to given polar and azimuthal angles. The angles are in radians.
//i.e. if we have the unit vector n = [ cos(phi)sin(theta), sin(phi)sin(theta), cos(theta) ]
//then we rotate the coordinate system s.t. the new x axis is n : 1) rotate the x axis around z through azimuthal, obtaining new x' and y' axes. 2) rotate x' axis around y' axis through polar, obtaining new x'', y'=y'', z'' axes
//then the rotated vector r' (returned vector) is that vector which has the same components in the new coordinate system as the vector r has in the original system.
__device__ inline cuReal3 rotate_polar(const cuReal3& r, cuBReal theta, cuBReal phi)
{
	return cuReal3(
		cos(phi)*sin(theta) * r.x - sin(phi) * r.y - cos(phi)*cos(theta) * r.z,
		sin(phi)*sin(theta) * r.x + cos(phi) * r.y - sin(phi)*cos(theta) * r.z,
		cos(theta)		    * r.x - 0 + sin(theta)	 		 * r.z
	);
}

//same as above but the rotation is specified using the unit vector n
__device__ inline cuReal3 rotate_polar(const cuReal3& r, const cuReal3& n)
{
	//if n = [cos(phi)sin(theta), sin(phi)sin(theta), cos(theta)]
	//where theta ranges in [0, PI], and phi ranges in [0, 2*PI] then:

	//nxy is sin(theta)
	cuBReal nxy = sqrt(n.x*n.x + n.y*n.y);

	//then sin(phi) = ny / nxy, and cos(phi) = nx / nxy
	//n.z = cos(theta)

	if (nxy > 0) {

		return cuReal3(
			n.x * r.x - (n.y / nxy) * r.y - (n.x / nxy) * n.z * r.z,
			n.y * r.x + (n.x / nxy) * r.y - (n.y / nxy) * n.z * r.z,
			n.z * r.x - 0 + nxy * r.z
		);
	}
	else {

		//special case where n = [0, 0, n.z]

		if (n.z > 0) {

			return cuReal3(-r.z, r.y, r.x);
		}
		else {

			return cuReal3(+r.z, r.y, -r.x);
		}
	}
}

//This is the inverse operation of rotate_polar.
//i.e. for rotate polar we have A * r = r_rot, where A is the rotation matrix for theta and phi angles.
//Then here we use the A^-1 matrix, so that if r_rot is passed as argument we get back the r vector (theta and phi same angles as for rotate_polar)
__device__ inline cuReal3 invrotate_polar(const cuReal3& r, cuBReal theta, cuBReal phi)
{
	return cuReal3(
		cos(phi)*sin(theta) * r.x + sin(phi) * sin(theta) * r.y + cos(theta) * r.z,
		-sin(phi)			* r.x + cos(phi)			  * r.y + 0,
		-cos(phi)*cos(theta)* r.x - sin(phi)*cos(theta)   * r.y + sin(theta) * r.z
	);
}

//same as above but the rotation is specified using the unit vector n
__device__ inline cuReal3 invrotate_polar(const cuReal3& r, const cuReal3& n)
{
	//if n = [cos(phi)sin(theta), sin(phi)sin(theta), cos(theta)]
	//where theta ranges in [0, PI], and phi ranges in [0, 2*PI] then:

	//nxy is sin(theta)
	cuBReal nxy = sqrt(n.x*n.x + n.y*n.y);

	//then sin(phi) = ny / nxy, and cos(phi) = nx / nxy
	//n.z = cos(theta)

	if (nxy > 0) {

		return cuReal3(
			n.x * r.x + n.y * r.y + n.z	* r.z,
			-(n.y / nxy) * r.x + (n.x / nxy) * r.y,
			-(n.x / nxy) * n.z * r.x - (n.y / nxy) * n.z * r.y + nxy * r.z
		);
	}
	else {

		//special case where n = [0, 0, n.z]

		if (n.z > 0) {

			return cuReal3(+r.z, r.y, -r.x);
		}
		else {

			return cuReal3(-r.z, r.y, +r.x);
		}
	}
}

//return a vector which is rotate by theta (0 to PI) from the vector r and by phi around the vector r (0 to 2PI)
//s.t. if r is along the x axis, theta is a rotation in the xy plane, and phi is a rotation around the x axis (geometric sense rotations).
__device__ inline cuReal3 relrotate_polar(const cuReal3& r, cuBReal theta, cuBReal phi)
{
	//rotation around the x axis first
	cuReal3 rdash = cuReal3(cos(theta), sin(theta)*cos(phi), sin(phi)*sin(theta)) * r.norm();

	//now the vector to return is that vector obtained by rotating the x axis into r
	return rotate_polar(rdash, r.normalized());
}
