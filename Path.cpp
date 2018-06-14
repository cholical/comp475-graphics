#include "GPath.h"
#include "GPoint.h"
#include "GMatrix.h"
#include "GRect.h"
#include "GMath.h"
#include <vector>
#include <iostream>
#include <cmath>

GPath& GPath::addRect(const GRect& rect, GPath::Direction direction) {
	if (direction == GPath::Direction::kCW_Direction) {
		this->moveTo(rect.left(), rect.top());
		this->lineTo(rect.right(), rect.top());
		this->lineTo(rect.right(), rect.bottom());
		this->lineTo(rect.left(), rect.bottom());
	} else {
		this->moveTo(rect.left(), rect.top());
		this->lineTo(rect.left(), rect.bottom());
		this->lineTo(rect.right(), rect.bottom());
		this->lineTo(rect.right(), rect.top());
	}
	return *this;
}

GPath& GPath::addPolygon(const GPoint pts[], int count) {
	this->moveTo(pts[0].fX, pts[0].fY);
	for (int i = 1; i < count; i++) {
		this->lineTo(pts[i].fX, pts[i].fY);
	}
	return *this;
}

GPath& GPath::addCircle(GPoint center, float radius, GPath::Direction direction) {
	GMatrix mapper;
	mapper.set6(radius, 0.0f, center.fX, 0.0f, radius, center.fY);
	GPoint a = mapper.mapXY(1.0f, 0.0f);
	GPoint b = mapper.mapXY(1.0f, -1.0f * tanf(M_PI/8.0f));
	GPoint c = mapper.mapXY(sqrt(2.0f)/2.0f, -1.0f * sqrt(2.0f)/2.0f);
	GPoint d = mapper.mapXY(tanf(M_PI/8.0f), -1.0f);
	GPoint e = mapper.mapXY(0.0f, -1.0f);
	GPoint f = mapper.mapXY(-1.0f * tanf(M_PI/8.0f), -1.0f);
	GPoint g = mapper.mapXY(-1.0f * sqrt(2.0f)/2.0f, -1.0f * sqrt(2.0f)/2.0f);
	GPoint h = mapper.mapXY(-1.0f, -1.0f * tanf(M_PI/8.0f));
	GPoint i = mapper.mapXY(-1.0f, 0.0f);
	GPoint j = mapper.mapXY(-1.0f, tanf(M_PI/8.0f));
	GPoint k = mapper.mapXY(-1.0f * sqrt(2.0f)/2.0f, sqrt(2.0f)/2.0f);
	GPoint l = mapper.mapXY(-1.0f * tanf(M_PI/8.0f), 1.0f);
	GPoint m = mapper.mapXY(0.0f, 1.0f);
	GPoint n = mapper.mapXY(tanf(M_PI/8.0f), 1.0f);
	GPoint o = mapper.mapXY(sqrt(2.0f)/2.0f, sqrt(2.0f)/2.0f);
	GPoint p = mapper.mapXY(1, tanf(M_PI/8.0f));
	this->moveTo(a);
	if (direction == GPath::Direction::kCW_Direction) {
		this->quadTo(p, o);
		this->quadTo(n, m);
		this->quadTo(l, k);
		this->quadTo(j, i);
		this->quadTo(h, g);
		this->quadTo(f, e);
		this->quadTo(d, c);
		this->quadTo(b, a);
	} else {
		this->quadTo(b, c);
		this->quadTo(d, e);
		this->quadTo(f, g);
		this->quadTo(h, i);
		this->quadTo(j, k);
		this->quadTo(l, m);
		this->quadTo(n, o);
		this->quadTo(p, a);
	}
	return *this;
}

void GPath::ChopQuadAt(const GPoint src[3], GPoint dst[5], float t) {
	float x;
	float y;
	GPoint p1;
	GPoint p2;
	GPoint p3;

	dst[0] = src[0];
	x = (1.0f - t) * src[0].fX + t * src[1].fX;
	y = (1.0f - t) * src[0].fY + t * src[1].fY;
	p1.set(x, y);
	dst[1] = p1;

	x = (1.0f - t) * src[1].fX + t * src[2].fX;
	y = (1.0f - t) * src[1].fY + t * src[2].fY;
	p3.set(x, y);
	dst[3] = p3;

	x = (1.0f - t) * p1.fX + t * p3.fX;
	y = (1.0f - t) * p1.fY + t * p3.fY;
	p2.set(x, y);
	dst[2] = p2;

	dst[4] = src[2];
}

void GPath::ChopCubicAt(const GPoint src[4], GPoint dst[7], float t) {
	GPoint p1;
	GPoint p2;
	GPoint p3;
	GPoint p4;
	GPoint p5;
	GPoint mid;

	dst[0] = src[0];

	p1.fX = (1.0f - t) * src[0].fX + t * src[1].fX;
	p1.fY = (1.0f - t) * src[0].fY + t * src[1].fY;
	dst[1] = p1; 

	p5.fX = (1.0f - t) * src[2].fX + t * src[3].fX;
	p5.fY = (1.0f - t) * src[2].fY + t * src[3].fY;
	dst[5] = p5;

	mid.fX = (1.0f - t) * src[1].fX + t * src[2].fX;
	mid.fY = (1.0f - t) * src[1].fY + t * src[2].fY;

	//p1 + mid = p2
	p2.fX = (1.0f - t) * p1.fX + t * mid.fX;
	p2.fY = (1.0f - t) * p1.fY + t * mid.fY;
	dst[2] = p2;

	//mid + p5 = p4
	p4.fX = (1.0f - t) * mid.fX + t * p5.fX;
	p4.fY = (1.0f - t) * mid.fY + t * p5.fY;
	dst[4] = p4;

	//p2 + p4 = p3
	p3.fX = (1.0f - t) * p2.fX + t * p4.fX;
	p3.fY = (1.0f - t) * p2.fY + t * p4.fY;
	dst[3] = p3;

	dst[6] = src[3];
}

GRect GPath::bounds() const {
	if (this->fPts.size() == 0 || this->fPts.size() == 1) {
		return GRect::MakeLTRB(0.0f, 0.0f, 0.0f, 0.0f);
	} 
	float l = std::numeric_limits<float>::infinity();
	float t = std::numeric_limits<double>::infinity();
	float r = -1.0f * std::numeric_limits<double>::infinity();
	float b = -1.0f * std::numeric_limits<double>::infinity();
	for (GPoint p : this->fPts) {
		l = std::min(l, p.fX);
		t = std::min(t, p.fY);
		r = std::max(r, p.fX);
		b = std::max(b, p.fY);
	}
	return GRect::MakeLTRB(l, t, r, b);
}

void GPath::transform(const GMatrix& m) {
	for (int i = 0; i < this->fPts.size(); i++) {
		m.mapPoints(&this->fPts[i], 1);
	}
}