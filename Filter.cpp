#include "GBlendMode.h"
#include "GColor.h"
#include "GPixel.h"
#include "GFilter.h"
#include <memory>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

class MyFilter: public GFilter {
public:

	MyFilter(GBlendMode mode, const GColor& src): fMode(mode), fColorSrc(src) {
		float fA = src.fA;
	    float fR = src.fR;
	    float fG = src.fG;
	    float fB = src.fB;
	    int iA = GRoundToInt(GPinToUnit(fA) * 255);
	    int iR = GRoundToInt(fR * fA * 255);
	    int iG = GRoundToInt(fG * fA * 255);
	    int iB = GRoundToInt(fB * fA * 255);
	    fSPixel = GPixel_PackARGB(iA, iR, iG, iB);
	}

	bool preservesAlpha() {
		//Check the blendmode to see how it determines its result alpha
		//Return true on the modes where the result alpha is equal to the destination alpha
		switch(fMode) {
			case GBlendMode::kClear: //!< [0, 0]
				return false;
		    case GBlendMode::kSrc: //!< [Sa, Sc]
		    	return false;
		    case GBlendMode::kDst: //!< [Da, Dc]
		    	return true;
		    case GBlendMode::kSrcOver: //!< [Sa + Da * (1 - Sa), Sc + Dc * (1 - Sa)]
		    	return false;
		    case GBlendMode::kDstOver: //!< [Da + Sa * (1 - Da), Dc + Sc * (1 - Da)]
		    	return false;
		    case GBlendMode::kSrcIn: //!< [Sa * Da, Sc * Da]
		    	return false;
		    case GBlendMode::kDstIn: //!< [Da * Sa, Dc * Sa]
		    	return false;
		    case GBlendMode::kSrcOut: //!< [Sa * (1 - Da), Sc * (1 - Da)]
		    	return false;
		    case GBlendMode::kDstOut: //!< [Da * (1 - Sa), Dc * (1 - Sa)]
		    	return false;
		    case GBlendMode::kSrcATop: //!< [Da, Sc * Da + Dc * (1 - Sa)]
		    	return true;
		    case GBlendMode::kDstATop: //!< [Sa, Dc * Sa + Sc * (1 - Da)]
		    	return false;
		    case GBlendMode::kXor: //!< [Sa + Da - 2 * Sa * Da, Sc * (1 - Da) + Dc * (1 - Sa)]
		    	return false;
		}
		return false;
	}

	void filter(GPixel output[], const GPixel input[], int count) {
		for (int i = 0; i < count; i++) {
			output[i] = generateRPixel(fMode, fSPixel, input[i]);
		}
	}

	GPixel generateRPixel(GBlendMode mode, GPixel sPixel, GPixel dPixel) {
 		GPixel rPixel;
	    int rA;
	    int rR;
	    int rG;
	    int rB;
 		switch (mode) {
			case GBlendMode::kClear: //!< [0, 0]
				rPixel = GPixel_PackARGB(0, 0, 0, 0);
				break;
		    case GBlendMode::kSrc: //!< [Sa, Sc]
		    	rA = GPixel_GetA(sPixel);
		    	rR = GPixel_GetR(sPixel);
		    	rG = GPixel_GetG(sPixel);
		    	rB = GPixel_GetB(sPixel);
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kDst: //!< [Da, Dc]
		    	rA = GPixel_GetA(dPixel);
		    	rR = GPixel_GetR(dPixel);
		    	rG = GPixel_GetG(dPixel);
		    	rB = GPixel_GetB(dPixel);
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kSrcOver: //!< [Sa + Da * (1 - Sa), Sc + Dc * (1 - Sa)]
		    	rA = GPixel_GetA(sPixel) + div255((255 - GPixel_GetA(sPixel)) * GPixel_GetA(dPixel));
    			rR = GPixel_GetR(sPixel) + div255((255 - GPixel_GetA(sPixel)) * GPixel_GetR(dPixel));
    			rG = GPixel_GetG(sPixel) + div255((255 - GPixel_GetA(sPixel)) * GPixel_GetG(dPixel));
    			rB = GPixel_GetB(sPixel) + div255((255 - GPixel_GetA(sPixel)) * GPixel_GetB(dPixel));
    			rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kDstOver: //!< [Da + Sa * (1 - Da), Dc + Sc * (1 - Da)]
			    rA = GPixel_GetA(dPixel) + div255((255 - GPixel_GetA(dPixel)) * GPixel_GetA(sPixel));
    			rR = GPixel_GetR(dPixel) + div255((255 - GPixel_GetA(dPixel)) * GPixel_GetR(sPixel));
    			rG = GPixel_GetG(dPixel) + div255((255 - GPixel_GetA(dPixel)) * GPixel_GetG(sPixel));
    			rB = GPixel_GetB(dPixel) + div255((255 - GPixel_GetA(dPixel)) * GPixel_GetB(sPixel));
    			rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kSrcIn: //!< [Sa * Da, Sc * Da]
		    	rA = div255(GPixel_GetA(sPixel) * GPixel_GetA(dPixel));
		    	rR = div255(GPixel_GetR(sPixel) * GPixel_GetA(dPixel));
		    	rG = div255(GPixel_GetG(sPixel) * GPixel_GetA(dPixel));
		    	rB = div255(GPixel_GetB(sPixel) * GPixel_GetA(dPixel));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kDstIn: //!< [Da * Sa, Dc * Sa]
		    	rA = div255(GPixel_GetA(dPixel) * GPixel_GetA(sPixel));
		    	rR = div255(GPixel_GetR(dPixel) * GPixel_GetA(sPixel));
		    	rG = div255(GPixel_GetG(dPixel) * GPixel_GetA(sPixel));
		    	rB = div255(GPixel_GetB(dPixel) * GPixel_GetA(sPixel));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    //Seems to have no impact 7, 8, 9 , 10
		    case GBlendMode::kSrcOut: //!< [Sa * (1 - Da), Sc * (1 - Da)]
		    	rA = div255(GPixel_GetA(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rR = div255(GPixel_GetR(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rG = div255(GPixel_GetG(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rB = div255(GPixel_GetB(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kDstOut: //!< [Da * (1 - Sa), Dc * (1 - Sa)]
		    	rA = div255(GPixel_GetA(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rR = div255(GPixel_GetR(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rG = div255(GPixel_GetG(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rB = div255(GPixel_GetB(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kSrcATop: //!< [Da, Sc * Da + Dc * (1 - Sa)]
		    	rA = GPixel_GetA(dPixel);
		    	rR = div255(GPixel_GetR(sPixel) * GPixel_GetA(dPixel) + GPixel_GetR(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rG = div255(GPixel_GetG(sPixel) * GPixel_GetA(dPixel) + GPixel_GetG(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rB = div255(GPixel_GetB(sPixel) * GPixel_GetA(dPixel) + GPixel_GetB(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kDstATop: //!< [Sa, Dc * Sa + Sc * (1 - Da)]
		    	rA = GPixel_GetA(sPixel);
		    	rR = div255(GPixel_GetR(dPixel) * GPixel_GetA(sPixel) + GPixel_GetR(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rG = div255(GPixel_GetG(dPixel) * GPixel_GetA(sPixel) + GPixel_GetG(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rB = div255(GPixel_GetB(dPixel) * GPixel_GetA(sPixel) + GPixel_GetB(sPixel) * (255 - GPixel_GetA(dPixel)));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		    case GBlendMode::kXor: //!< [Sa + Da - 2 * Sa * Da, Sc * (1 - Da) + Dc * (1 - Sa)]
		    	rA = GPixel_GetA(sPixel) + GPixel_GetA(dPixel) - div255(2 * GPixel_GetA(sPixel) * GPixel_GetA(dPixel));
		    	rR = div255(GPixel_GetR(sPixel) * (255 - GPixel_GetA(dPixel)) + GPixel_GetR(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rG = div255(GPixel_GetG(sPixel) * (255 - GPixel_GetA(dPixel)) + GPixel_GetG(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rB = div255(GPixel_GetB(sPixel) * (255 - GPixel_GetA(dPixel)) + GPixel_GetB(dPixel) * (255 - GPixel_GetA(sPixel)));
		    	rPixel = GPixel_PackARGB(rA, rR, rG, rB);
		    	break;
		}
		return rPixel;
 	}

 	unsigned div255(unsigned x) {
	    x += 128;
    	return x + (x >> 8) >> 8;
	}

private:
	GBlendMode fMode;
	const GColor& fColorSrc;
	GPixel fSPixel;
};

std::unique_ptr<GFilter> GCreateBlendFilter(GBlendMode mode, const GColor& src) {
	return std::unique_ptr<GFilter>(new MyFilter(mode, src));
}
