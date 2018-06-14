#include "GMatrix.h"
#include "GColor.h"
#include "GMath.h"
#include "GPoint.h"
#include "GRect.h"
#include <iostream>
#include <cmath>

void GMatrix::setIdentity() {
	float A = 1;
	float B = 0;
	float C = 0;
	float D = 0;
	float E = 1;
	float F = 0;
	set6(A, B, C, D, E, F);
}

void GMatrix::setTranslate(float tx, float ty) {
	float A = 1;
	float B = 0;
	float C = tx;
	float D = 0;
	float E = 1;
	float F = ty;
	set6(A, B, C, D, E, F);
}

void GMatrix::setScale(float sx, float sy) {
	float A = sx;
	float B = 0;
	float C = 0;
	float D = 0;
	float E = sy;
	float F = 0;
	set6(A, B, C, D, E, F);
}

void GMatrix::setRotate(float radians) {float A = cosf(radians);
	float B = -1.0f * sinf(radians);
	float C = 0;
	float D = sinf(radians);
	float E = cosf(radians);
	float F = 0;
	set6(A, B, C, D, E, F);
}

void GMatrix::setConcat(const GMatrix& secundo, const GMatrix& primo) {
	float A = secundo[0];
	float B = secundo[1];
	float C = secundo[2];
	float D = secundo[3];
	float E = secundo[4];
	float F = secundo[5];
	float a = primo[0];
	float b = primo[1];
	float c = primo[2];
	float d = primo[3];
	float e = primo[4];
	float f = primo[5];

	float first = (A * a) + (B * d);
	float second = (A * b) + (B * e);
	float third = (A * c) + (B * f) + C;
	float fourth = (D * a) + (E * d);
	float fifth = (D * b) + (E * e);
	float sixth = (D * c) + (E * f) + F;

	set6(first, second, third, fourth, fifth, sixth);
}

bool GMatrix::invert(GMatrix* inverse) const {
	float det = (fMat[SX] * fMat[SY]) - (fMat[KX] * fMat[KY]);
	if (det == 0) {
		return false;
	} else {
		inverse->set6(fMat[SY] / det, -1.0f * fMat[KX] / det, ((fMat[KX] * fMat[TY]) - (fMat[SY] * fMat[TX])) / det, -1.0f * fMat[KY] / det, fMat[SX] / det, ((fMat[KY] * fMat[TX]) - (fMat[SX] * fMat[TY])) / det);
		return true;
	}
}

void GMatrix::mapPoints(GPoint dst[], const GPoint src[], int count) const {
	for (int i = 0; i < count; i++) {
		dst[i] = GPoint::Make(fMat[SX] * src[i].fX + fMat[KX] * src[i].fY + fMat[TX], fMat[KY] * src[i].fX + fMat[SY] * src[i].fY + fMat[TY]);
	}
}