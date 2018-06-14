#include "GShader.h"
#include "GBitmap.h"
#include "GMatrix.h"
#include "GColor.h"
#include "GPoint.h"
#include "GMath.h"
#include "GPixel.h"
#include <tgmath.h>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

class MyShader: public GShader {
public:

	MyShader(const GBitmap& bitmap, const GMatrix& localInv,  GShader::TileMode tileMode) : fBitmap(bitmap), fLocalInv(localInv), fTileMode(tileMode) {
		localInv.invert(&this->fLocalMatrix);
		// std::cout << "Constructor: \n";
		// switch (this->fTileMode) {
		// 	case TileMode::kClamp:
		// 		std::cout << "kClamp\n";
		// 		break;
		// 	case TileMode::kRepeat:
		// 		std::cout << "kRepeat\n";
		// 		break;
		// 	case TileMode::kMirror:
		// 		std::cout << "kMirror\n";
		// 		break;
		// 	default:
		// 		std::cout << "Could not determine tileMode\n";
		// }
	}

	bool isOpaque() {
		return this->fBitmap.isOpaque();
	}

	bool setContext(const GMatrix& ctm) {
        GMatrix tmp;
        tmp.setConcat(ctm, this->fLocalMatrix);
        if (tmp.invert(&this->fInverse)) {
   //      	std::cout << "Prescale this->fInverse\n";
   //      	std::cout << "[" << this->fInverse[0] << ", " <<  this->fInverse[1] << ", " << this->fInverse[2] << "]\n";
			// std::cout << "[" << this->fInverse[3] << ", " <<  this->fInverse[4] << ", " << this->fInverse[5] << "]\n";
			// std::cout << "Width and Height values: (" << this->fBitmap.width() << ", " << this->fBitmap.height() << ")\n";
			// std::cout << "Scale values: (" << 1.0f/((float) this->fBitmap.width()) << ", " << 1.0f/((float) this->fBitmap.height()) << ")\n";
        	this->fInverse.postScale(1.0f/((float) this->fBitmap.width()), 1.0f/((float) this->fBitmap.height()));
   //      	std::cout << "Prescale this->fInverse\n";
   //      	std::cout << "[" << this->fInverse[0] << ", " <<  this->fInverse[1] << ", " << this->fInverse[2] << "]\n";
			// std::cout << "[" << this->fInverse[3] << ", " <<  this->fInverse[4] << ", " << this->fInverse[5] << "]\n";
        	return true;
        } else {
        	return false;
        }
        return tmp.invert(&this->fInverse);
    }

	void shadeRow(int x, int y, int count, GPixel row[]) {
		GPoint local = this->fInverse.mapXY(x + 0.5, y + 0.5);
		GPixel sPixel;
		GPixel* address;
		// std::cout << "Starting local point: (" << local.fX << ", " << local.fY << ")\n";
		for (int i = 0; i < count; i++) {
			float fX = local.fX;
			float fY = local.fY;
			// std::cout << "Pretiling (fX, fY): (" << fX << ", " << fY << ")\n";
			switch (this->fTileMode) {
				case TileMode::kClamp:
					fX = std::min(std::max(fX, 0.0f), 0.99999f);
					fY = std::min(std::max(fY, 0.0f), 0.99999f);
					// local.fX = std::min(std::max(local.fX, 0.0f), (float) this->fBitmap.width() - 1);
					// local.fY = std::min(std::max(local.fY, 0.0f), (float) this->fBitmap.height() - 1);
					break;
	        	case TileMode::kRepeat:
	        		fX = fX - GFloorToInt(fX);
	        		fY = fY - GFloorToInt(fY);
	        		break;
	        	case TileMode::kMirror:
	        		fX = fX * 0.5;
	        		fX = fX - GFloorToInt(fX);
	        		if (fX > 0.5) {
	        			fX = 1 - fX;
	        		}
	        		fX = fX * 2;

	        		fY = fY * 0.5;
	        		fY = fY - GFloorToInt(fY);
	        		if (fY > 0.5) {
	        			fY = 1 - fY;
	        		}
	        		fY = fY * 2;
	        		break;
			}
			// std::cout << "After tiling (fX, fY): " << fX << ", " << fY << ")\n";
			fX *= this->fBitmap.width();
			fY *= this->fBitmap.height();
			int sX = (int) fX;
			int sY = (int) fY;
			// std::cout << "GetAddress (sX, sY): " << sX << ", " << sY << ")\n";
			address = this->fBitmap.getAddr(sX, sY);
			memcpy(&sPixel, address, sizeof(GPixel));
			row[i] = sPixel;
			local.fX += fInverse[GMatrix::SX];
			local.fY += fInverse[GMatrix::KY];
		}
	}

private:
	const GBitmap fBitmap;
	GMatrix fLocalInv;
	GMatrix fInverse;
	GMatrix fLocalMatrix;
	TileMode fTileMode;
};

std::unique_ptr<GShader> GCreateBitmapShader(const GBitmap& bitmap, const GMatrix& localInv,  GShader::TileMode tileMode) {
	if (!bitmap.pixels()) {
		return nullptr;
	}
	GMatrix testMatrix();
	// if (!localInv.invert(testMatrix)) {
	// 	return nullptr;
	// }
	return std::unique_ptr<GShader>(new MyShader(bitmap, localInv, tileMode));
}