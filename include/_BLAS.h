#pragma once
#include <cmath>
#include <malloc.h>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <cstdio>
#include <immintrin.h>
#include <random>

//if you can change it, than change it
namespace BLAS
{
	static constexpr double Pi = 3.14159265358979323846264338327950288L;
	static constexpr double E = 2.71828182845904523536028747135266250L;

	enum class Type
	{
		Native = 0,
		Parasitic = 1,
		Non32Aligened = 2,//must be Parasitic as well...
	};
	enum class MatType
	{
		NormalMat,
		SquareMat,
		DiagonalMat,
		SymmetricMat,
		LMat,
		UMat,
		//for band mat, width == height
		BandMat,
		LBandMat,
		UBandMat,
		//sparse mat
		SparseMat,
	};

	//note: if a vec is Non32Aligened, then don't do any +-*/ operation to 
	// other vecs unless other ones has the same offset from 32-byte-aligened as this one!

	inline unsigned long long ceiling4(unsigned long long length)
	{
		return (((length - 1) >> 2) + 1) << 2;
	}
	inline unsigned long long floor(unsigned long long length)
	{
		return (length >> 2) << 2;
	}
	inline unsigned long long ceiling4(unsigned long long width, unsigned long long height)
	{
		return ((((unsigned long long(width) - 1) >> 2) + 1) << 2) * height;
	}
	inline size_t ceiling256dSize(unsigned long long length)
	{
		return (((size_t(length) - 1) >> 2) + 1) << 5;
	}
	inline size_t ceiling256dSize(unsigned long long width, unsigned long long height)
	{
		return ((((size_t(width) - 1) >> 2) + 1) << 5) * height;
	}
	inline double* malloc64d(unsigned long long length)
	{
		return (double*)_mm_malloc(length * sizeof(double), 32);
	}
	inline double* malloc256d(unsigned long long length)
	{
		return (double*)_mm_malloc(ceiling256dSize(length), 32);
	}
	inline double* malloc256d(unsigned long long width, unsigned long long height)
	{
		return (double*)_mm_malloc(ceiling256dSize(width) * height, 32);
	}
	inline void* memcpy64d(void* dst, void const* src, unsigned long long length)
	{
		return ::memcpy(dst, src, length * sizeof(double));
	}
	inline void* memcpy256d(void* dst, void const* src, unsigned long long length)
	{
		return ::memcpy(dst, src, ceiling256dSize(length));
	}
	inline void* memcpy256d(void* dst, void const* src, unsigned long long width, unsigned long long height)
	{
		return ::memcpy(dst, src, ceiling256dSize(width, height));
	}
	inline void* memset64d(void* dst, long long val, unsigned long long length)
	{
		return ::memset(dst, val, length * sizeof(double));
	}
	inline void* memset256d(void* dst, int val, unsigned long long length)
	{
		return ::memset(dst, val, ceiling256dSize(length));
	}
	inline void* memset256d(void* dst, int val, unsigned long long width, unsigned long long height)
	{
		return ::memset(dst, val, ceiling256dSize(width, height));
	}

	inline unsigned long long getPtrOffset64d(double* ptr)
	{
		return (unsigned long long(ptr) >> 3) & 3;
	}
	inline double* getPtr256d(double* ptr)
	{
		return (double*)(unsigned long long(ptr) & -32);
	}

	struct mat;
	struct cplx
	{
		double re;
		double im;

		double norm()const
		{
			return sqrt(re * re + im * im);
		}
		void print()const
		{
			::printf("(%.4e, %.4e)\n", re, im);
		}
	};
	struct vec
	{
		double* data;
		unsigned long long dim;
		unsigned long long begining;
		Type type;

		vec() :data(nullptr), dim(0), begining(0), type(Type::Native) {}
		vec(unsigned long long _length, bool _clear = true)
			:
			data(_length ? malloc256d(_length) : nullptr),
			dim(_length),
			begining(0),
			type(Type::Native)
		{
			if (_clear && data)memset256d(data, 0, _length);
		}
		vec(vec const& a)
			:
			data(a.dim ? malloc256d(a.dim) : nullptr),
			dim(a.dim),
			begining(0),//Native vec must begin with 0
			type(Type::Native)
		{
			if (dim)memcpy64d(data, a.data + a.begining, dim);
		}
		vec(vec&& a) :data(nullptr), dim(0), begining(0), type(Type::Native)
		{
			if (a.type == Type::Native)
			{
				data = a.data;
				dim = a.dim;
				a.data = nullptr;
				a.dim = 0;
			}
			else
			{
				if (a.dim)
				{
					data = malloc256d(a.dim);
					dim = a.dim;
					memcpy64d(data, a.data + a.begining, dim);
				}
			}
		}
		vec(double* _data, unsigned long long _length, Type _type)//Parasitic or Non32Aligened
			:data(getPtr256d(_data)), dim(_length), begining(getPtrOffset64d(_data)), type(_type) {}
		vec(std::initializer_list<double>const& a)
			:
			data(a.size() ? malloc256d(a.size()) : nullptr),
			dim(a.size()),
			type(Type::Native)
		{
			if (dim)memcpy64d(data, a.begin(), dim);
		}
		~vec()
		{
			if (type == Type::Native)_mm_free(data);
			data = nullptr;
			dim = 0;
		}
		inline double& operator[](unsigned long long a)
		{
			return data[a + begining];
		}
		void realloc(unsigned long long _dim, bool _clear = true)
		{
			if (type == Type::Native && _dim)
			{
				if (dim)
				{
					double* r((double*)malloc256d(_dim));
					memcpy64d(r, data, dim);
					if (_clear && _dim > dim)
						memset64d(r + dim, 0, _dim - dim);
					_mm_free(data);
					dim = _dim;
				}
				else
				{
					data = (double*)malloc256d(_dim);
					if (_clear)memset64d(data, 0, _dim);
					dim = _dim;
				}
			}
		}
		void reconstruct(unsigned long long _dim, bool _clear = true)
		{
			if (type == Type::Native)
			{
				if (dim)_mm_free(data);
				if (_dim)
				{
					data = (double*)malloc256d(_dim);
					if (_clear)memset64d(data, 0, _dim);
					dim = _dim;
				}
			}
		}
		void clearTail()
		{
			if (dim & 3)
				for (unsigned long long c0(dim); c0 < ceiling4(dim); ++c0)
					data[c0] = 0;
		}
		//moveTo
		//vec& moveTo(vec& a)
		//{
		//	if (type == Type::Native)
		//	{
		//		a.~vec();
		//		a.data = data;
		//		a.dim = dim;
		//		a.type = type;
		//		data = nullptr;
		//		dim = 0;
		//	}
		//	else a = *this;
		//	return a;
		//}

		//= += -= *= /=
		vec& operator =(vec&& a)
		{
			if (a.dim)
			{
				if (type == Type::Native)
				{
					if (a.type == Type::Native)
					{
						_mm_free(data);
						data = a.data;
						dim = a.dim;
						a.data = nullptr;
						a.dim = 0;
					}
					else
					{
						if (dim != a.dim)
						{
							_mm_free(data);
							data = malloc256d(a.dim);
							dim = a.dim;
						}
						memcpy64d(data, a.data + a.begining, dim);
					}
				}
				else
				{
					unsigned minDim(dim > a.dim ? a.dim : dim);
					memcpy64d(data, a.data + a.begining, minDim);
				}
			}
			else if (type == Type::Native)
			{
				_mm_free(data);
				data = nullptr;
				dim = 0;
			}
			return *this;
		}
		vec& operator =(vec const& a)
		{
			if (a.dim)
			{
				if (type == Type::Native)
				{
					if (dim != a.dim)
					{
						_mm_free(data);
						data = malloc256d(a.dim);
						dim = a.dim;
					}
					memcpy64d(data, a.data + a.begining, dim);
				}
				else
				{
					unsigned minDim(dim > a.dim ? a.dim : dim);
					memcpy64d(data, a.data + a.begining, minDim);
				}
			}
			else if (type == Type::Native)
			{
				_mm_free(data);
				data = nullptr;
				dim = 0;
			}
			return *this;
		}
		vec& operator+=(vec const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				for (unsigned long long c0(0); c0 < minDim; ++c0)
					data[c0] += a.data[c0];
				/*unsigned long long minDim4(minDim >> 2);
				__m256d* aData((__m256d*)data);
				__m256d* bData((__m256d*)a.data);
				unsigned long long c0(0);
				for (; c0 < minDim4; ++c0)
					aData[c0] = _mm256_add_pd(aData[c0], bData[c0]);
				if ((c0 << 2) < minDim)
					for (unsigned long long c1(c0 << 2); c1 < minDim; ++c1)
						data[c1] += a.data[c1];*/
			}
			return *this;
		}
		vec& operator-=(vec const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				for (unsigned long long c0(0); c0 < minDim; ++c0)
					data[c0] -= a.data[c0];
			}
			return *this;
		}
		vec& operator*=(vec const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				for (unsigned long long c0(0); c0 < minDim; ++c0)
					data[c0] *= a.data[c0];
			}
			return *this;
		}
		vec& operator/=(vec const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				for (unsigned long long c0(0); c0 < minDim; ++c0)
					data[c0] /= a.data[c0];
			}
			return *this;
		}
		vec& operator =(double a)
		{
			if (dim)
			{
				for (unsigned long long c0(0); c0 < dim; ++c0)
					data[c0] = a;
			}
			return *this;
		}
		vec& operator+=(double a)
		{
			if (dim)
			{
				for (unsigned long long c0(0); c0 < dim; ++c0)
					data[c0] += a;
			}
			return *this;
		}
		vec& operator-=(double a)
		{
			if (dim)
			{
				for (unsigned long long c0(0); c0 < dim; ++c0)
					data[c0] -= a;
			}
			return *this;
		}
		vec& operator*=(double a)
		{
			if (dim)
			{
				for (unsigned long long c0(0); c0 < dim; ++c0)
					data[c0] *= a;
			}
			return *this;
		}
		vec& operator/=(double a)
		{
			if (dim)
			{
				for (unsigned long long c0(0); c0 < dim; ++c0)
					data[c0] /= a;
			}
			return *this;
		}
		//vecA = a * vecB + vecA
		vec& fmadd(double a, vec const& b)
		{
			if (b.dim && dim)
			{
				unsigned long long minDim(dim > b.dim ? b.dim : dim);
				/*for (unsigned long long c0(0); c0 < minDim; ++c0)
					data[c0] += a * b.data[c0];*/
				unsigned long long minDim4(minDim >> 2);
				__m256d* aData((__m256d*)data);
				__m256d* bData((__m256d*)b.data);
				unsigned long long c0(0);
				__m256d tp = _mm256_broadcast_sd(&a);
				for (; c0 < minDim4; ++c0)
					aData[c0] = _mm256_fmadd_pd(tp, bData[c0], aData[c0]);
				if ((c0 << 2) < minDim)
					for (unsigned long long c1(c0 << 2); c1 < minDim; ++c1)
						data[c1] += a * b.data[c1];
			}
			return *this;
		}
		//+-*/
		vec operator+(vec const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long l(dim > a.dim ? a.dim : dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] + a.data[c0];
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator-(vec const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long l(dim > a.dim ? a.dim : dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] - a.data[c0];
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator*(vec const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long l(dim > a.dim ? a.dim : dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] * a.data[c0];
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator/(vec const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long l(dim > a.dim ? a.dim : dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] / a.data[c0];
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator+(double a)const
		{
			if (dim)
			{
				unsigned long long l(dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] + a;
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator-(double a)const
		{
			if (dim)
			{
				unsigned long long l(dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] - a;
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator*(double a)const
		{
			if (dim)
			{
				unsigned long long l(dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] * a;
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		vec operator/(double a)const
		{
			if (dim)
			{
				unsigned long long l(dim);
				double* d(malloc256d(l));
				for (unsigned long long c0(0); c0 < l; ++c0)
					d[c0] = data[c0] / a;
				return vec(d, l, Type::Native);
			}
			else return vec();
		}
		//dot
		double operator,(vec const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long e0(dim + begining);
				unsigned long long e1(a.dim + a.begining);
				unsigned long long minE(e0 >= e1 ? e1 : e0);
				double s(0);
				/*for (unsigned long long c0(0); c0 < minDim; ++c0)
					s += data[c0] * a.data[c0];*/
				unsigned long long maxB(begining >= a.begining ? begining : a.begining);
				unsigned long long minDim4(minE >> 2);
				__m256d* aData((__m256d*)data);
				__m256d* bData((__m256d*)a.data);
				unsigned long long c0(0);
				__m256d tp = { 0 };
				if (begining)
				{
					tp = _mm256_fmadd_pd(aData[c0], bData[c0], tp);
					for (unsigned long long c1(0); c1 < maxB; ++c1)
						tp.m256d_f64[c1] = 0;
					++c0;
				}
				for (unsigned long long c1(minE); c1 < 4; ++c1)
					tp.m256d_f64[c1] = 0;
				for (; c0 < minDim4; ++c0)
					tp = _mm256_fmadd_pd(aData[c0], bData[c0], tp);
				for (unsigned long long c1(c0 << 2); c1 < minE; ++c1)
					s += data[c1] * a.data[c1];
				s += tp.m256d_f64[0];
				s += tp.m256d_f64[1];
				s += tp.m256d_f64[2];
				s += tp.m256d_f64[3];
				return s;
			}
			else return 0;
		}
		//normalize
		vec& normalize()
		{
			double s(this->norm2());
			if (s)(*this) *= 1 / s;
			return *this;
		}
		//non-in-situ mult mat
		vec operator()(mat const& a)const;
		vec& operator()(mat const& a, vec& b)const;

		//norm
		double norm1()const
		{
			if (dim)
			{
				double s(0);
				/*for (unsigned long long c0(0); c0 < dim; ++c0)
					s += abs(data[c0]);*/
				unsigned long long a((1llu << 63) - 1llu);
				__m256d gg = _mm256_broadcast_sd((double*)&a);
				unsigned long long finalDim(dim + begining);
				unsigned long long dim4(finalDim >> 2);
				__m256d* aData((__m256d*)data);
				unsigned long long c0(0);
				__m256d tp = { 0 };
				if (begining)
				{
					tp = _mm256_add_pd(tp, _mm256_and_pd(gg, aData[c0++]));
					for (unsigned long long c1(0); c1 < begining; ++c1)
						tp.m256d_f64[c1] = 0;
				}
				for (unsigned long long c1(finalDim); c1 < 4; ++c1)
					tp.m256d_f64[c1] = 0;
				for (; c0 < dim4; ++c0)
					tp = _mm256_add_pd(tp, _mm256_and_pd(gg, aData[c0]));
				for (unsigned long long c1(0); c1 < 4; ++c1)
					s += tp.m256d_f64[c1];
				for (unsigned long long c1(c0 << 2); c1 < finalDim; ++c1)
					s += abs(data[c1]);
				return s;
			}
			return 0;
		}
		double norm2Square()const
		{
			if (dim)
			{
				double s(0);
				unsigned long long finalDim(dim + begining);
				unsigned long long dim4(finalDim >> 2);
				__m256d* aData((__m256d*)data);
				unsigned long long c0(0);
				__m256d tp = { 0 };
				if (begining)
				{
					__m256d gg = aData[c0++];
					tp = _mm256_fmadd_pd(gg, gg, tp);
					for (unsigned long long c1(0); c1 < begining; ++c1)
						tp.m256d_f64[c1] = 0;
				}
				for (unsigned long long c1(finalDim); c1 < 4; ++c1)
					tp.m256d_f64[c1] = 0;
				for (; c0 < dim4; ++c0)
				{
					__m256d gg = aData[c0];
					tp = _mm256_fmadd_pd(gg, gg, tp);
				}
				for (unsigned long long c1(c0 << 2); c1 < finalDim; ++c1)
					s += data[c1] * data[c1];
				s += tp.m256d_f64[0];
				s += tp.m256d_f64[1];
				s += tp.m256d_f64[2];
				s += tp.m256d_f64[3];
				return s;
			}
			else return 0;
		}
		double norm2()const
		{
			return sqrt(norm2Square());
		}
		double normInf()const
		{
			if (dim)
			{
				double s(0);
				/*for (unsigned long long c0(0); c0 < dim; ++c0)
					if (s < abs(data[c0]))s = abs(data[c0]);*/
				unsigned long long a((1llu << 63) - 1llu);
				double g(*(double*)&a);
				__m256d gg = _mm256_broadcast_sd((double*)&a);
				unsigned long long finalDim(dim + begining);
				unsigned long long dim4(finalDim >> 2);
				__m256d* aData((__m256d*)data);
				unsigned long long c0(0);
				__m256d tp = { 0 };
				if (begining)
				{
					tp = _mm256_and_pd(gg, aData[c0++]);
					for (unsigned long long c1(0); c1 < begining; ++c1)
						tp.m256d_f64[c1] = 0;
				}
				for (unsigned long long c1(finalDim); c1 < 4; ++c1)
					tp.m256d_f64[c1] = 0;
				for (; c0 < dim4; ++c0)
					tp = _mm256_max_pd(tp, _mm256_and_pd(gg, aData[c0]));
				for (unsigned long long c1(0); c1 < 4; ++c1)
					if (s < tp.m256d_f64[c1])s = tp.m256d_f64[c1];
				for (unsigned long long c1(c0 << 2); c1 < finalDim; ++c1)
					if (s < abs(data[c1]))s = abs(data[c1]);
				return s;
			}
			return 0;
		}
		double normP(double p)const
		{
			if (dim && p)
			{
				double s(0);
				/*for (unsigned long long c0(0); c0 < dim; ++c0)
					s += pow(abs(data[c0]), p);*/
				unsigned long long a((1llu << 63) - 1llu);
				double g(*(double*)&a);
				__m256d gg = _mm256_broadcast_sd((double*)&a);
				__m256d pp = _mm256_broadcast_sd(&p);
				unsigned long long finalDim(dim + begining);
				unsigned long long dim4(finalDim >> 2);
				__m256d* aData((__m256d*)data);
				unsigned long long c0(0);
				__m256d tp = { 0 };
				if (begining)
				{
					tp = _mm256_pow_pd(_mm256_and_pd(gg, aData[c0++]), pp);
					for (unsigned long long c1(0); c1 < begining; ++c1)
						tp.m256d_f64[c1] = 0;
				}
				for (unsigned long long c1(finalDim); c1 < 4; ++c1)
					tp.m256d_f64[c1] = 0;
				for (; c0 < dim4; ++c0)
					tp = _mm256_add_pd(
						_mm256_pow_pd(_mm256_and_pd(gg, aData[c0]), pp), tp);
				for (unsigned long long c1(0); c1 < 4; ++c1)
					s += tp.m256d_f64[c1];
				for (unsigned long long c1(c0 << 2); c1 < finalDim; ++c1)
					s += pow(abs(data[c1]), p);
				return pow(s, 1 / p);
			}
			return 0;
		}

		void print()const
		{
			::printf("[");
			unsigned long long finalDim(dim + begining);
			if (dim)
			{
				for (unsigned long long c0(begining); c0 < finalDim - 1; ++c0)
					::printf("%.4f, ", data[c0]);
				::printf("%.4f", data[finalDim - 1]);
			}
			::printf("]\n");
		}
		void printInfo()const
		{
			char const* str;
			switch (type)
			{
			case Type::Native:str = "Native"; break;
			case Type::Parasitic:str = "Parastic"; break;
			case Type::Non32Aligened:str = "Non32Aligened"; break;
			}
			::printf("{dim: %u, begining: %u, type: %s}\n", dim, begining, str);
		}
		void printToTxt(char const* name)const
		{
			//in the form of Mathematica matrix (paste)
			if (data)
			{
				FILE* temp(::fopen(name, "w+"));
				::fprintf(temp, "{");
				unsigned long long finalDim(dim + begining);
				for (unsigned long long c0(begining); c0 < finalDim - 1; ++c0)
					::fprintf(temp, "{%.14e}, ", data[c0]);
				::fprintf(temp, "{%.14e}}\n", data[finalDim - 1]);
				::fclose(temp);
			}
		}
		void printToTableTxt(char const* name, bool _inRow)const
		{
			//in the form of Mathematica matrix (table)
			if (data)
			{
				FILE* temp(::fopen(name, "w+"));
				for (unsigned long long c0(begining); c0 < dim + begining; ++c0)
					::fprintf(temp, _inRow ? "%.14e " : "%.14e\n", data[c0]);
				::fclose(temp);
			}
		}
	};
	struct mat
	{
		double* data;
		union
		{
			unsigned long long width;
			unsigned long long halfBandWidth;//if is BandMat
			unsigned long long* rowIndice;//if is SparseMat
		};
		union
		{
			unsigned long long height;
			unsigned long long* colIndice;//if is SparseMat
		};
		union
		{
			unsigned long long width4d;
			unsigned long long elementNum;
		};
		Type type;
		MatType matType;

		mat() :data(nullptr), width(0), height(0), width4d(ceiling4(width)),
			type(Type::Native), matType(MatType::NormalMat) {}
		mat(unsigned long long _width, unsigned long long _height, bool _clear = true)
			:
			data((_width&& _height) ? malloc256d(_width, _height) : nullptr),
			width(data ? _width : 0),
			height(data ? _height : 0),
			width4d(data ? ceiling4(_width) : 0),
			type(Type::Native),
			matType(MatType::NormalMat)
		{
			if (_clear && data)
				memset256d(data, 0, width, height);
		}
		mat(unsigned long long _halfBandWidth, unsigned long long _height, MatType _type, bool _clear = true)
			:
			data(nullptr),
			halfBandWidth(0),
			height(0),
			width4d(0),
			type(Type::Native),
			matType(_type)
		{
			if (_type == MatType::NormalMat && width && height)
			{
				data = malloc256d(_halfBandWidth, _height);
				width = _halfBandWidth;
				height = _height;
				width4d = ceiling4(_halfBandWidth);
			}
			else if (_type == MatType::BandMat && _height)
			{
				data = malloc256d(2 * _halfBandWidth + 4, _height);
				if (_halfBandWidth > _height - 1)halfBandWidth = _height - 1;
				else halfBandWidth = _halfBandWidth;
				height = _height;
				width4d = ceiling4(2 * _halfBandWidth + 4);
			}
			else if ((_type == MatType::LBandMat || _type == MatType::UBandMat) && _height)
			{
				data = malloc256d(_halfBandWidth + 4, _height);
				if (_halfBandWidth > _height - 1)halfBandWidth = _height - 1;
				else halfBandWidth = _halfBandWidth;
				height = _height;
				width4d = ceiling4(_halfBandWidth + 4);
			}
			if (_clear && data)
				memset64d(data, 0, width4d * height);
		}
		mat(MatType _type, unsigned long long _elementNum)
			:
			data(nullptr),
			rowIndice(nullptr),
			colIndice(nullptr),
			elementNum(_elementNum),
			type(Type::Native),
			matType(_type)
		{
			if (_type == MatType::SparseMat && _elementNum)
			{
				data = malloc64d(_elementNum);
				rowIndice = (unsigned long long*)malloc64d(_elementNum);
				colIndice = (unsigned long long*)malloc64d(_elementNum);
			}
		}
		mat(mat const& a)
			:
			data((a.width&& a.height) ? malloc256d(a.width, a.height) : nullptr),
			width(a.width),
			height(a.height),
			width4d(a.width4d),
			type(Type::Native),
			matType(a.matType)
		{
			if (data)
				memcpy256d(data, a.data, width, height);
		}
		mat(mat&& a) :data(nullptr), width(0), height(0), type(Type::Native), matType(MatType::NormalMat)
		{
			if (a.data)
			{
				if (a.type == Type::Native)
				{
					data = a.data;
					width = a.width;
					height = a.height;
					matType = a.matType;
					a.data = nullptr;
					a.width = a.height = a.width4d = 0;
				}
				else
				{
					data = malloc256d(a.width, a.height);
					width = a.width;
					height = a.height;
					memcpy256d(data, a.data, a.width, a.height);
				}
				width4d = ceiling4(width);
			}
		}
		mat(double* _data, unsigned long long _width, unsigned long long _height, Type _type, MatType _matType)
			:
			data(_data), width(_width), height(_height), width4d(ceiling4(_width)),
			type(_type), matType(_matType)
		{
		}
		mat(std::initializer_list<std::initializer_list<double>>const& a)
			:
			data(nullptr), width(0), height(0), type(Type::Native), matType(MatType::NormalMat)
		{
			if (a.size())
			{
				unsigned long long h(a.size());
				unsigned long long w(0);
				for (unsigned long long c0(0); c0 < h; ++c0)
					if ((a.begin() + c0)->size() > w)
						w = (a.begin() + c0)->size();
				if (w)
				{
					unsigned long long l(ceiling4(w, h));
					data = malloc64d(l);
					memset64d(data, 0, l);
					width = w;
					height = h;
					width4d = ceiling4(width);
					type = Type::Native;
					matType = MatType::NormalMat;
					for (unsigned long long c0(0); c0 < height; ++c0)
						memcpy64d(data + width4d * c0, (a.begin() + c0)->begin(),
							(a.begin() + c0)->size());
				}
			}
		}
		~mat()
		{
			if (type == Type::Native)_mm_free(data);
			data = nullptr;
			if (matType < MatType::SparseMat)width = height = 0;
			else
			{
				_mm_free(rowIndice);
				_mm_free(colIndice);
				rowIndice = colIndice = nullptr;
			}
		}
		template<class T>inline double& operator[](T a)
		{
			return data[a];
		}
		inline double& operator() (unsigned long long a, unsigned long long b)
		{
			return data[unsigned long long(a) * width4d + b];
		}
		inline double  BandEle(unsigned long long a, unsigned long long b)const
		{
			if (a <= halfBandWidth)return data[a * width4d + b];
			else return data[a * width4d + b - (long long(a - halfBandWidth) / 4) * 4];
		}
		inline double& BandEleRef(unsigned long long a, unsigned long long b)
		{
			if (a <= halfBandWidth)return data[a * width4d + b];
			else return data[a * width4d + b - (long long(a - halfBandWidth) / 4) * 4];
		}
		inline double  LBandEle(unsigned long long a, unsigned long long b)const
		{
			if (a <= halfBandWidth)return data[a * width4d + b];
			else return data[a * width4d + b - (long long(a - halfBandWidth) / 4) * 4];
		}
		inline double& LBandEleRef(unsigned long long a, unsigned long long b)
		{
			if (a <= halfBandWidth)return data[a * width4d + b];
			else return data[a * width4d + b - (long long(a - halfBandWidth) / 4) * 4];
		}
		inline double  UBandEle(unsigned long long a, unsigned long long b)const
		{
			return data[a * width4d + b - (a & -4)];
		}
		inline double& UBandEleRef(unsigned long long a, unsigned long long b)
		{
			return data[a * width4d + b - (a & -4)];
		}
		inline unsigned long long BandBeginOffset(unsigned long long a)const
		{
			if (a <= halfBandWidth)return 0;
			else return a - (long long(a - halfBandWidth) / 4) * 4 - halfBandWidth;
		}
		inline unsigned long long LBandBeginOffset(unsigned long long a)const
		{
			if (a <= halfBandWidth)return 0;
			else return a - (long long(a - halfBandWidth) / 4) * 4 - halfBandWidth;
		}
		inline unsigned long long UBandBeginOffset(unsigned long long a)const
		{
			return a % 4;
		}
		inline vec getBandRow(unsigned long long a)
		{
			unsigned long long bgn, end;
			if (a <= halfBandWidth)bgn = 0;
			else bgn = a - halfBandWidth;
			if (a >= height - halfBandWidth)end = height;
			else end = a + halfBandWidth + 1;
			return vec(data + a * width4d + LBandBeginOffset(a), end - bgn, Type::Non32Aligened);
		}
		inline vec getLBandRow(unsigned long long a)
		{
			return vec(data + a * width4d + LBandBeginOffset(a),
				a <= halfBandWidth ? a + 1 : halfBandWidth + 1, Type::Non32Aligened);
		}
		inline vec getUBandRow(unsigned long long a)
		{
			return vec(data + a * width4d + a % 4,
				height - a <= halfBandWidth ? height - a : halfBandWidth + 1, Type::Non32Aligened);
		}
		inline vec const getBandRow(unsigned long long a)const
		{
			unsigned long long bgn, end;
			if (a <= halfBandWidth)bgn = 0;
			else bgn = a - halfBandWidth;
			if (a >= height - halfBandWidth)end = height;
			else end = a + halfBandWidth + 1;
			return vec(data + a * width4d + LBandBeginOffset(a), end - bgn, Type::Non32Aligened);
		}
		inline vec const getLBandRow(unsigned long long a)const
		{
			return vec(data + a * width4d + LBandBeginOffset(a),
				a <= halfBandWidth ? a + 1 : halfBandWidth + 1, Type::Non32Aligened);
		}
		inline vec const getUBandRow(unsigned long long a)const
		{
			return vec(data + a * width4d + a % 4,
				height - a <= halfBandWidth ? height - a : halfBandWidth + 1, Type::Non32Aligened);
		}
		inline vec const getLBandRowL(unsigned long long a)const
		{
			if (a)
				return vec(data + a * width4d + LBandBeginOffset(a),
					a <= halfBandWidth ? a : halfBandWidth, Type::Non32Aligened);
			return vec();
		}
		inline vec const getUBandRowU(unsigned long long a)const
		{
			if (height - a)
				return vec(data + a * width4d + a % 4 + 1,
					height - a <= halfBandWidth ? height - a - 1 : halfBandWidth, Type::Non32Aligened);
			return vec();
		}
		void clear()
		{
			if (data)memset64d(data, 0, height * width4d);
		}
		void reconstruct(unsigned long long _width, unsigned long long _height, bool _clear = true)
		{
			if (type == Type::Native)
			{
				if (data)_mm_free(data);
				unsigned long long s(ceiling256dSize(_width, _height));
				if (s)
				{
					data = (double*)malloc64d(s);
					if (_clear)memset64d(data, 0, s);
					width = _width;
					height = _height;
				}
			}
		}
		void extend(unsigned int _elementNum)
		{
			if (_elementNum > elementNum && matType == MatType::SparseMat)
			{
				double* _data(malloc64d(_elementNum));
				unsigned long long* _r((unsigned long long*)malloc64d(_elementNum));
				unsigned long long* _c((unsigned long long*)malloc64d(_elementNum));
				memcpy64d(_data, data, elementNum);
				memcpy64d(_r, rowIndice, elementNum);
				memcpy64d(_c, colIndice, elementNum);
				_mm_free(data);
				_mm_free(rowIndice);
				_mm_free(colIndice);
				elementNum = _elementNum;
			}
		}
		void addSparse(unsigned long long a, unsigned long long b, double s, unsigned long long& cnt)
		{
			data[cnt] = s;
			rowIndice[cnt] = a;
			colIndice[cnt++] = b;
		}
		//= += -= *= /=
		mat& operator =(mat&& a)
		{
			if (type == Type::Native)
			{
				if (a.type == Type::Native)
				{
					_mm_free(data);
					data = a.data;
					width = a.width;
					height = a.height;
					width4d = a.width4d;
					matType = a.matType;
					a.data = nullptr;
					a.width = a.height = a.width4d = 0;
				}
				else
				{
					if (unsigned long long(a.width4d) * a.height !=
						unsigned long long(width4d) * height)
					{
						_mm_free(data);
						data = malloc256d(a.width, a.height);
					}
					width = a.width;
					height = a.height;
					width4d = a.width4d;
					memcpy256d(data, a.data, width, height);
				}
			}
			else
			{
				unsigned long long minWidth(width > a.width ? a.width : width);
				unsigned long long minHeight(height > a.height ? a.height : height);
				for (unsigned long long c0(0); c0 < minHeight; ++c0)
					memcpy64d(data + width4d * c0, a.data + a.width4d * c0, minWidth);
			}
			return *this;
		}
		mat& operator =(mat const& a)
		{
			if (type == Type::Native)
			{
				if (unsigned long long(a.width4d) * a.height !=
					unsigned long long(width4d) * height)
				{
					_mm_free(data);
					data = malloc256d(a.width, a.height);
				}
				width = a.width;
				height = a.height;
				width4d = a.width4d;
				memcpy256d(data, a.data, width, height);
			}
			else
			{
				unsigned long long minWidth(width > a.width ? a.width : width);
				unsigned long long minHeight(height > a.height ? a.height : height);
				for (unsigned long long c0(0); c0 < minHeight; ++c0)
					memcpy64d(data + width4d * c0, a.data + a.width4d * c0, minWidth);
			}
			return *this;
		}
		mat& operator+=(mat const& a)
		{
			if (data && a.data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						data[c0 * width4d + c1] += a.data[c0 * a.width4d + c1];
			}
			return *this;
		}
		mat& operator-=(mat const& a)
		{
			if (data && a.data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						data[c0 * width4d + c1] -= a.data[c0 * a.width4d + c1];
			}
			return *this;
		}
		mat& operator*=(mat const& a)
		{
			if (data && a.data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						data[c0 * width4d + c1] *= a.data[c0 * a.width4d + c1];
			}
			return *this;
		}
		mat& operator/=(mat const& a)
		{
			if (data && a.data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						data[c0 * width4d + c1] /= a.data[c0 * a.width4d + c1];
			}
			return *this;
		}
		mat& operator =(double a)
		{
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						data[c0 * width4d + c1] = a;
			}
			return *this;
		}
		mat& operator+=(double a)
		{
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						data[c0 * width4d + c1] += a;
			}
			return *this;
		}
		mat& operator-=(double a)
		{
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						data[c0 * width4d + c1] -= a;
			}
			return *this;
		}
		mat& operator*=(double a)
		{
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						data[c0 * width4d + c1] *= a;
			}
			return *this;
		}
		mat& operator/=(double a)
		{
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						data[c0 * width4d + c1] /= a;
			}
			return *this;
		}
		//+-*/ to do: mat types' operations?
		mat operator+(mat const& a)const
		{
			if (a.data && data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				unsigned long long minW4d(ceiling4(minW));
				double* d(malloc256d(minW, minH));
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						d[c0 * minW4d + c1] = data[c0 * width4d + c1] + a.data[c0 * a.width4d + c1];
				return mat(d, minW, minH, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator-(mat const& a)const
		{
			if (a.data && data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				unsigned long long minW4d(ceiling4(minW));
				double* d(malloc256d(minW, minH));
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						d[c0 * minW4d + c1] = data[c0 * width4d + c1] - a.data[c0 * a.width4d + c1];
				return mat(d, minW, minH, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator*(mat const& a)const
		{
			if (a.data && data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				unsigned long long minW4d(ceiling4(minW));
				double* d(malloc256d(minW, minH));
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						d[c0 * minW4d + c1] = data[c0 * width4d + c1] * a.data[c0 * a.width4d + c1];
				return mat(d, minW, minH, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator/(mat const& a)const
		{
			if (a.data && data)
			{
				unsigned long long minW(a.width > width ? width : a.width);
				unsigned long long minH(a.height > height ? height : a.height);
				unsigned long long minW4d(ceiling4(minW));
				double* d(malloc256d(minW, minH));
				for (unsigned long long c0(0); c0 < minH; ++c0)
					for (unsigned long long c1(0); c1 < minW; ++c1)
						d[c0 * minW4d + c1] = data[c0 * width4d + c1] / a.data[c0 * a.width4d + c1];
				return mat(d, minW, minH, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator+(double a)const
		{
			if (data)
			{
				double* d(malloc256d(width, height));
				memcpy256d(d, data, width, height);
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						d[c0 * width4d + c1] += a;
				return mat(d, width, height, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator-(double a)const
		{
			if (data)
			{
				double* d(malloc256d(width, height));
				memcpy256d(d, data, width, height);
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						d[c0 * width4d + c1] -= a;
				return mat(d, width, height, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator*(double a)const
		{
			if (data)
			{
				double* d(malloc256d(width, height));
				memcpy256d(d, data, width, height);
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						d[c0 * width4d + c1] *= a;
				return mat(d, width, height, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		mat operator/(double a)const
		{
			if (data)
			{
				double* d(malloc256d(width, height));
				memcpy256d(d, data, width, height);
				for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < width; ++c1)
						d[c0 * width4d + c1] /= a;
				return mat(d, width, height, Type::Native, MatType::NormalMat);
			}
			return mat();
		}
		//non-in-situ mult vec
		vec operator()(vec const& a)const
		{
			unsigned long long h;
			unsigned long long minDim;
			if (matType < MatType::BandMat)
			{
				h = height;
				minDim = width > a.dim ? a.dim : width;
			}
			else if (matType < MatType::SparseMat)
			{
				h = height;
				minDim = height > a.dim ? a.dim : height;
			}
			else
			{
				h = minDim = a.dim;
			}
			if (minDim && h)
			{
				vec r(h, false);
				return (*this)(a, r);
			}
			return vec();
		}
		vec& operator()(vec const& a, vec& b)const
		{
			unsigned long long w(matType < MatType::BandMat ? width : height);
			if (matType == MatType::SparseMat)w = a.dim;
			unsigned long long minDim(w > a.dim ? a.dim : w);
			if (minDim)
			{
				bool overflow(ceiling4(minDim) > ceiling4(b.dim));
				if (overflow && b.type != Type::Native)return b;
				vec const* source(&a);
				vec r;
				if (&b == source)
				{
					source = &r;
					r = a;
				}
				if (overflow)b.reconstruct(minDim, false);
				switch (matType)
				{
				case MatType::NormalMat:
				case MatType::SquareMat:
				case MatType::DiagonalMat:
				case MatType::SymmetricMat:
				case MatType::LMat:
				case MatType::UMat:
				{
					constexpr unsigned long long warp = 8;
					unsigned long long minWidth4((minDim - 1) / 4 + 1);
					__m256d* aData((__m256d*)data);
					__m256d* bData((__m256d*)source->data);
					__m256d* rData((__m256d*)b.data);
					unsigned long long heightFloor4((height >> 2) << 2);
					unsigned long long widthWarp((minWidth4 / warp) * warp);
					unsigned long long warpLeftFloor((minDim >> 2) - widthWarp);
					unsigned long long warpLeftCeiling(minWidth4 - widthWarp);
					unsigned long long c0(0);
					for (; c0 < heightFloor4; c0 += 4)
					{
						__m256d ans[4] = { 0 };
						__m256d tp[warp];
						unsigned long long c1(0);
						for (; c1 < widthWarp; c1 += warp)
						{
							__m256d* s(aData + minWidth4 * c0 + c1);
#pragma unroll(4)
							for (unsigned long long c2(0); c2 < warp; ++c2)
								tp[c2] = bData[c1 + c2];
							for (unsigned long long c2(0); c2 < 4; ++c2, s += minWidth4)
							{
#pragma unroll(4)
								for (unsigned long long c3(0); c3 < warp; ++c3)
								{
									__m256d t = s[c3];
									ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
								}
							}
						}
						if (c1 < minWidth4)
						{
							__m256d* s(aData + minWidth4 * c0 + c1);
#pragma unroll(4)
							for (unsigned long long c2(0); c2 < warpLeftCeiling; ++c2)
								tp[c2] = bData[c1 + c2];
							unsigned long long finalWidth(minDim - ((minDim >> 2) << 2));
							for (unsigned long long c2(finalWidth); c2 < 4; ++c2)
								tp[warpLeftFloor].m256d_f64[c2] = 0;
							for (unsigned long long c2(0); c2 < 4; ++c2, s += minWidth4)
							{
#pragma unroll(4)
								for (unsigned long long c3(0); c3 < warpLeftCeiling; ++c3)
								{
									__m256d t = s[c3];
									ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
								}
							}
						}
						__m256d s;
						for (unsigned long long c1(0); c1 < 4; ++c1)
						{
							s.m256d_f64[c1] = ans[c1].m256d_f64[0];
							s.m256d_f64[c1] += ans[c1].m256d_f64[1];
							s.m256d_f64[c1] += ans[c1].m256d_f64[2];
							s.m256d_f64[c1] += ans[c1].m256d_f64[3];
						}
						rData[c0 >> 2] = s;
					}
					if (c0 < height)
					{
						unsigned long long heightLeft(height - heightFloor4);
						__m256d ans[4] = { 0 };
						__m256d tp[warp];
						unsigned long long c1(0);
						for (; c1 < widthWarp; c1 += warp)
						{
							__m256d* s(aData + minWidth4 * c0 + c1);
#pragma unroll(4)
							for (unsigned long long c2(0); c2 < warp; ++c2)
								tp[c2] = bData[c1 + c2];
							for (unsigned long long c2(0); c2 < heightLeft; ++c2, s += minWidth4)
							{
#pragma unroll(4)
								for (unsigned long long c3(0); c3 < warp; ++c3)
								{
									__m256d t = s[c3];
									ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
								}
							}
						}
						if (c1 < minWidth4)
						{
							__m256d* s(aData + minWidth4 * c0 + c1);
#pragma unroll(4)
							for (unsigned long long c2(0); c2 < warpLeftCeiling; ++c2)
								tp[c2] = bData[c1 + c2];
							unsigned long long finalWidth(minDim - ((minDim >> 2) << 2));
							for (unsigned long long c2(finalWidth); c2 < 4; ++c2)
								tp[warpLeftFloor].m256d_f64[c2] = 0;
							for (unsigned long long c2(0); c2 < heightLeft; ++c2, s += minWidth4)
							{
#pragma unroll(4)
								for (unsigned long long c3(0); c3 < warpLeftCeiling; ++c3)
								{
									__m256d t = s[c3];
									ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
								}
							}
						}
						__m256d s;
						for (unsigned long long c1(0); c1 < heightLeft; ++c1)
						{
							s.m256d_f64[c1] = ans[c1].m256d_f64[0];
							s.m256d_f64[c1] += ans[c1].m256d_f64[1];
							s.m256d_f64[c1] += ans[c1].m256d_f64[2];
							s.m256d_f64[c1] += ans[c1].m256d_f64[3];
						}
						rData[c0 >> 2] = s;
					}
					break;
				}
				case MatType::BandMat:
				{
					for (unsigned long long c0(0); c0 < height; ++c0)
					{
						vec tp(getBandRow(c0));
						unsigned long long bgn(c0 <= halfBandWidth ? 0 : c0 - halfBandWidth);
						vec ta(a.data + bgn, tp.dim, Type::Non32Aligened);
						b.data[c0] = (tp, ta);
					}
					break;
				}
				case MatType::LBandMat:
				{
					for (unsigned long long c0(0); c0 < height; ++c0)
					{
						vec tp(getLBandRow(c0));
						unsigned long long bgn(c0 <= halfBandWidth ? 0 : c0 - halfBandWidth);
						vec ta(a.data + bgn, tp.dim, Type::Non32Aligened);
						b.data[c0] = (tp, ta);
					}
					break;
				}
				case MatType::UBandMat:
				{
					for (unsigned long long c0(0); c0 < height; ++c0)
					{
						vec tp(getUBandRow(c0));
						vec ta(a.data + c0, tp.dim, Type::Non32Aligened);
						b.data[c0] = (tp, ta);
					}
					break;
				}
				case MatType::SparseMat:
				{
					unsigned long long n(0);
					for (unsigned long long c0(0); c0 < minDim && n < elementNum; ++c0)
					{
						b.data[c0] = 0;
						if (rowIndice[n] > c0)continue;
						else
						{
							while (n < elementNum)
							{
								if (rowIndice[n] > c0)break;
								b.data[c0] += data[n] * a.data[colIndice[n]];
								n++;
							}
						}
						/*__m256d ta, tb;
						__m256d ts = { 0 };
						double s(0);
						if (rowIndice[n] == c0)
						{
							while (1)
							{
								unsigned long long c1(0);
								bool flag(false);
								for (; c1 < 4 && n + c1 < elementNum; ++c1)
								{
									if (rowIndice[n + c1] > c0)break;
									ta.m256d_f64[c1] = data[n + c1];
									tb.m256d_f64[c1] = a.data[colIndice[n + c1]];
								}
								if (c1 < 4)
								{
									for (unsigned long long c2(c1); c2 < 4; ++c2)
										ta.m256d_f64[c2] = 0;
									flag = true;
								}
								ts = _mm256_fmadd_pd(ta, tb, ts);
								n += c1;
								if (flag)
								{
									for (unsigned long long c2(0); c2 < 4; ++c2)
										s += ts.m256d_f64[c2];
									break;
								}
							}
						}
						b.data[c0] = s;*/
					}
				}
				}
				return b;
			}
		}
		//non-in-situ mult mat
		mat operator()(mat const& a)const
		{
			unsigned long long minDim(width > a.height ? a.height : width);
			if (minDim)
			{
				mat r(a.width, height, false);
				/*for (unsigned long long c0(0); c0 < height; ++c0)
					for (unsigned long long c1(0); c1 < minDim; ++c1)
						for (unsigned long long c2(0); c2 < a.width; ++c2)
							r.data[c0 * a.width + c2] += data[c0 * width4d + c1] * a.data[c1 * a.width + c2];*/
				return (*this)(a, r);
			}
			return mat();
		}
		mat& operator()(mat const& a, mat& b)const
		{
			unsigned long long minDim(width > a.height ? a.height : width);
			if (minDim)
			{
				bool overflow(ceiling4(a.width, height) > b.width4d * b.height);
				if (overflow && b.type != Type::Native)return b;
				mat const* source(this);
				mat r;
				if (&b == this)
				{
					source = &r;
					r = *this;
				}
				if (overflow)b.reconstruct(a.width, height, false);
				__m256d* aData((__m256d*)a.data);
				__m256d* bData((__m256d*)b.data);
				constexpr unsigned long long warp = 16;
				unsigned long long aWidth256d(a.width4d / 4);
				unsigned long long aWidthWarpFloor(aWidth256d / warp * warp);
				unsigned long long warpLeft(aWidth256d - aWidthWarpFloor);
				unsigned long long height2Floor(height & (-2));
				unsigned long long c0(0);
				for (; c0 < height2Floor; c0 += 2)
				{
					unsigned long long c1(0);
					for (; c1 < aWidthWarpFloor; c1 += warp)
					{
						__m256d ans0[warp] = { 0 };
						__m256d ans1[warp] = { 0 };
						for (unsigned long long c2(0); c2 < minDim; ++c2)
						{
							//__m256d t = _mm256_i32gather_pd(tempData, offset, 8);
							__m256d tp0 = _mm256_broadcast_sd(source->data + c0 * width4d + c2);
							__m256d tp1 = _mm256_broadcast_sd(source->data + (c0 + 1) * width4d + c2);
#pragma unroll(4)
							for (unsigned long long c3(0); c3 < warp; ++c3)
							{
								__m256d b = aData[aWidth256d * c2 + c1 + c3];
								ans0[c3] = _mm256_fmadd_pd(tp0, b, ans0[c3]);
								ans1[c3] = _mm256_fmadd_pd(tp1, b, ans1[c3]);
							}
						}
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warp; ++c3)
						{
							bData[c0 * aWidth256d + c1 + c3] = ans0[c3];
							bData[(c0 + 1) * aWidth256d + c1 + c3] = ans1[c3];
						}
					}
					if (c1 < aWidth256d)
					{
						__m256d ans0[warp] = { 0 };
						__m256d ans1[warp] = { 0 };
						for (unsigned long long c2(0); c2 < minDim; ++c2)
						{
							__m256d tp0 = _mm256_broadcast_sd(source->data + c0 * width4d + c2);
							__m256d tp1 = _mm256_broadcast_sd(source->data + (c0 + 1) * width4d + c2);
							for (unsigned long long c3(0); c3 < warpLeft; ++c3)
							{
								__m256d b = aData[aWidth256d * c2 + c1 + c3];
								ans0[c3] = _mm256_fmadd_pd(tp0, b, ans0[c3]);
								ans1[c3] = _mm256_fmadd_pd(tp1, b, ans1[c3]);
							}
						}
						for (unsigned long long c3(0); c3 < warpLeft; ++c3)
						{
							bData[c0 * aWidth256d + c1 + c3] = ans0[c3];
							bData[(c0 + 1) * aWidth256d + c1 + c3] = ans1[c3];
						}
					}
				}
				if (c0 < height)
				{
					unsigned long long c1(0);
					for (; c1 < aWidthWarpFloor; c1 += warp)
					{
						__m256d ans0[warp] = { 0 };
						for (unsigned long long c2(0); c2 < minDim; ++c2)
						{
							__m256d tp0 = _mm256_broadcast_sd(source->data + c0 * width4d + c2);
#pragma unroll(4)
							for (unsigned long long c3(0); c3 < warp; ++c3)
							{
								__m256d b = aData[aWidth256d * c2 + c1 + c3];
								ans0[c3] = _mm256_fmadd_pd(tp0, b, ans0[c3]);
							}
						}
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warp; ++c3)
							bData[c0 * aWidth256d + c1 + c3] = ans0[c3];
					}
					if (c1 < aWidth256d)
					{
						__m256d ans0[warp] = { 0 };
						for (unsigned long long c2(0); c2 < minDim; ++c2)
						{
							__m256d tp0 = _mm256_broadcast_sd(source->data + c0 * width4d + c2);
							for (unsigned long long c3(0); c3 < warpLeft; ++c3)
							{
								__m256d b = aData[aWidth256d * c2 + c1 + c3];
								ans0[c3] = _mm256_fmadd_pd(tp0, b, ans0[c3]);
							}
						}
						for (unsigned long long c3(0); c3 < warpLeft; ++c3)
							bData[c0 * aWidth256d + c1 + c3] = ans0[c3];
					}
				}
			}
			return b;
		}
		//true blas
		mat& schmidtOrtho()
		{
			if (data && width == height)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
				{
					vec tp(data + width4d * c0, width, Type::Parasitic);
					for (unsigned long long c1(0); c1 < c0; ++c1)
					{
						vec ts(data + width4d * c1, width, Type::Parasitic);
						tp.fmadd(-(tp, ts), ts);
					}
					tp.normalize();
				}
			}
			return *this;
		}
		vec& solveL(vec const& a, vec& b)const
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			double ll(data[0]);
			if (ll == 0.0)return b;
			b.data[0] = a.data[0] / ll;
			unsigned long long c0(1);
			if (matType == MatType::LBandMat && a.dim >= height)
			{
				for (; c0 < height; ++c0)
				{
					ll = LBandEle(c0, c0);
					if (ll == 0.0)return b;
					vec tp(getLBandRowL(c0));
					unsigned long long bgn(c0 <= halfBandWidth ? 0 : c0 - halfBandWidth);
					vec tb(b.data + bgn, tp.dim, Type::Non32Aligened);
					b.data[c0] = (a.data[c0] - (tp, tb)) / ll;
				}
			}
			else
			{
				for (; c0 < minDim; ++c0)
				{
					ll = data[c0 * width4d + c0];
					if (ll == 0.0)return b;
					vec tp(data + c0 * width4d, c0, Type::Parasitic);
					b.data[c0] = (a.data[c0] - (tp, b)) / ll;
				}
			}
			return b;
		}
		vec& solveU(vec const& a, vec& b)const
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			double ll(data[(minDim - 1) * (width4d + 1)]);
			if (ll == 0.0)return b;
			long long c0(minDim - 1);
			b.data[c0] = a.data[c0] / ll;
			--c0;
			if (matType == MatType::UBandMat && a.dim >= height)
			{
				for (; c0 >= 0; --c0)
				{
					ll = UBandEle(c0, c0);
					if (ll == 0.0)return b;
					vec tp(getUBandRowU(c0));
					vec tb(b.data + c0 + 1, tp.dim, Type::Non32Aligened);
					b.data[c0] = (a.data[c0] - (tp, tb)) / ll;
				}
			}
			else
			{
				for (; c0 >= 0; --c0)
				{
					ll = data[c0 * width4d + c0];
					if (ll == 0.0)return b;
					unsigned long long c01(c0 + 1);
					vec tp(data + c0 * width4d + c01, minDim - c01, Type::Non32Aligened);
					vec tb(b.data + c01, minDim - c01, Type::Non32Aligened);
					b.data[c0] = (a.data[c0] - (tp, tb)) / ll;
				}
			}
			return b;
		}
		vec& solveUid(vec const& a, vec& b)const
		{
			//assuming that mat[i][i]==1
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			long long c0(minDim - 1);
			b.data[c0] = a.data[c0];
			--c0;
			if (matType == MatType::UBandMat && a.dim >= height)
			{
				for (; c0 >= 0; --c0)
				{
					vec tp(getUBandRowU(c0));
					vec tb(b.data + c0 + 1, tp.dim, Type::Non32Aligened);
					b.data[c0] = (a.data[c0] - (tp, tb));
				}
			}
			else
			{
				for (; c0 >= 0; --c0)
				{
					unsigned long long c01(c0 + 1);
					vec tp(data + c0 * width4d + c01, minDim - c01, Type::Non32Aligened);
					vec tb(b.data + c01, minDim - c01, Type::Non32Aligened);
					b.data[c0] = (a.data[c0] - (tp, tb));
				}
			}
			return b;
		}
		vec& solveGauss(vec& a, vec& b)
		{
			//no column principal
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			for (long long c0(minDim - 1); c0 > 0; --c0)
			{
				double ll(data[c0 * width4d + c0]);
				if (ll == 0.0)return b;
				ll = -1 / ll;
				double aa(a.data[c0]);
				vec tp(data + c0 * width4d, c0, Type::Parasitic);
				for (long long c1(c0 - 1); c1 >= 0; --c1)
				{
					double le(data[c1 * width4d + c0] * ll);
					if (le == 0.0)continue;
					vec ts(data + c1 * width4d, c0, Type::Parasitic);
					ts.fmadd(le, tp);
					a.data[c1] += le * aa;
					//data[c1 * width4d + c0] = 0;
				}
			}
			return solveL(a, b);
		}
		vec& solveJacobiIter(vec const& a, vec& b, double eps)const
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			vec irll(minDim, false);
			vec ll(minDim, false);
			vec b0(b.data, minDim, Type::Parasitic);
			vec b1(minDim, false);
			vec delta(minDim, false);
			b0 = a;
			for (unsigned long long c0(0); c0 < minDim; ++c0)
				irll.data[c0] = -data[c0 * width4d + c0];
			for (unsigned long long c0(0); c0 < 100; ++c0)
			{
				for (unsigned long long c1(0); c1 < 5; ++c1)
				{
					(*this)(b0, b1);
					b1 -= a;
					b1 *= irll;
					b1 += b0;
					(*this)(b1, b0);
					b0 -= a;
					b0 *= irll;
					b0 += b1;
				}
				delta = b1; delta -= b0;
				if (delta.norm1() < eps)
				{
					::printf("iters:\t%d\n", c0 * 10);
					return b;
				}
			}
			return b;
		}
		//symmetric
		vec& solveCholesky(vec const& a, vec& b)
		{
			//only use L mat since it's a symmetric matrix...
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			double s(1 / data[0]);
			vec tp(minDim, false);
			tp[0] = s;
			for (unsigned long long c0(1); c0 < minDim; ++c0)
				data[c0] = data[c0 * width4d] * s;
			for (unsigned long long c0(1); c0 < minDim; ++c0)
			{
				vec tll(data + c0 * width4d, c0, Type::Parasitic);
				vec bll(b.data, c0, Type::Parasitic);
				bll = tll; bll *= tp;
				tp[c0] = 1 / (data[c0 * width4d + c0] -= (bll, tll));
				for (unsigned long long c1(c0 + 1); c1 < minDim; ++c1)
				{
					double s(data[c1 * width4d + c0] -= (bll, vec(data + c1 * width4d, c0, Type::Parasitic)));
					data[c0 * width4d + c1] = s * tp[c0];
				}
			}
			solveL(a, tp);
			solveUid(tp, b);
		}
		//symmetric band (for LBandMat)
		vec& solveCholeskyBand(vec const& a, vec& b)
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			if (b.dim < minDim)
			{
				if (b.type == Type::Native)b.reconstruct(minDim, false);
				else return b;
			}
			double s(1 / data[0]);
			vec tp(minDim, false);
			tp[0] = s;
			mat uM(halfBandWidth, height, MatType::UBandMat, true);
			for (unsigned long long c0(1); c0 < 1 + halfBandWidth; ++c0)
				uM.data[c0] = data[c0 * width4d] * s;
			for (unsigned long long c0(1); c0 < minDim; ++c0)
			{
				unsigned long long len(c0 <= halfBandWidth ? c0 : halfBandWidth);
				unsigned long long bgn(LBandBeginOffset(c0));
				unsigned long long len4(ceiling4(len + bgn));
				vec tll(data + c0 * width4d, len4, Type::Parasitic);
				vec bll(b.data, len4, Type::Parasitic);
				unsigned long long tbgn(c0 <= halfBandWidth ? 0 : c0 - (long long(c0 - halfBandWidth) / 4) * 4);
				vec pll(tp.data + ((c0 - len) & -4), len4, Type::Parasitic);
				bll = tll; bll *= pll;
				vec bn(b.data + bgn, len, Type::Non32Aligened);
				tp[c0] = 1 / (LBandEleRef(c0, c0) -= (bn, tll));
				unsigned long long c1(c0 + 1);
				for (; c1 < c0 + halfBandWidth && c1 < minDim; ++c1)
				{
					unsigned long long bgn1(LBandBeginOffset(c1));
					unsigned long long end1(c1 <= halfBandWidth ? c0 : c0 - (long long(c1 - halfBandWidth) / 4) * 4);
					if (c1 > halfBandWidth && bgn1 == 0)
					{
						bll.data += 4;
						bll.dim -= 4;
					}
					double s(data[c1 * width4d + end1] -=
					(bll, vec(data + c1 * width4d + bgn1, end1 - bgn1, Type::Non32Aligened)));
					uM.UBandEleRef(c0, c1) = s * tp[c0];
				}
				if (c1 == c0 + halfBandWidth && c1 < minDim)
				{
					unsigned long long end1(c1 <= halfBandWidth ? c0 : c0 - (long long(c1 - halfBandWidth) / 4) * 4);
					uM.UBandEleRef(c0, c1) = data[c1 * width4d + end1] * tp[c0];
				}
			}
			solveL(a, tp);
			uM.solveUid(tp, b);
			return b;
		}
		vec& solveSteepestDescent(vec const& a, vec& b, double _eps)const
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			vec x0(b.data, minDim, Type::Parasitic);
			vec r(minDim, false);
			vec Ar(minDim, false);
			x0 = a;
			(*this)(x0, r);
			r -= a;
			for (unsigned long long c0(0); c0 < 1000; ++c0)
			{
				double eps(r.norm2Square());
				if (eps / minDim < _eps * _eps)
				{
					//::printf("iters:\t%d\n", c0);
					return b;
				}
				(*this)(r, Ar);
				double alpha(-eps / (r, Ar));
				x0.fmadd(alpha, r);
				r.fmadd(alpha, Ar);
			}
			return b;
		}
		vec& solveConjugateGradient(vec const& a, vec& b, double _eps)const
		{
			unsigned long long minDim(height > a.dim ? a.dim : height);
			if (!minDim)return b;
			vec x0(b.data, minDim, Type::Parasitic);
			vec r(minDim, false);
			vec p(minDim, false);
			vec Ap(minDim, false);
			//x0 = a;
			//(*this)(x0, r);
			//r -= a;
			//p = r;
			//for (unsigned long long c0(0); c0 < 100; ++c0)
			//{
			//	if (r.norm2() < _eps)
			//	{
			//		//::printf("iters:\t%d\n", c0 * 10);
			//		return b;
			//	}
			//	for (unsigned long long c1(0); c1 < 10; ++c1)
			//	{
			//		(*this)(p, Ap);
			//		double d((Ap, p));
			//		//double eps(r.norm2Square());
			//		double alpha(-(r, p) / d);
			//		x0.fmadd(alpha, p);
			//		r.fmadd(alpha, Ap);
			//		double beta((r, Ap) / d);
			//		p *= beta;
			//		p += r;
			//	}
			//}
			x0 = 0;
			(*this)(x0, r);
			r -= a;
			p = r;
			double rNorm(r.norm2Square());
			for (unsigned long long c0(0); c0 < 10000; ++c0)
			{
				if (rNorm / minDim < _eps * _eps)
				{
					::printf("iters:\t%d\n", c0);
					return b;
				}
				(*this)(p, Ap);
				double alpha(-rNorm / (Ap, p));
				x0.fmadd(alpha, p);
				r.fmadd(alpha, Ap);
				double rNorm1(rNorm);
				rNorm = r.norm2Square();
				double beta(rNorm / rNorm1);
				p *= beta;
				p += r;
			}
			return b;
		}

		void print()const
		{
			::printf("[\n");
			if (data)
			{
				for (unsigned long long c0(0); c0 < height; ++c0)
				{
					::printf("\t[%4.2f", data[width4d * c0]);
					unsigned long long ed(width);
					if (matType >= MatType::BandMat && matType < MatType::SparseMat)
						ed = width4d;
					for (unsigned long long c1(1); c1 < ed; ++c1)
						::printf(", %4.2f", data[width4d * c0 + c1]);
					::printf("]\n");
				}
			}
			::printf("]\n");
		}
		void printSparse()const
		{
			if (data && matType == MatType::SparseMat)
			{
				unsigned long long n(0);
				unsigned long long c0(0), c1(0);
				unsigned long long p0, p1;
				unsigned long long maxP1(0);
				while (n < elementNum)
				{
					p0 = rowIndice[n];
					p1 = colIndice[n];
					if (maxP1 < p1)maxP1 = p1;
					if (c0 < p0)
					{
						for (; c1 <= maxP1; ++c1)
							::printf("  0.0, ");
						::printf("\n");
						++c0; c1 = 0;
					}
					for (; c0 < p0; ++c0)
					{
						for (c1 = 0; c1 <= maxP1; ++c1)
							::printf("  0.0, ");
						::printf("\n");
						c1 = 0;
					}
					for (; c1 < p1; ++c1)
						::printf("  0.0, ");
					::printf("%5.1f, ", data[n++]);
					++c1;
				}
				::printf("\n");
			}
		}
		void printInfo()const
		{
			char const* str = "";
			switch (matType)
			{
			case MatType::NormalMat:str = "NormalMat"; break;
			case MatType::SquareMat:str = "SquareMat"; break;
			case MatType::DiagonalMat:str = "DiagonalMat"; break;
			case MatType::LMat:str = "LMat"; break;
			case MatType::UMat:str = "UMat"; break;
			case MatType::BandMat:str = "BandMat"; break;
			case MatType::LBandMat:str = "LBandMat"; break;
			case MatType::UBandMat:str = "UBandMat"; break;
			}
			::printf("{width: %u, height: %u, type: %s, matType: %s}\n", width, height,
				type == Type::Native ? "Native" : "Parasitic", str);
		}
		void printToTxt(char const* name)const
		{
			//in the form of Mathematica matrix(for paste)
			if (data)
			{
				FILE* temp(::fopen(name, "w+"));
				::fprintf(temp, "{\n");
				for (unsigned long long c0(0); c0 < height - 1; ++c0)
				{
					::fprintf(temp, "{%.14e", data[width4d * c0]);
					for (unsigned long long c1(1); c1 < width; ++c1)
						::fprintf(temp, ", %.14e", data[width4d * c0 + c1]);
					::fprintf(temp, "},\n");
				}
				::fprintf(temp, "{%.14e", data[width4d * (height - 1)]);
				for (unsigned long long c1(1); c1 < width; ++c1)
					::fprintf(temp, ", %.14e", data[width4d * (height - 1) + c1]);
				::fprintf(temp, "}\n}");
				::fclose(temp);
			}
		}
		void printToTableTxt(char const* name)const
		{
			//in the form of Mathematica matrix(for Import)
			if (data)
			{
				FILE* temp(::fopen(name, "w+"));
				for (unsigned long long c0(0); c0 < height; ++c0)
				{
					for (unsigned long long c1(0); c1 < width; ++c1)
						::fprintf(temp, "%4.2f ", data[width4d * c0 + c1]);
					::fprintf(temp, "\n");
				}
			}
		}
	};

	struct vecCplx
	{
		vec re;
		vec im;
		unsigned long long dim;
		unsigned long long begining;
		Type type;

		vecCplx() :re(), im(), dim(0), begining(0), type(Type::Native) {}
		vecCplx(unsigned long long _length, bool _clear = true)
			:
			re(_length, _clear),
			im(_length, _clear),
			dim(_length),
			begining(0),
			type(Type::Native)
		{
		}
		vecCplx(vecCplx const& a)
			:
			re(a.re),
			im(a.im),
			dim(a.dim),
			begining(0),
			type(Type::Native)
		{
		}
		vecCplx(vecCplx&& a) :re((vec&&)a.re), im((vec&&)a.im), dim(a.dim), begining(0), type(Type::Native)
		{
		}
		vecCplx(double* _re, double* _im, unsigned long long _length, Type _type)//Parasitic or Non32Aligened
			:
			re(_re, _length, _type),
			im(_im, _length, _type)
		{
		}

		void realloc(unsigned long long _dim, bool _clear = true)
		{
			re.realloc(_dim, _clear);
			im.realloc(_dim, _clear);
		}
		void reconstruct(unsigned long long _dim, bool _clear = true)
		{
			re.reconstruct(_dim, _clear);
			im.reconstruct(_dim, _clear);
		}
		void clearTail()
		{
			re.clearTail();
			im.clearTail();
		}
		vecCplx& operator =(vecCplx&& a)
		{
			re = (vec&&)a.re;
			im = (vec&&)a.im;
			return *this;
		}
		vecCplx& operator*=(vec const& a)
		{
			re *= a;
			im *= a;
			return *this;
		}
		vecCplx& operator/=(vec const& a)
		{
			re /= a;
			im /= a;
			return *this;
		}
		vecCplx& operator =(vecCplx const& a)
		{
			re = a.re;
			im = a.im;
			return *this;
		}
		vecCplx& operator+=(vecCplx const& a)
		{
			re += a.re;
			im += a.im;
			return *this;
		}
		vecCplx& operator-=(vecCplx const& a)
		{
			re -= a.re;
			im -= a.im;
		}
		vecCplx& operator*=(vecCplx const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				/*for (unsigned long long c1(0); c1 < minDim; ++c1)
				{
					double a1 = re.data[c1];
					double a2 = im.data[c1];
					double b1 = a.re.data[c1];
					double b2 = a.im.data[c1];
					re.data[c1] = a1 * b1 - a2 * b2;
					im.data[c1] = a2 * b1 + a1 * b2;
				}*/
				unsigned long long minDim4(minDim >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				__m256d* bRe((__m256d*)a.re.data);
				__m256d* bIm((__m256d*)a.im.data);
				unsigned long long c0(0);
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d b1 = bRe[c0];
					__m256d b2 = bIm[c0];
					aRe[c0] = _mm256_fmsub_pd(a1, b1, _mm256_mul_pd(a2, b2));
					aIm[c0] = _mm256_fmadd_pd(a2, b1, _mm256_mul_pd(a1, b2));
				}
				if ((c0 << 2) < minDim)
					for (unsigned long long c1(c0 << 2); c1 < minDim; ++c1)
					{
						double a1 = re.data[c1];
						double a2 = im.data[c1];
						double b1 = a.re.data[c1];
						double b2 = a.im.data[c1];
						re.data[c1] = a1 * b1 - a2 * b2;
						im.data[c1] = a2 * b1 + a1 * b2;
					}
			}
			return *this;
		}
		vecCplx& operator/=(vecCplx const& a)
		{
			if (a.dim && dim)
			{
				unsigned long long minDim(dim > a.dim ? a.dim : dim);
				/*for (unsigned long long c1(0); c1 < minDim; ++c1)
				{
					double a1 = re.data[c1];
					double a2 = im.data[c1];
					double b1 = a.re.data[c1];
					double b2 = a.im.data[c1];
					double dv, t0, t1;
					dv = b1 * b1 + b2 * b2;
					re.data[c1] = (a1 * b1 + a2 * b2) / dv;
					im.data[c1] = (a2 * b1 - a1 * b2) / dv;
				}*/
				unsigned long long minDim4(minDim >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				__m256d* bRe((__m256d*)a.re.data);
				__m256d* bIm((__m256d*)a.im.data);
				unsigned long long c0(0);
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d b1 = bRe[c0];
					__m256d b2 = bIm[c0];
					__m256d dv, t0, t1;
					dv = _mm256_fmadd_pd(b1, b1, _mm256_mul_pd(b2, b2));
					t0 = _mm256_fmadd_pd(a1, b1, _mm256_mul_pd(a2, b2));
					aRe[c0] = _mm256_div_pd(t0, dv);
					t1 = _mm256_fmsub_pd(a2, b1, _mm256_mul_pd(a1, b2));
					aIm[c0] = _mm256_div_pd(t1, dv);
				}
				if ((c0 << 2) < minDim)
					for (unsigned long long c1(c0 << 2); c1 < minDim; ++c1)
					{
						double a1 = re.data[c1];
						double a2 = im.data[c1];
						double b1 = a.re.data[c1];
						double b2 = a.im.data[c1];
						double dv, t0, t1;
						dv = b1 * b1 + b2 * b2;
						re.data[c1] = (a1 * b1 + a2 * b2) / dv;
						im.data[c1] = (a2 * b1 - a1 * b2) / dv;
					}
			}
			return *this;
		}
		vecCplx& operator+=(cplx a)
		{
			re += a.re;
			im += a.im;
			return *this;
		}
		vecCplx& operator-=(cplx a)
		{
			re -= a.re;
			im -= a.im;
			return *this;
		}
		vecCplx& operator*=(cplx a)
		{
			if (dim)
			{
				unsigned long long minDim4(dim >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				__m256d b1 = _mm256_broadcast_sd(&a.re);
				__m256d b2 = _mm256_broadcast_sd(&a.im);
				unsigned long long c0(0);
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					aRe[c0] = _mm256_fmsub_pd(a1, b1, _mm256_mul_pd(a2, b2));
					aIm[c0] = _mm256_fmadd_pd(a2, b1, _mm256_mul_pd(a1, b2));
				}
				if ((c0 << 2) < dim)
					for (unsigned long long c1(c0 << 2); c1 < dim; ++c1)
					{
						double a1 = re.data[c1];
						double a2 = im.data[c1];
						re.data[c1] = a1 * a.re - a2 * a.im;
						im.data[c1] = a2 * a.re + a1 * a.im;
					}
			}
			return *this;
		}
		vecCplx& operator/=(cplx a)
		{
			double an(a.norm());
			a.re /= an;
			a.im = -a.im / an;
			return (*this) *= a;
		}
		vecCplx& fmadd(double a, vecCplx const& b)
		{
			re.fmadd(a, b.re);
			im.fmadd(a, b.im);
			return *this;
		}
		vecCplx& fmadd(cplx a, vecCplx const& b)
		{
			if (b.dim && dim)
			{
				unsigned long long minDim(dim > b.dim ? b.dim : dim);
				unsigned long long minDim4(minDim >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				__m256d* bRe((__m256d*)b.re.data);
				__m256d* bIm((__m256d*)b.im.data);
				__m256d c1 = _mm256_broadcast_sd(&a.re);
				__m256d c2 = _mm256_broadcast_sd(&a.im);
				unsigned long long c0(0);
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d b1 = bRe[c0];
					__m256d b2 = bIm[c0];
					a1 = _mm256_fmsub_pd(b2, c2, a1);
					aRe[c0] = _mm256_fmadd_pd(b1, c1, a1);
					a2 = _mm256_fmadd_pd(b1, c2, a2);
					aIm[c0] = _mm256_fmadd_pd(b2, c1, a2);
				}
				if ((c0 << 2) < minDim)
					for (unsigned long long c1(c0 << 2); c1 < minDim; ++c1)
					{
						double b1 = b.re.data[c1];
						double b2 = b.im.data[c1];
						re.data[c1] += b1 * a.re - b2 * a.im;
						im.data[c1] += b2 * a.re + b1 * a.im;
					}
			}
			return *this;
		}
		//dot
		cplx operator,(vecCplx const& a)const
		{
			if (a.dim && dim)
			{
				unsigned long long e0(dim + begining);
				unsigned long long e1(a.dim + a.begining);
				unsigned long long minE(e0 >= e1 ? e1 : e0);
				double sRe(0), sIm(0);
				unsigned long long maxB(begining >= a.begining ? begining : a.begining);
				unsigned long long minDim4(minE >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				__m256d* bRe((__m256d*)a.re.data);
				__m256d* bIm((__m256d*)a.im.data);
				unsigned long long c0(0);
				__m256d tpRe = { 0 };
				__m256d tpIm = { 0 };
				if (begining)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d b1 = bRe[c0];
					__m256d b2 = bIm[c0];
					tpRe = _mm256_fmadd_pd(a1, b1, tpRe);
					tpIm = _mm256_fmadd_pd(a2, b1, tpIm);
					tpRe = _mm256_fmsub_pd(a2, b2, tpRe);
					tpIm = _mm256_fmadd_pd(a1, b2, tpIm);
					for (unsigned long long c1(0); c1 < maxB; ++c1)
						tpRe.m256d_f64[c1] = tpIm.m256d_f64[c1] = 0;
					++c0;
				}
				for (unsigned long long c1(minE); c1 < 4; ++c1)
					tpRe.m256d_f64[c1] = tpIm.m256d_f64[c1] = 0;
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d b1 = bRe[c0];
					__m256d b2 = bIm[c0];
					tpRe = _mm256_fmadd_pd(a1, b1, tpRe);
					tpIm = _mm256_fmadd_pd(a2, b1, tpIm);
					tpRe = _mm256_fmsub_pd(a2, b2, tpRe);
					tpIm = _mm256_fmadd_pd(a1, b2, tpIm);
				}
				for (unsigned long long c1(c0 << 2); c1 < minE; ++c1)
				{
					double a1 = re.data[c1];
					double a2 = im.data[c1];
					double b1 = a.re.data[c1];
					double b2 = a.im.data[c1];
					sRe += a1 * b1 - a2 * b2;
					sIm += a2 * b1 + a1 * b2;
				}
				sRe += tpRe.m256d_f64[0];
				sRe += tpRe.m256d_f64[1];
				sRe += tpRe.m256d_f64[2];
				sRe += tpRe.m256d_f64[3];
				sIm += tpIm.m256d_f64[0];
				sIm += tpIm.m256d_f64[1];
				sIm += tpIm.m256d_f64[2];
				sIm += tpIm.m256d_f64[3];
				return { sRe,sIm };
			}
			else return { 0 };
		}
		//normSquare (no complex conjugate)
		cplx normSquare()const
		{
			if (dim)
			{
				unsigned long long e0(dim + begining);
				double sRe(0), sIm(0);
				unsigned long long minDim4(e0 >> 2);
				__m256d* aRe((__m256d*)re.data);
				__m256d* aIm((__m256d*)im.data);
				unsigned long long c0(0);
				__m256d tpRe = { 0 };
				__m256d tpIm = { 0 };
				if (begining)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d tp = _mm256_mul_pd(a1, a2);
					tp = _mm256_mul_pd({ 2,2,2,2 }, tp);
					tpRe = _mm256_fmadd_pd(a1, a1, tpRe);
					tpIm = _mm256_add_pd(tpIm, tp);
					tpRe = _mm256_fmsub_pd(a2, a2, tpRe);
					for (unsigned long long c1(0); c1 < begining; ++c1)
						tpRe.m256d_f64[c1] = tpIm.m256d_f64[c1] = 0;
					++c0;
				}
				for (unsigned long long c1(e0); c1 < 4; ++c1)
					tpRe.m256d_f64[c1] = tpIm.m256d_f64[c1] = 0;
				for (; c0 < minDim4; ++c0)
				{
					__m256d a1 = aRe[c0];
					__m256d a2 = aIm[c0];
					__m256d tp = _mm256_mul_pd(a1, a2);
					tp = _mm256_mul_pd({ 2,2,2,2 }, tp);
					tpRe = _mm256_fmadd_pd(a1, a1, tpRe);
					tpIm = _mm256_add_pd(tpIm, tp);
					tpRe = _mm256_fmsub_pd(a2, a2, tpRe);
				}
				for (unsigned long long c1(c0 << 2); c1 < e0; ++c1)
				{
					double a1 = re.data[c1];
					double a2 = im.data[c1];
					sRe += a1 * a1 - a2 * a2;
					sIm += 2 * a1 * a2;
				}
				sRe += tpRe.m256d_f64[0];
				sRe += tpRe.m256d_f64[1];
				sRe += tpRe.m256d_f64[2];
				sRe += tpRe.m256d_f64[3];
				sIm += tpIm.m256d_f64[0];
				sIm += tpIm.m256d_f64[1];
				sIm += tpIm.m256d_f64[2];
				sIm += tpIm.m256d_f64[3];
				return { sRe,sIm };
			}
			else return { 0 };
		}

		void print()const
		{
			::printf("[");
			unsigned long long finalDim(dim + begining);
			if (dim)
			{
				for (unsigned long long c0(begining); c0 < finalDim - 1; ++c0)
					::printf("{%.4f, %.4f}, ", re.data[c0], im.data[c0]);
				::printf("{%.4f, %.4f}", re.data[finalDim - 1], im.data[finalDim - 1]);
			}
			::printf("]\n");
		}
	};
	struct matCplx
	{
		mat re;
		mat im;
		Type type;
		MatType matType;

		matCplx(MatType _type, unsigned long long _elementNumRe, unsigned long long _elementNumIm)
			:
			re(_type, _elementNumRe),
			im(_type, _elementNumIm),
			type(Type::Native),
			matType(_type)
		{
		}

		vecCplx& operator()(vecCplx const& a, vecCplx& b)const
		{
			unsigned long long w(matType < MatType::BandMat ? re.width : re.height);
			if (matType == MatType::SparseMat)w = a.dim;
			unsigned long long minDim(w > a.dim ? a.dim : w);
			if (minDim)
			{
				bool overflow(ceiling4(minDim) > ceiling4(b.dim));
				if (overflow && b.type != Type::Native)return b;
				vecCplx const* source(&a);
				vecCplx r;
				if (&b == source)
				{
					source = &r;
					r = a;
				}
				if (overflow)b.reconstruct(minDim, false);
				switch (matType)
				{
				case MatType::NormalMat:
				case MatType::SquareMat:
				case MatType::DiagonalMat:
				case MatType::SymmetricMat:
				case MatType::LMat:
				case MatType::UMat:
					/*{
						constexpr unsigned long long warp = 8;
						unsigned long long minWidth4((minDim - 1) / 4 + 1);
						__m256d* aData((__m256d*)data);
						__m256d* bData((__m256d*)source->data);
						__m256d* rData((__m256d*)b.data);
						unsigned long long heightFloor4((height >> 2) << 2);
						unsigned long long widthWarp((minWidth4 / warp) * warp);
						unsigned long long warpLeftFloor((minDim >> 2) - widthWarp);
						unsigned long long warpLeftCeiling(minWidth4 - widthWarp);
						unsigned long long c0(0);
						for (; c0 < heightFloor4; c0 += 4)
						{
							__m256d ans[4] = { 0 };
							__m256d tp[warp];
							unsigned long long c1(0);
							for (; c1 < widthWarp; c1 += warp)
							{
								__m256d* s(aData + minWidth4 * c0 + c1);
	#pragma unroll(4)
								for (unsigned long long c2(0); c2 < warp; ++c2)
									tp[c2] = bData[c1 + c2];
								for (unsigned long long c2(0); c2 < 4; ++c2, s += minWidth4)
								{
	#pragma unroll(4)
									for (unsigned long long c3(0); c3 < warp; ++c3)
									{
										__m256d t = s[c3];
										ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
									}
								}
							}
							if (c1 < minWidth4)
							{
								__m256d* s(aData + minWidth4 * c0 + c1);
	#pragma unroll(4)
								for (unsigned long long c2(0); c2 < warpLeftCeiling; ++c2)
									tp[c2] = bData[c1 + c2];
								unsigned long long finalWidth(minDim - ((minDim >> 2) << 2));
								for (unsigned long long c2(finalWidth); c2 < 4; ++c2)
									tp[warpLeftFloor].m256d_f64[c2] = 0;
								for (unsigned long long c2(0); c2 < 4; ++c2, s += minWidth4)
								{
	#pragma unroll(4)
									for (unsigned long long c3(0); c3 < warpLeftCeiling; ++c3)
									{
										__m256d t = s[c3];
										ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
									}
								}
							}
							__m256d s;
							for (unsigned long long c1(0); c1 < 4; ++c1)
							{
								s.m256d_f64[c1] = ans[c1].m256d_f64[0];
								s.m256d_f64[c1] += ans[c1].m256d_f64[1];
								s.m256d_f64[c1] += ans[c1].m256d_f64[2];
								s.m256d_f64[c1] += ans[c1].m256d_f64[3];
							}
							rData[c0 >> 2] = s;
						}
						if (c0 < height)
						{
							unsigned long long heightLeft(height - heightFloor4);
							__m256d ans[4] = { 0 };
							__m256d tp[warp];
							unsigned long long c1(0);
							for (; c1 < widthWarp; c1 += warp)
							{
								__m256d* s(aData + minWidth4 * c0 + c1);
	#pragma unroll(4)
								for (unsigned long long c2(0); c2 < warp; ++c2)
									tp[c2] = bData[c1 + c2];
								for (unsigned long long c2(0); c2 < heightLeft; ++c2, s += minWidth4)
								{
	#pragma unroll(4)
									for (unsigned long long c3(0); c3 < warp; ++c3)
									{
										__m256d t = s[c3];
										ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
									}
								}
							}
							if (c1 < minWidth4)
							{
								__m256d* s(aData + minWidth4 * c0 + c1);
	#pragma unroll(4)
								for (unsigned long long c2(0); c2 < warpLeftCeiling; ++c2)
									tp[c2] = bData[c1 + c2];
								unsigned long long finalWidth(minDim - ((minDim >> 2) << 2));
								for (unsigned long long c2(finalWidth); c2 < 4; ++c2)
									tp[warpLeftFloor].m256d_f64[c2] = 0;
								for (unsigned long long c2(0); c2 < heightLeft; ++c2, s += minWidth4)
								{
	#pragma unroll(4)
									for (unsigned long long c3(0); c3 < warpLeftCeiling; ++c3)
									{
										__m256d t = s[c3];
										ans[c2] = _mm256_fmadd_pd(t, tp[c3], ans[c2]);
									}
								}
							}
							__m256d s;
							for (unsigned long long c1(0); c1 < heightLeft; ++c1)
							{
								s.m256d_f64[c1] = ans[c1].m256d_f64[0];
								s.m256d_f64[c1] += ans[c1].m256d_f64[1];
								s.m256d_f64[c1] += ans[c1].m256d_f64[2];
								s.m256d_f64[c1] += ans[c1].m256d_f64[3];
							}
							rData[c0 >> 2] = s;
						}
						break;
					}*/
				case MatType::BandMat:
					/*{
						for (unsigned long long c0(0); c0 < height; ++c0)
						{
							vec tp(getBandRow(c0));
							unsigned long long bgn(c0 <= halfBandWidth ? 0 : c0 - halfBandWidth);
							vec ta(a.data + bgn, tp.dim, Type::Non32Aligened);
							b.data[c0] = (tp, ta);
						}
						break;
					}*/
				case MatType::LBandMat:
					/*{
						for (unsigned long long c0(0); c0 < height; ++c0)
						{
							vec tp(getLBandRow(c0));
							unsigned long long bgn(c0 <= halfBandWidth ? 0 : c0 - halfBandWidth);
							vec ta(a.data + bgn, tp.dim, Type::Non32Aligened);
							b.data[c0] = (tp, ta);
						}
						break;
					}*/
				case MatType::UBandMat:
					/*{
						for (unsigned long long c0(0); c0 < height; ++c0)
						{
							vec tp(getUBandRow(c0));
							vec ta(a.data + c0, tp.dim, Type::Non32Aligened);
							b.data[c0] = (tp, ta);
						}
						break;
					}*/
					break;
				case MatType::SparseMat:
				{
					unsigned long long n(0);
					for (unsigned long long c0(0); c0 < minDim && n < re.elementNum; ++c0)
					{
						b.re.data[c0] = 0;
						b.im.data[c0] = 0;
						if (re.rowIndice[n] > c0)continue;
						else
						{
							while (n < re.elementNum)
							{
								if (re.rowIndice[n] > c0)break;
								double red(re.data[n]);
								unsigned long long rec(re.colIndice[n]);
								b.re.data[c0] += red * a.re.data[rec];
								b.im.data[c0] += red * a.im.data[rec];
								n++;
							}
						}
					}
					n = 0;
					for (unsigned long long c0(0); c0 < minDim && n < im.elementNum; ++c0)
					{
						if (im.rowIndice[n] > c0)continue;
						else
						{
							while (n < im.elementNum)
							{
								if (im.rowIndice[n] > c0)break;
								double red(im.data[n]);
								unsigned long long rec(im.colIndice[n]);
								b.re.data[c0] -= red * a.im.data[rec];
								b.im.data[c0] += red * a.re.data[rec];
								n++;
							}
						}
					}
				}
				}
				return b;
			}
		}
		void printSparse()const
		{
			if ((re.data || im.data) && matType == MatType::SparseMat)
			{
				unsigned long long nre(0), nim(0);
				unsigned long long c0(0), c1(0);
				unsigned long long p0re, p0im, p1re, p1im;
				unsigned long long maxP1(0);
				bool flagre(nre < re.elementNum), flagim(nim < im.elementNum);
				while (flagre || flagim)
				{
					if (flagre)
					{
						p0re = re.rowIndice[nre];
						p1re = re.colIndice[nre];
						if (maxP1 < p1re)maxP1 = p1re;
					}
					if (flagim)
					{
						p0im = im.rowIndice[nim];
						p1im = im.colIndice[nim];
						if (maxP1 < p1im)maxP1 = p1im;
					}
					unsigned long long minP0;
					unsigned long long chosenP1;
					unsigned long long which;
					if (!flagim)
					{
						which = 1;
						minP0 = p0re;
						chosenP1 = p1re;
					}
					else if (!flagre)
					{
						which = 2;
						minP0 = p0im;
						chosenP1 = p1im;
					}
					else if (p0re < p0im)
					{
						which = 1;
						minP0 = p0re;
						chosenP1 = p1re;
					}
					else if (p0re == p0im)
					{
						minP0 = p0re;
						if (p1re < p1im)
						{
							which = 1;
							chosenP1 = p1re;
						}
						else if (p1re == p1im)
						{
							which = 3;
							chosenP1 = p1re;
						}
						else if (p1re > p1im)
						{
							which = 2;
							chosenP1 = p1im;
						}
					}
					else if (p0re > p0im)
					{
						which = 2;
						minP0 = p0im;
						chosenP1 = p1im;
					}
					if (c0 < minP0)
					{
						for (; c1 <= maxP1; ++c1)
							::printf("  0.0 + 0.0i, ");
						::printf("\n");
						++c0; c1 = 0;
					}
					for (; c0 < minP0; ++c0)
					{
						for (c1 = 0; c1 <= maxP1; ++c1)
							::printf("  0.0 + 0.0i, ");
						::printf("\n");
						c1 = 0;
					}
					for (; c1 < chosenP1; ++c1)
						::printf("  0.0 + 0.0i, ");
					switch (which)
					{
					case 1: ::printf("%5.1f + 0.0i, ", re.data[nre++]); break;
					case 2: ::printf("  0.0 +%2.1fi, ", im.data[nim++]); break;
					case 3: ::printf("%5.1f +%2.1fi, ", re.data[nre++], im.data[nim++]); break;
					}
					++c1;
					flagre = nre < re.elementNum;
					flagim = nim < im.elementNum;
				}
				::printf("\n");
			}
		}
	};

	//non-in-situ mult mat
	vec vec::operator()(mat const& a)const
	{
		unsigned long long minDim(a.height > dim ? dim : a.height);
		if (minDim)
		{
			vec r(a.width, false);
			/*for (unsigned long long c0(0); c0 < minDim; ++c0)
				for (unsigned long long c1(0); c1 < a.width; ++c1)
					r.data[c1] += data[c0] * a.data[c0 * a.width4d + c1];*/
			return (*this)(a, r);
		}
		return vec();
	}
	vec& vec::operator()(mat const& a, vec& b)const
	{
		unsigned long long minDim(a.height > dim ? dim : a.height);
		if (minDim)
		{
			bool overflow(ceiling4(minDim) > ceiling4(b.dim));
			if (overflow && b.type != Type::Native)return b;
			vec const* source(this);
			vec r;
			if (&b == this)
			{
				source = &r;
				r = *this;
			}
			if (overflow)b.reconstruct(minDim, false);
			constexpr unsigned long long warp = 16;
			unsigned long long width4(a.width4d >> 2);
			unsigned long long widthWarpFloor((width4 / warp) * warp);
			unsigned long long minDim4Floor(minDim & -4);
			__m256d* aData((__m256d*)a.data);
			__m256d* bData((__m256d*)b.data);
			unsigned long long c0(0);
			for (; c0 < widthWarpFloor; c0 += warp)
			{
				__m256d ans[warp] = { 0 };
				unsigned long long c1(0);
				__m256d tp[4];
				for (; c1 < minDim4Floor; c1 += 4)
				{
					tp[0] = _mm256_broadcast_sd(source->data + c1);
					tp[1] = _mm256_broadcast_sd(source->data + c1 + 1);
					tp[2] = _mm256_broadcast_sd(source->data + c1 + 2);
					tp[3] = _mm256_broadcast_sd(source->data + c1 + 3);
					__m256d* s(aData + width4 * c1 + c0);
#pragma unroll(4)
					for (unsigned long long c2(0); c2 < 4; ++c2)
					{
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warp; ++c3)
							ans[c3] = _mm256_fmadd_pd(s[c2 * width4 + c3], tp[c2], ans[c3]);
					}
				}
				if (c1 < minDim)
				{
					unsigned long long deltaMinDim(minDim - c1);
					for (unsigned long long c2(0); c2 < deltaMinDim; ++c2)
					{
						double b = source->data[c1 + c2];
						tp[c2] = { b,b,b,b };
					}
					__m256d* s(aData + width4 * c1 + c0);
#pragma unroll(4)
					for (unsigned long long c2(0); c2 < deltaMinDim; ++c2)
					{
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warp; ++c3)
							ans[c3] = _mm256_fmadd_pd(s[c2 * width4 + c3], tp[c2], ans[c3]);
					}
				}
#pragma unroll(4)
				for (unsigned long long c3(0); c3 < warp; ++c3)
					bData[c0 + c3] = ans[c3];
			}
			if (c0 < a.width4d)
			{
				unsigned long long warpLeft(width4 - widthWarpFloor);
				__m256d ans[warp] = { 0 };
				unsigned long long c1(0);
				__m256d tp[4];
				for (; c1 < minDim4Floor; c1 += 4)
				{
					tp[0] = _mm256_broadcast_sd(source->data + c1);
					tp[1] = _mm256_broadcast_sd(source->data + c1 + 1);
					tp[2] = _mm256_broadcast_sd(source->data + c1 + 2);
					tp[3] = _mm256_broadcast_sd(source->data + c1 + 3);
					__m256d* s(aData + width4 * c1 + c0);
#pragma unroll(4)
					for (unsigned long long c2(0); c2 < 4; ++c2)
					{
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warpLeft; ++c3)
							ans[c3] = _mm256_fmadd_pd(s[c2 * width4 + c3], tp[c2], ans[c3]);
					}
				}
				if (c1 < minDim)
				{
					unsigned long long deltaMinDim(minDim - c1);
					for (unsigned long long c2(0); c2 < deltaMinDim; ++c2)
					{
						double b = source->data[c1 + c2];
						tp[c2] = { b,b,b,b };
					}
					__m256d* s(aData + width4 * c1 + c0);
#pragma unroll(4)
					for (unsigned long long c2(0); c2 < deltaMinDim; ++c2)
					{
#pragma unroll(4)
						for (unsigned long long c3(0); c3 < warpLeft; ++c3)
							ans[c3] = _mm256_fmadd_pd(s[c2 * width4 + c3], tp[c2], ans[c3]);
					}
				}
#pragma unroll(4)
				for (unsigned long long c3(0); c3 < warpLeft; ++c3)
					bData[c0 + c3] = ans[c3];
			}
		}
		return b;
	}

	//misc
	template<class T>void randomVec(vec& a, std::mt19937& mt, T& rd)
	{
		for (unsigned long long c0(0); c0 < a.dim; ++c0)
			a.data[c0] = rd(mt);
	}
	template<class T>void randomVecCplx(vecCplx& a, std::mt19937& mt, T& rd)
	{
		for (unsigned long long c0(0); c0 < a.dim; ++c0)
			a.re.data[c0] = rd(mt);
		for (unsigned long long c0(0); c0 < a.dim; ++c0)
			a.im.data[c0] = rd(mt);
	}
	template<class T>void randomMat(mat& a, std::mt19937& mt, T& rd)
	{
		for (unsigned long long c0(0); c0 < a.height; ++c0)
			for (unsigned long long c1(0); c1 < a.width; ++c1)
				a(c0, c1) = rd(mt);
	}
	template<class T>void randomMatGood(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(0);
			for (; c1 < a.width && c1 < c0; ++c1)
				a(c0, c1) = ratio * rd(mt);
			a(c0, c1++) = 1 + ratio * rd(mt);
			for (; c1 < a.width; ++c1)
				a(c0, c1) = ratio * rd(mt);
		}
	}
	template<class T>void randomMatL(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(0);
			for (; c1 < a.width && c1 < c0; ++c1)
				a(c0, c1) = ratio * rd(mt);
			a(c0, c1++) = 1 + ratio * rd(mt);
			for (; c1 < a.width; ++c1)
				a(c0, c1) = 0;
		}
	}
	template<class T>void randomMatSymmetric(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		//make sure square matrix!
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(0);
			for (; c1 < a.width && c1 < c0; ++c1)
				a(c1, c0) = a(c0, c1) = ratio * rd(mt);
			a(c0, c1++) = 1 + ratio * rd(mt);
		}
	}
	template<class T>void randomMatU(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(0);
			for (; c1 < a.width && c1 < c0; ++c1)
				a(c0, c1) = 0;
			a(c0, c1++) = 1 + ratio * rd(mt);
			for (; c1 < a.width; ++c1)
				a(c0, c1) = ratio * rd(mt);
		}
	}
	template<class T>void randomMatBandL(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		a.clear();
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(a.LBandBeginOffset(c0));
			unsigned long long ending(c0 <= a.halfBandWidth ? c0 + 1 : a.halfBandWidth + 1);
			ending += c1;
			for (; c1 < ending - 1; ++c1)
				a.data[c0 * a.width4d + c1] = ratio * rd(mt);
			a.data[c0 * a.width4d + c1] = 1 + ratio * rd(mt);
		}
	}
	template<class T>void randomMatBandU(mat& a, std::mt19937& mt, T& rd, double ratio)
	{
		a.clear();
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long c1(c0 % 4);
			unsigned long long ending(a.height - c0 <= a.halfBandWidth ? a.height - c0 : a.halfBandWidth + 1);
			ending += c1;
			a.data[c0 * a.width4d + c1++] = 1 + ratio * rd(mt);
			for (; c1 < ending; ++c1)
				a.data[c0 * a.width4d + c1] = ratio * rd(mt);
		}
	}
	mat& transBandToNormalMat(mat const& a, mat& b)
	{
		if (b.width != a.height || b.height != a.height)
			b.reconstruct(a.height, a.height, false);
		b.clear();
		if (a.matType == MatType::LBandMat)
		{
			for (unsigned long long c0(0); c0 < a.height; ++c0)
			{
				unsigned long long bgn(a.LBandBeginOffset(c0));
				unsigned long long ost(c0 <= a.halfBandWidth ? c0 : a.halfBandWidth);
				memcpy(b.data + c0 * b.width4d + c0 - ost,
					a.data + c0 * a.width4d + bgn, (ost + 1) * sizeof(double));
			}
		}
		else if (a.matType == MatType::UBandMat)
		{
			for (unsigned long long c0(0); c0 < a.height; ++c0)
			{
				unsigned long long ost(a.height - c0 <= a.halfBandWidth ? a.height - c0 : a.halfBandWidth + 1);
				memcpy(b.data + c0 * b.width4d + c0,
					a.data + c0 * a.width4d + c0 % 4, ost * sizeof(double));
			}
		}
		return b;
	}
	mat& transLBandToSymmetricMat(mat const& a, mat& b)
	{
		if (b.width != a.height || b.height != a.height)
			b.reconstruct(a.height, a.height, false);
		b.clear();
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long bgn(a.LBandBeginOffset(c0));
			unsigned long long ost(c0 <= a.halfBandWidth ? c0 : a.halfBandWidth);
			memcpy(b.data + c0 * b.width4d + c0 - ost,
				a.data + c0 * a.width4d + bgn, (ost + 1) * sizeof(double));
		}
		for (unsigned long long c0(0); c0 < a.height - 1; ++c0)
			for (unsigned long long c1(c0 + 1); c1 < a.height; ++c1)
				b.data[c0 * b.width4d + c1] = b.data[c1 * b.width4d + c0];
		return b;
	}
	mat& transLBandToSymmetricBandMat(mat const& a, mat& b)
	{
		//use the size of b
		b.clear();
		for (unsigned long long c0(0); c0 < a.height; ++c0)
		{
			unsigned long long bgn(a.LBandBeginOffset(c0));
			unsigned long long ost(c0 <= a.halfBandWidth ? c0 : a.halfBandWidth);
			memcpy(b.data + c0 * b.width4d + bgn,
				a.data + c0 * a.width4d + bgn, (ost + 1) * sizeof(double));
		}
		for (unsigned long long c0(0); c0 < a.height - 1; ++c0)
			for (unsigned long long c1(c0 + 1); c1 <= c0 + a.halfBandWidth && c1 < a.height; ++c1)
				b.BandEleRef(c0, c1) = b.BandEleRef(c1, c0);
		return b;
	}
	mat& transNormalMatToBand(mat const& a, mat& b)
	{
		//use the size of b
		b.clear();
		unsigned long long bgn, end;
		for (unsigned long long c0(0); c0 < b.height; ++c0)
		{
			if (c0 <= b.halfBandWidth)bgn = 0;
			else bgn = c0 - b.halfBandWidth;
			if (c0 >= b.height - b.halfBandWidth)end = b.height;
			else end = c0 + b.halfBandWidth + 1;
			memcpy(b.data + c0 * b.width4d + b.BandBeginOffset(c0),
				a.data + a.width4d * c0 + bgn, (end - bgn) * sizeof(double));
		}
		return b;
	}
}