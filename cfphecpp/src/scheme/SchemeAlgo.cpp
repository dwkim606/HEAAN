#include "SchemeAlgo.h"

#include <NTL/ZZ.h>
#include <cmath>
#include <map>
#include <thread>
#include <future>

#include "Params.h"

Cipher SchemeAlgo::powerOf2(Cipher& cipher, const long& logDegree) {
	Cipher res = cipher;
	for (long i = 0; i < logDegree; ++i) {
		scheme.squareAndEqual(res);
		scheme.modSwitchOneAndEqual(res);
	}
	return res;
}

Cipher* SchemeAlgo::powerOf2Extended(Cipher& cipher, const long& logDegree) {
	Cipher* res = new Cipher[logDegree + 1];
	res[0] = cipher;
	for (long i = 1; i < logDegree + 1; ++i) {
		res[i] = scheme.square(res[i-1]);
		scheme.modSwitchOneAndEqual(res[i]);
	}
	return res;
}

//-----------------------------------------

Cipher SchemeAlgo::power(Cipher& cipher, const long& degree) {
	long logDegree = log2(degree);
	long po2Degree = 1 << logDegree;

	Cipher res = powerOf2(cipher, logDegree);
	long remDegree = degree - po2Degree;
	if(remDegree > 0) {
		Cipher tmp = power(cipher, remDegree);
		scheme.modEmbedAndEqual(tmp, res.level);
		scheme.multModSwitchOneAndEqual(res, tmp);
	}
	return res;
}

Cipher* SchemeAlgo::powerExtended(Cipher& cipher, const long& degree) {
	Cipher* res = new Cipher[degree];
	long logDegree = log2(degree);
	Cipher* cpows = powerOf2Extended(cipher, logDegree);
	long idx = 0;
	for (long i = 0; i < logDegree; ++i) {
		long powi = (1 << i);
		res[idx++] = cpows[i];
		for (int j = 0; j < powi-1; ++j) {
			res[idx] = scheme.modEmbed(res[j], cpows[i].level);
			scheme.multModSwitchOneAndEqual(res[idx++], cpows[i]);
		}
	}
	res[idx++] = cpows[logDegree];
	long degree2 = (1 << logDegree);
	for (int i = 0; i < (degree - degree2); ++i) {
		res[idx] = scheme.modEmbed(res[i], cpows[logDegree].level);
		scheme.multModSwitchOneAndEqual(res[idx++], cpows[logDegree]);
	}
	return res;
}

//-----------------------------------------

Cipher SchemeAlgo::prod2(Cipher*& ciphers, const long& logDegree) {
	Cipher* res = ciphers;
	for (long i = logDegree; i > 0; --i) {
		long powi = (1 << i);
		long powih = (powi >> 1);
		Cipher* cprodvec = new Cipher[powih];
		for (long j = 0; j < powih; ++j) {
			cprodvec[j] = scheme.mult(res[2 * j], res[2 * j + 1]);
			scheme.modSwitchOneAndEqual(cprodvec[j]);
		}
		res = cprodvec;
	}
	return res[0];
}

Cipher* SchemeAlgo::multAndModSwitchVec(Cipher*& ciphers1, Cipher*& ciphers2, long& size) {
	Cipher* res = new Cipher[size];
	thread*  thpool = new thread[size];
	for (long i = 0; i < size; ++i) {
		thpool[i] = thread(&SchemeAlgo::dummymult, this, ref(res[i]), ref(ciphers1[i]), ref(ciphers2[i]));
	}
	for (long i = 0; i < size; ++i) {
		thpool[i].join();
		//TODO check what is the problem with the code
	}
	return res;
}

void SchemeAlgo::multModSwitchAndEqualVec(Cipher*& ciphers1, Cipher*& ciphers2, long& size) {
	thread* thpool = new thread[size];
	for (long i = 0; i < size; ++i) {
		thpool[i] = thread(&SchemeAlgo::dummymultequal, this, ref(ciphers1[i]), ref(ciphers2[i]));
	}
	for (long i = 0; i < size; ++i) {
		//TODO check what is the problem with the code
		thpool[i].join();
	}
}

void SchemeAlgo::dummymult(Cipher& res, Cipher& c1, Cipher& c2) {
	res = scheme.mult(c1, c2);
	scheme.modSwitchOneAndEqual(res);
}

void SchemeAlgo::dummymultequal(Cipher& c1, Cipher& c2) {
	scheme.multAndEqual(c1, c2);
	scheme.modSwitchOneAndEqual(c1);
}

//-----------------------------------------

Cipher SchemeAlgo::inverse(Cipher& cipher, const long& steps) {
	Cipher cpow = cipher;
	Cipher tmp = scheme.addConst(cipher, scheme.params.p);
	scheme.modEmbedOneAndEqual(tmp);
	Cipher res = tmp;

	for (long i = 1; i < steps; ++i) {
		scheme.squareAndEqual(cpow);
		scheme.modSwitchOneAndEqual(cpow);
		tmp = cpow;
		scheme.addConstAndEqual(tmp, scheme.params.p);
		scheme.multAndEqual(tmp, res);
		scheme.modSwitchAndEqual(tmp, i + 2);
		res = tmp;
	}
	return res;
}

Cipher* SchemeAlgo::inverseExtended(Cipher& cipher, const long& steps) {
	Cipher* res = new Cipher[steps];
	Cipher cpow = cipher;
	Cipher tmp = scheme.addConst(cipher, scheme.params.p);
	scheme.modEmbedOneAndEqual(tmp);
	res[0] = tmp;

	for (long i = 1; i < steps; ++i) {
		scheme.squareAndEqual(cpow);
		scheme.modSwitchOneAndEqual(cpow);
		tmp = cpow;
		scheme.addConstAndEqual(tmp, scheme.params.p);
		scheme.multAndEqual(tmp, res[i - 1]);
		scheme.modSwitchAndEqual(tmp, i + 2);
		res[i] = tmp;
	}
	return res;
}

//-----------------------------------------

Cipher SchemeAlgo::function(Cipher& cipher, string& funcName, const long& degree) {
	Cipher* cpows = powerExtended(cipher, degree);

	ZZ* pows = scheme.aux.taylorPowsMap.at(funcName);
	double* coeffs = scheme.aux.taylorCoeffsMap.at(funcName);

	Cipher res = scheme.multByConst(cpows[0], pows[1]);
	ZZ a0 = pows[0] << scheme.params.logp;
	scheme.addConstAndEqual(res, a0);

	for (int i = 1; i < degree; ++i) {
		if(abs(coeffs[i + 1]) > 1e-17) {
			Cipher aixi = scheme.multByConst(cpows[i], pows[i + 1]);
			scheme.modEmbedAndEqual(res, aixi.level);
			scheme.addAndEqual(res, aixi);
		}
	}
	scheme.modSwitchOneAndEqual(res);
	return res;
}

Cipher SchemeAlgo::functionLazy(Cipher& cipher, string& funcName, const long& degree) {
	Cipher* cpows = powerExtended(cipher, degree);

	ZZ* pows = scheme.aux.taylorPowsMap.at(funcName);
	double* coeffs = scheme.aux.taylorCoeffsMap.at(funcName);

	Cipher res = scheme.multByConst(cpows[0], pows[1]);
	ZZ a0 = pows[0] << scheme.params.logp;
	scheme.addConstAndEqual(res, a0);

	for (int i = 1; i < degree; ++i) {
		if(abs(coeffs[i + 1]) > 1e-27) {
			Cipher aixi = scheme.multByConst(cpows[i], pows[i + 1]);
			scheme.modEmbedAndEqual(res, aixi.level);
			scheme.addAndEqual(res, aixi);
		}
	}
	return res;
}

Cipher* SchemeAlgo::functionExtended(Cipher& cipher, string& funcName, const long& degree) {
	Cipher* cpows = powerExtended(cipher, degree);

	ZZ* pows = scheme.aux.taylorPowsMap.at(funcName);
	double* coeffs = scheme.aux.taylorCoeffsMap.at(funcName);

	Cipher aixi = scheme.multByConst(cpows[0], pows[1]);
	ZZ a0 = pows[0] << scheme.params.logp;
	scheme.addConstAndEqual(aixi, a0);
	Cipher* res = new Cipher[degree];
	res[0] = aixi;
	for (long i = 1; i < degree; ++i) {
		if(abs(coeffs[i + 1]) > 1e-17) {
			aixi = scheme.multByConst(cpows[i], pows[i + 1]);
			Cipher tmp = scheme.modEmbed(res[i - 1], aixi.level);
			scheme.addAndEqual(aixi, tmp);
			res[i] = aixi;
		} else {
			res[i] = res[i - 1];
		}
	}
	for (long i = 0; i < degree; ++i) {
		scheme.modSwitchOneAndEqual(res[i]);
	}
	return res;
}

//-----------------------------------------

Cipher* SchemeAlgo::fftRaw(Cipher*& ciphers, const long& size, const bool& isForward) {

	if(size == 1) {
		return ciphers;
	}

	long sizeh = size >> 1;

	Cipher* sub1 = new Cipher[sizeh];
	Cipher* sub2 = new Cipher[sizeh];

	for (long i = 0; i < sizeh; ++i) {
		sub1[i] = ciphers[2 * i];
		sub2[i] = ciphers[2 * i + 1];
	}

	future<Cipher*> f1 = async(&SchemeAlgo::fftRaw, this, ref(sub1), ref(sizeh), ref(isForward));
	future<Cipher*> f2 = async(&SchemeAlgo::fftRaw, this, ref(sub2), ref(sizeh), ref(isForward));

	Cipher* y1 = f1.get();
	Cipher* y2 = f2.get();

	long shift = isForward ? (scheme.params.M / size) : (scheme.params.M - scheme.params.M / size);

	Cipher* res = new Cipher[size];

	thread* thpool = new thread[sizeh];
	for (long i = 0; i < sizeh; ++i) {
		thpool[i] = thread(&SchemeAlgo::dummy, this, ref(res[i]), ref(res[i + sizeh]), ref(y1[i]), ref(y2[i]), shift * i);
	}
	for (long i = 0; i < sizeh; ++i) {
		thpool[i].join();
	}
	return res;
}

void SchemeAlgo::dummy(Cipher& res1, Cipher& res2, Cipher& y1, Cipher& y2, long shift) {
	scheme.multByMonomialAndEqual(y2, shift);
	res1 = y1;
	res2 = y1;
	scheme.addAndEqual(res1, y2);
	scheme.subAndEqual(res2, y2);
}

Cipher* SchemeAlgo::fft(Cipher*& ciphers, const long& size) {
	return fftRaw(ciphers, size, true);
}

Cipher* SchemeAlgo::fftInv(Cipher*& ciphers, const long& size) {
	Cipher* fftInv = fftRaw(ciphers, size, false);
	long logsize = log2(size);
	long bits = scheme.params.logp - logsize;
	thread* thpool = new thread[size];
	for (long i = 0; i < size; ++i) {
		thpool[i] = thread(&SchemeAlgo::rescale, this, ref(fftInv[i]), ref(bits));
	}
	for(long i = 0; i < size; ++i) {
		thpool[i].join();
	}
	return fftInv;
}

void SchemeAlgo::rescale(Cipher& c, long& bits) {
	scheme.leftShiftAndEqual(c, bits);
	scheme.modSwitchOneAndEqual(c);
}

Cipher* SchemeAlgo::fftInvLazy(Cipher*& ciphers, const long& size) {
	return fftRaw(ciphers, size, false);
}

Cipher SchemeAlgo::slotsum(Cipher& cipher, const long& slots) {
	long logslots = log2(slots);
	Cipher res = cipher;
	for (long i = 0; i < logslots; ++i) {
		Cipher rot = scheme.rotate2(cipher, i);
		scheme.addAndEqual(res, rot);
	}
	return res;
}

void SchemeAlgo::slotsumAndEqual(Cipher& cipher, const long& slots) {
	long logslots = log2(slots);
	for (long i = 0; i < logslots; ++i) {
		Cipher rot = scheme.rotate2(cipher, i);
		scheme.addAndEqual(cipher, rot);
	}
}
