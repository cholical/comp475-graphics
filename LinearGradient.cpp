#include "GColor.h"
#include "GShader.h"
#include "GMatrix.h"
#include "GPoint.h"
#include "GMath.h"
#include "GPixel.h"
#include <tgmath.h>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

class MyLinearGradient: public GShader {
public:

	MyLinearGradient(GPoint p0, GPoint p1, const GColor colors[], int count, GShader::TileMode tileMode) : p0(p0), p1(p1), fCount(count), fTileMode(tileMode) {
		this->fColors = new GColor[count];
		memcpy(this->fColors, colors, count * sizeof(GColor));
		if (this->fCount > 1) {
			this->fLocalMatrix.set6(p1.fX - p0.fX, (p1.fY - p0.fY) * -1.0f, p0.fX, p1.fY - p0.fY, p1.fX - p0.fX, p0.fY);
			this->fD = pow(pow(p1.fY - p0.fY, 2) + pow(p1.fX - p0.fX, 2), 0.5);
			this->fInterval = 1.0f / (this->fCount - 1);
			// std::cout << "this->fCount - 1: " << this->fCount << "\n";
			// std::cout << "1.0f / (this->fCount - 1): " << 1.0f / (this->fCount - 1) << "\n";
			// std::cout << "this->fInterval: " << this->fInterval << "\n";
		}
	}

	bool isOpaque() {
		bool opacity = true;
		for (int i = 0; i < fCount; i++) {
			if (fColors[i].fA != 1.0f) {
				opacity = false;
				break;
			}
		}
		return opacity;
	}

	bool setContext(const GMatrix& ctm) {
		if (this->fCount > 1) {
			GMatrix tmp;
	        tmp.setConcat(ctm, this->fLocalMatrix);
	        return tmp.invert(&this->fInverse);
		} else {
			return true;
		}
	}

	void shadeRow(int x, int y, int count, GPixel row[]) {
		if (this->fCount > 1) {
			for (int i = 0; i < count; i++) {
				GPoint local = this->fInverse.mapXY(x + i + 0.5, y + 0.5);
				switch (this->fTileMode) {
					case TileMode::kClamp:
						local.fX = std::min(std::max(local.fX, 0.0f), 1.0f);
						break;
		        	case TileMode::kRepeat:
		        		local.fX = local.fX - GFloorToInt(local.fX);
		        		break;
		        	case TileMode::kMirror:
		        		local.fX = local.fX * 0.5;
		        		local.fX = local.fX - GFloorToInt(local.fX);
		        		if (local.fX > 0.5) {
		        			local.fX = 1 - local.fX;
		        		}
		        		local.fX = local.fX * 2;
		        		break;
				}
				int colorIndex0 = GFloorToInt(local.fX / this->fInterval);
				int colorIndex1 = colorIndex0 + 1;

				float percentage = (local.fX - (colorIndex0 * this->fInterval))/this->fInterval;
				GColor color0 = this->fColors[colorIndex0];
				GColor color1 = this->fColors[colorIndex1];
				int iA, iR, iG, iB;

			    iA = GRoundToInt(GPinToUnit(color0.fA) * 255);
			    iR = GRoundToInt(GPinToUnit(color0.fR) * GPinToUnit(color0.fA) * 255);
			    iG = GRoundToInt(GPinToUnit(color0.fG) * GPinToUnit(color0.fA) * 255);
			    iB = GRoundToInt(GPinToUnit(color0.fB) * GPinToUnit(color0.fA) * 255);
			    GPixel c0 = GPixel_PackARGB(iA, iR, iG, iB);

			    iA = GRoundToInt(GPinToUnit(color1.fA) * 255);
			    iR = GRoundToInt(GPinToUnit(color1.fR) * GPinToUnit(color1.fA) * 255);
			    iG = GRoundToInt(GPinToUnit(color1.fG) * GPinToUnit(color1.fA) * 255);
			    iB = GRoundToInt(GPinToUnit(color1.fB) * GPinToUnit(color1.fA) * 255);
				GPixel c1 = GPixel_PackARGB(iA, iR, iG, iB);

			    iA = GPixel_GetA(c0) + GRoundToInt(percentage * (GPixel_GetA(c1) - GPixel_GetA(c0)));
			    iR = GPixel_GetR(c0) + GRoundToInt(percentage * (GPixel_GetR(c1) - GPixel_GetR(c0)));
			    iG = GPixel_GetG(c0) + GRoundToInt(percentage * (GPixel_GetG(c1) - GPixel_GetG(c0)));
			    iB = GPixel_GetB(c0) + GRoundToInt(percentage * (GPixel_GetB(c1) - GPixel_GetB(c0)));

			    row[i] = GPixel_PackARGB(iA, iR, iG, iB);
			}
		} else {
			int iA = GRoundToInt(GPinToUnit(this->fColors[0].fA) * 255);
		    int iR = GRoundToInt(this->fColors[0].fR * this->fColors[0].fA * 255);
		    int iG = GRoundToInt(this->fColors[0].fG * this->fColors[0].fA * 255);
		    int iB = GRoundToInt(this->fColors[0].fB * this->fColors[0].fA * 255);
			for (int i = 0; i < count; i++) {
				row[i] = GPixel_PackARGB(iA, iR, iG, iB);
			}
		}
	}

private:
	GPoint p0;
	GPoint p1;
	GColor* fColors;
	int fCount;
	GMatrix fInverse;
	GMatrix fLocalMatrix;
	float fD;
	float fInterval;
	TileMode fTileMode;
};

std::unique_ptr<GShader> GCreateLinearGradient(GPoint p0, GPoint p1, const GColor colors[], int count, GShader::TileMode tileMode) {
	if (count < 1) {
		return nullptr;
	}
	return std::unique_ptr<GShader>(new MyLinearGradient(p0, p1, colors, count, tileMode));
}