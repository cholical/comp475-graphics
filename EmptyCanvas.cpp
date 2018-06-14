#include "GCanvas.h"
#include "GPath.h"
#include "GShader.h"
#include "GRect.h"
#include "GColor.h"
#include "GPixel.h"
#include "GBitmap.h"
#include "GMath.h"
#include "GPaint.h"
#include "GBlendMode.h"
#include "GFilter.h"
#include "GPoint.h"
#include <iostream>
#include <stack>
#include <algorithm>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

struct Edge {
public:
	int minY;
	int maxY;
	float dxdy;
	float w;
	float currX;
	int winding;

    bool init(GPoint p0, GPoint p1) {
    	winding = 1;
    	if (p0.fY > p1.fY) {
	        std::swap(p0, p1);
	        winding = -1 * winding;
	    }
        int y0 = GRoundToInt(p0.fY);
        int y1 = GRoundToInt(p1.fY);
        if (y0 == y1) {
            return false;
        }
        minY = std::min(y0, y1);
        maxY = std::max(y0, y1);
        dxdy = (p1.fX - p0.fX) / (p1.fY - p0.fY);
        w = dxdy * (GRoundToInt(p0.fY) - p0.fY + 0.5);
        currX = p0.fX + w;
        return true;
    }

    bool containsY(int y) {
    	// std::cout << "ContainsY called with y = " << y << "\n";
    	// std::cout << "(minY, maxY): (" << minY << ", " << maxY << ")\n";
    	if ((y >= minY) && (y < maxY)) {
    		return true;
    	} else {
    		return false;
    	}
    }

    void incrementCurrX() {
    	currX += dxdy;
    }
};

struct Layer {
public:
	GBitmap fBitmap;
	GPaint fPaint;
	const GRect* fBounds;
	Layer(GBitmap bitmap, const GRect* bounds, const GPaint& paint) {
		fBitmap = bitmap;
		fPaint = paint;
		fBounds = bounds;
	}
};

struct QuadCurve {
public:
	GPoint p0;
	GPoint controlPoint;
	GPoint p1;

	QuadCurve(GPoint a, GPoint b, GPoint c) {
		p0.set(a.fX, a.fY);
		controlPoint.set(b.fX, b.fY);
		p1.set(c.fX, c.fY);
	}

	float getX(float t) {
		return p0.fX * pow(1.0f - t, 2) + controlPoint.fX * 2.0f * (1.0f - t) * t + p1.fX * pow(t, 2);
	}

	float getY(float t) {
		return p0.fY * pow(1.0f - t, 2) + controlPoint.fY * 2.0f * (1.0f - t) * t + p1.fY * pow(t, 2);
	}
};

struct CubicCurve {
public:
	GPoint p0;
	GPoint controlPoint0;
	GPoint controlPoint1;
	GPoint p1;

	CubicCurve(GPoint a, GPoint b, GPoint c, GPoint d) {
		p0.set(a.fX, a.fY);
		controlPoint0.set(b.fX, b.fY);
		controlPoint1.set(c.fX, c.fY);
		p1.set(d.fX, d.fY);
	}

	float getX(float t) {
		return p0.fX * pow(1.0f - t, 3) + controlPoint0.fX * 3.0f * pow(1.0f - t, 2) * t + controlPoint1.fX * 3.0f * (1 - t) * pow(t, 2) + p1.fX * pow(t, 3);
	}

	float getY(float t) {
		return p0.fY * pow(1.0f - t, 3) + controlPoint0.fY * 3.0f * pow(1.0f - t, 2) * t + controlPoint1.fY * 3.0f * (1 - t) * pow(t, 2) + p1.fY * pow(t, 3);
	}
};

class EmptyCanvas: public GCanvas {
public: 

	EmptyCanvas(const GBitmap& device) : fDevice(device) {
		this->ctm = GMatrix();
		this->currentDevice = &this->fDevice;
	}

	void drawPaint(const GPaint& paint) override {
		//Get paint shader and set CTM
		GShader* shader = paint.getShader();
		if (shader) {
			if (!shader->setContext(this->ctm)) {
				return;
			}
		}
		
		//Get paint blend mode
		GBlendMode mode = paint.getBlendMode();

		//Get paint color
		GColor color = paint.getColor().pinToUnit();
	    int iA = GRoundToInt(GPinToUnit(color.fA) * 255);
	    int iR = GRoundToInt(color.fR * color.fA * 255);
	    int iG = GRoundToInt(color.fG * color.fA * 255);
	    int iB = GRoundToInt(color.fB * color.fA * 255);

	    GPixel dPixel;
	    GPixel rPixel;
	    GPixel sPixel = GPixel_PackARGB(iA, iR, iG, iB);

	    //Get paint filter
	    GFilter* fl = paint.getFilter();
	    if (fl) {
	    	fl->filter(&sPixel, &sPixel, 1);
	    }
	    
	    GPixel pixelStorage[this->currentDevice->width()];

	    //Fill in the bitmap
	    for (int y = 0; y < this->currentDevice->height(); y++) {
	    	if (shader) {
	    		//If shader is present, retrieve the shader pixels
	    		shader->shadeRow(0, y, this->currentDevice->width(), pixelStorage);
	    		//If the filter is present, filter each shader pixel
		    	if (fl) {
		    		fl->filter(pixelStorage, pixelStorage, this->currentDevice->width());
		    	}
	    	}
	    	int pixelStorageIndex = 0;
	    	//Walk through each of the x, y coordinates
	    	for (int x = 0; x < this->currentDevice->width(); x++) {
	    		GPixel* address = this->currentDevice->getAddr(x, y);
	    		memcpy(&dPixel, address, sizeof(GPixel));
	    		if (shader) {
	    			sPixel = pixelStorage[pixelStorageIndex];
	    			pixelStorageIndex++;
	    		}
	    		rPixel = generateRPixel(mode, sPixel, dPixel);
	    		memcpy(address, &rPixel, sizeof(GPixel));
	    	}
	    }
	}

	void drawRect(const GRect& rect, const GPaint& paint) override {
		//Convert rectange bounds into polygon and use the draw convex polygon formula
		GPoint points[4];
		points[0] = GPoint::Make(rect.fLeft, rect.fTop);
		points[1] = GPoint::Make(rect.fRight, rect.fTop);
		points[2] = GPoint::Make(rect.fRight, rect.fBottom);
		points[3] = GPoint::Make(rect.fLeft, rect.fBottom);

		this->drawConvexPolygon(points, 4, paint);
 	}

 	void drawConvexPolygon(const GPoint points[], int count, const GPaint& paint) override {
 		//Array of transformed polygon points
 		GPoint transformedPoints[count];
 		this->ctm.mapPoints(transformedPoints, points, count);

 		//Get paint shader and set CTM
		GShader* shader = paint.getShader();
		if (shader) {
			if (!shader->setContext(this->ctm)) {
				return;
			}
		}
		
		//Get paint blend mode
		GBlendMode mode = paint.getBlendMode();

		//Get paint color
		GColor color = paint.getColor().pinToUnit();
	    int iA = GRoundToInt(GPinToUnit(color.fA) * 255);
	    int iR = GRoundToInt(color.fR * color.fA * 255);
	    int iG = GRoundToInt(color.fG * color.fA * 255);
	    int iB = GRoundToInt(color.fB * color.fA * 255);

	    GPixel dPixel;
	    GPixel rPixel;
	    GPixel sPixel = GPixel_PackARGB(iA, iR, iG, iB);

	    //Get paint filter
	    GFilter* fl = paint.getFilter();
	    if (fl) {
	    	fl->filter(&sPixel, &sPixel, 1);
	    }

	    //This is the rectangle to clip with
 		GRect bounds = GRect::MakeXYWH(0.0f, 0.0f, this->currentDevice->width(), this->currentDevice->height());
 		Edge storage[1000];
 		Edge* edge = storage;
		GPoint p0;
		GPoint p1;
 		for (int i = 0; i < count; i++) {
 			if (i + 1 == count) {
 				p0 = transformedPoints[i];
				p1 = transformedPoints[0];
				edge = clip_line(bounds, p0, p1, edge);
 			} else {
 				p0 = transformedPoints[i];
				p1 = transformedPoints[i + 1];
 				edge = clip_line(bounds, p0, p1, edge);
 			}
 		}
 		int edgeCount = edge - storage;
 		Edge tmp;

		for (int i = 0; i < edgeCount; i++) {
 			for (int j = i + 1; j < edgeCount; j++) {
 				if (storage[i].minY > storage[j].minY) {
 					tmp = storage[i];
 					storage[i] = storage[j];
 					storage[j] = tmp;
 				}
 			}
 		}

 		//Stable sort storage by maxY
 		for (int i = 0; i < edgeCount; i++) {
 			for (int j = i + 1; j < edgeCount; j++) {
 				if (storage[i].minY == storage[j].minY && storage[i].maxY > storage[j].maxY) {
 					tmp = storage[i];
 					storage[i] = storage[j];
 					storage[j] = tmp;
 				}
 			}
 		}

 		//After sorting, storage should now be sorted first by minY and then by maxY

 		int minY = GRoundToInt(bounds.bottom());
 		int maxY = GRoundToInt(bounds.top());

 		for (int i = 0; i < edgeCount; i++) {
 			minY = std::max(GRoundToInt(bounds.top()), std::min(storage[i].minY, minY));
 			maxY = std::min(GRoundToInt(bounds.bottom()), std::max(storage[i].maxY, maxY));
 		}

 		GPixel pixelStorage[GRoundToInt(bounds.width())];

 		int edgeStorageIndex = 0;
 		Edge e0 = storage[edgeStorageIndex];
 		edgeStorageIndex++;
 		Edge e1 = storage[edgeStorageIndex];
 		edgeStorageIndex++;

 		for (int y = minY; y < maxY; y++) {

 			if (!(e0.containsY(y))) {
 				e0 = storage[edgeStorageIndex];
 				edgeStorageIndex++;
 			}

 			if (!(e1.containsY(y))) {
 				e1 = storage[edgeStorageIndex];
 				edgeStorageIndex++;
 			}

 			int leftX = std::max(GRoundToInt(bounds.left()), std::min(GRoundToInt(bounds.right()), GRoundToInt(std::min(e0.currX, e1.currX))));
 			int rightX = std::max(GRoundToInt(bounds.left()), std::min(GRoundToInt(bounds.right()), GRoundToInt(std::max(e0.currX, e1.currX))));
 			// std::cout << "leftX: " << leftX << "\n";
 			// std::cout << "rightX: " << rightX << "\n";
			if (shader) {
				shader->shadeRow(leftX, y, rightX - leftX, pixelStorage);
		    	if (fl) {
		    		fl->filter(pixelStorage, pixelStorage, rightX - leftX);
		    	}
			}
 			int pixelStorageIndex = 0;
 			for (int x = leftX; x < rightX; x++) {
 				GPixel* address = this->currentDevice->getAddr(x, y);
	    		memcpy(&dPixel, address, sizeof(GPixel));
	    		if (shader) {
    				sPixel = pixelStorage[pixelStorageIndex];
	    			pixelStorageIndex++;
	    		}
	    		rPixel = generateRPixel(mode, sPixel, dPixel);
	    		memcpy(address, &rPixel, sizeof(GPixel));
 			}
 			e0.incrementCurrX();
 			e1.incrementCurrX();
 		}
 	}

 	void drawPath(const GPath& path, const GPaint& paint) {
 		//Get paint shader and set CTM
		GShader* shader = paint.getShader();
		if (shader) {
			if (!shader->setContext(this->ctm)) {
				return;
			}
		}
		
		//Get paint blend mode
		GBlendMode mode = paint.getBlendMode();

		//Get paint color
		GColor color = paint.getColor().pinToUnit();
	    int iA = GRoundToInt(GPinToUnit(color.fA) * 255);
	    int iR = GRoundToInt(color.fR * color.fA * 255);
	    int iG = GRoundToInt(color.fG * color.fA * 255);
	    int iB = GRoundToInt(color.fB * color.fA * 255);

	    GPixel dPixel;
	    GPixel rPixel;
	    GPixel sPixel = GPixel_PackARGB(iA, iR, iG, iB);

	    //Get paint filter
	    GFilter* fl = paint.getFilter();
	    if (fl) {
	    	fl->filter(&sPixel, &sPixel, 1);
	    }

	    GRect bounds = GRect::MakeXYWH(0.0f, 0.0f, this->currentDevice->width(), this->currentDevice->height());
 		Edge storage[1000];
 		Edge* edge = storage;
		GPoint p0;
		GPoint p1;
		GPath::Edger edger(path);
		GPoint pContainer[4];
		Edge tmp;

		GPath::Verb currentVerb;
		while ((currentVerb = edger.next(pContainer)) != GPath::Verb::kDone) {
			int segmentCount;
			float step;
			switch (currentVerb) {
				case GPath::Verb::kLine:
					{
						this->ctm.mapPoints(pContainer, pContainer, 2);
						edge = clip_line(bounds, pContainer[0], pContainer[1], edge);
						break;
					}
					
				case GPath::Verb::kQuad:
					{
						this->ctm.mapPoints(pContainer, pContainer, 3);
						QuadCurve qCurve(pContainer[0], pContainer[1], pContainer[2]);
						//11 points
						segmentCount = 20;
						step = 1.0f / (float) segmentCount;
						GPoint qCoordinates[segmentCount + 1];
						qCoordinates[0] = GPoint::Make(qCurve.getX(0.0f), qCurve.getY(0.0f));
						for (int i = 1; i <= segmentCount; i++) {
							float t = i * step;
							qCoordinates[i] = GPoint::Make(qCurve.getX(t), qCurve.getY(t));
						}
						for (int i = 0; i < segmentCount; i++) {
							edge = clip_line(bounds, qCoordinates[i], qCoordinates[i + 1], edge);
						}
						break;
					}
					
				case GPath::Verb::kCubic:
					{
						this->ctm.mapPoints(pContainer, pContainer, 4);
						CubicCurve cCurve(pContainer[0], pContainer[1], pContainer[2], pContainer[3]);
						//11 points
						segmentCount = 10;
						step = 1.0f / (float) segmentCount;
						GPoint cCoordinates[segmentCount + 1];
						cCoordinates[0] = GPoint::Make(cCurve.getX(0.0f), cCurve.getY(0.0f));
						for (int i = 1; i <= segmentCount; i++) {
							float t = i * step;
							cCoordinates[i] = GPoint::Make(cCurve.getX(t), cCurve.getY(t));
						}
						for (int i = 0; i < segmentCount; i++) {
							edge = clip_line(bounds, cCoordinates[i], cCoordinates[i + 1], edge);
						}
						break;
					}
			}
			
		}

		int edgeCount = edge - storage;

		for (int i = 0; i < edgeCount; i++) {
 			for (int j = i + 1; j < edgeCount; j++) {
 				if (storage[i].minY > storage[j].minY) {
 					tmp = storage[i];
 					storage[i] = storage[j];
 					storage[j] = tmp;
 				}
 			}
 		}

 		//Stable sort storage by maxY
 		for (int i = 0; i < edgeCount; i++) {
 			for (int j = i + 1; j < edgeCount; j++) {
 				if (storage[i].minY == storage[j].minY && storage[i].currX > storage[j].currX) {
 					tmp = storage[i];
 					storage[i] = storage[j];
 					storage[j] = tmp;
 				}
 			}
 		}

 		int minY = bounds.bottom();
 		int maxY = bounds.top();

 		for (int i = 0; i < edgeCount; i++) {
 			minY = std::max(GRoundToInt(bounds.top()), std::min(storage[i].minY, minY));
 			maxY = std::min(GRoundToInt(bounds.bottom()), std::max(storage[i].maxY, maxY));
 		}
 		
 		for (int y = minY; y < maxY; y++) {
 			std::vector<int> xValues;
 			for (int i = 0; i < edgeCount; i++) {
 				if (storage[i].containsY(y)) {
 					xValues.push_back(storage[i].currX);
 					storage[i].incrementCurrX();
 				}
 			}
 			std::sort(xValues.begin(), xValues.end());
 			// xValues.erase( std::unique( xValues.begin(), xValues.end() ), xValues.end() );

 			for (int j = 0; j < xValues.size(); j++) {
 				if (j % 2 == 0) {
 					GPixel pixelStorage[GRoundToInt(bounds.width())];
 					int minX = std::max(0, std::min(xValues[j], GRoundToInt(bounds.width())));
					int maxX = std::max(0, std::min(xValues[j + 1], GRoundToInt(bounds.width())));
					if (shader) {
						shader->shadeRow(minX, y, maxX - minX, pixelStorage);
				    	if (fl) {
				    		fl->filter(pixelStorage, pixelStorage, maxX - minX);
				    	}
					}
					int pixelStorageIndex = 0;
					for (int x = minX; x < maxX; x++) {
		 				GPixel* address = this->currentDevice->getAddr(x, y);
			    		memcpy(&dPixel, address, sizeof(GPixel));
			    		if (shader) {
		    				sPixel = pixelStorage[pixelStorageIndex];
							pixelStorageIndex++;
			    		}
			    		rPixel = generateRPixel(mode, sPixel, dPixel);
			    		memcpy(address, &rPixel, sizeof(GPixel));
		 			}
 				}
 			}
 			// std::vector<Edge*> activeEdges;
 			// Edge* e1;
 			// for (int i = 0; i < edgeCount; i++) {
 			// 	if (storage[i].containsY(y)) {
 			// 		e1 = storage[i];
 			// 		activeEdges.push_back(e1);
 			// 	}
 			// }

 			// int winding = 0;
 			// //Sort activEdges based on currX
 			// int leftX;
 			// int rightX;
 			// GPixel pixelStorage[GRoundToInt(bounds.width())];
 			// for (int i = 0; i < edgeCount; i++) {
 			// 	if (storage[i].containsY(y)) {
 			// 		if (winding == 0) {
 			// 			leftX = std::max(0, std::min(GRoundToInt(storage[i].currX), GRoundToInt(bounds.width())));
 			// 			storage[i].incrementCurrX();
 			// 		}
 			// 		winding += storage[i].winding;
 			// 		if (winding == 0) {
 			// 			rightX = std::max(0, std::min(GRoundToInt(storage[i].currX), GRoundToInt(bounds.width())));
 			// 			storage[i].incrementCurrX();
 			// 			if (shader) {
				// 			shader->shadeRow(leftX, y, rightX - leftX, pixelStorage);
				// 	    	if (fl) {
				// 	    		fl->filter(pixelStorage, pixelStorage, rightX - leftX);
				// 	    	}
				// 		}
				// 		int pixelStorageIndex = 0;
				// 		for (int x = leftX; x < rightX; x++) {
			 // 				GPixel* address = this->currentDevice->getAddr(x, y);
				//     		memcpy(&dPixel, address, sizeof(GPixel));
				//     		if (shader) {
			 //    				sPixel = pixelStorage[pixelStorageIndex];
				// 				pixelStorageIndex++;
				//     		}
				//     		rPixel = generateRPixel(mode, sPixel, dPixel);
				//     		memcpy(address, &rPixel, sizeof(GPixel));
			 // 			}
 			// 		}
 			// 	}
 			// }
 		}
 	}

 	void concat(const GMatrix& matrix) {
		this->ctm = this->ctm.preConcat(matrix);
 	}

 	static Edge* clip_line(const GRect& bounds, GPoint p0, GPoint p1, Edge* edge) {
	    if (p0.fY == p1.fY) {
	        return edge;
	    }

	    if (p0.fY > p1.fY) {
	        std::swap(p0, p1);
	    }
	    // now we're monotonic in Y: p0 <= p1
	    if (p1.fY <= bounds.top() || p0.fY >= bounds.bottom()) {
	        return edge;
	    }
	    
	    double dxdy = (double)(p1.fX - p0.fX) / (p1.fY - p0.fY);
	    if (p0.fY < bounds.top()) {
	        p0.fX += dxdy * (bounds.top() - p0.fY);
	        p0.fY = bounds.top();
	    }
	    if (p1.fY > bounds.bottom()) {
	        p1.fX += dxdy * (bounds.bottom() - p1.fY);
	        p1.fY = bounds.bottom();
	    }

	    // Now p0...p1 is strictly inside bounds vertically, so we just need to clip horizontally

	    if (p0.fX > p1.fX) {
	        std::swap(p0, p1);
	    }
	    // now we're left-to-right: p0 .. p1

	    if (p1.fX <= bounds.left()) {   // entirely to the left
	        p0.fX = p1.fX = bounds.left();
	        return edge + edge->init(p0, p1);
	    }
	    if (p0.fX >= bounds.right()) {  // entirely to the right
	        p0.fX = p1.fX = bounds.right();
	        return edge + edge->init(p0, p1);
	    }

	    if (p0.fX < bounds.left()) {
	        float y = p0.fY + (bounds.left() - p0.fX) / dxdy;
	        edge += edge->init(GPoint::Make(bounds.left(), p0.fY), GPoint::Make(bounds.left(), y));
	        p0.set(bounds.left(), y);
	    }
	    if (p1.fX > bounds.right()) {
	        float y = p0.fY + (bounds.right() - p0.fX) / dxdy;
	        edge += edge->init(GPoint::Make(bounds.right(), y), GPoint::Make(bounds.right(), p1.fY));
	        p1.set(bounds.right(), y);
	    }
	    return edge + edge->init(p0, p1);
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

	uint64_t expand(uint32_t x) {
	    uint64_t hi = x & 0xFF00FF00;  // the A and G components
	    uint64_t lo = x & 0x00FF00FF;  // the R and B components
	    return (hi << 24) | lo;
	}

	// turn 0xXX into 0x00XX00XX00XX00XX
	uint64_t replicate(uint64_t x) {
	    return (x << 48) | (x << 32) | (x << 16) | x;
	}

	// turn 0x..AA..CC..BB..DD into 0xAABBCCDD
	uint32_t compact(uint64_t x) {
	    return ((x >> 24) & 0xFF00FF00) | (x & 0xFF00FF);
	}

	void save() {
 		GMatrix save(this->ctm[GMatrix::SX], this->ctm[GMatrix::KX], this->ctm[GMatrix::TX], this->ctm[GMatrix::KX], this->ctm[GMatrix::SY], this->ctm[GMatrix::TY]);
 		this->ctmStack.push(save);
 	}

 	void restore() {
 		if (this->ctmStack.empty()) {
 			//Error
 		} else {
 			if (this->layerStack.empty()) {
 				//Regular save was used instead of save layer
 				GMatrix popped = this->ctmStack.top();
 				this->ctmStack.pop();
 				this->ctm.set6(popped[GMatrix::SX], popped[GMatrix::KX], popped[GMatrix::TX], popped[GMatrix::KX], popped[GMatrix::SY], popped[GMatrix::TY]);
 			} else {
 				//Get previous layer
 				Layer currentLayer = layerStack.top();
 				layerStack.pop();

 				//Get paint settings
 				GFilter* fl = currentLayer.fPaint.getFilter();
 				GBlendMode mode = currentLayer.fPaint.getBlendMode();

 				//Reset CTM
 				GMatrix layerCtm;
 				layerCtm.set6(this->ctm[GMatrix::SX], this->ctm[GMatrix::KX], this->ctm[GMatrix::TX], this->ctm[GMatrix::KX], this->ctm[GMatrix::SY], this->ctm[GMatrix::TY]);
 				if (currentLayer.fBounds) {
 					layerCtm.postTranslate(currentLayer.fBounds->left(), currentLayer.fBounds->top());
 				}
 				GMatrix popped = this->ctmStack.top();
 				this->ctmStack.pop();
 				this->ctm.set6(popped[GMatrix::SX], popped[GMatrix::KX], popped[GMatrix::TX], popped[GMatrix::KX], popped[GMatrix::SY], popped[GMatrix::TY]);
 				GPixel sPixel;
				GPixel dPixel;
				GPixel rPixel;
				for (int y = 0; y < this->fDevice.height(); y++) {
					for (int x = 0; x < this->fDevice.width(); x++) {
						GPoint layerLocalPoint = layerCtm.mapXY(x, y);
						int pinnedX = std::max(0, std::min(GRoundToInt(layerLocalPoint.fX), currentLayer.fBitmap.width() - 1));
						int pinnedY = std::max(0, std::min(GRoundToInt(layerLocalPoint.fY), currentLayer.fBitmap.height() - 1));
						GPixel* layerAddress = currentLayer.fBitmap.getAddr(pinnedX, pinnedY);
						memcpy(&sPixel, layerAddress, sizeof(GPixel));
						if (fl) {
							fl->filter(&sPixel, &sPixel, 1);
						}
						GPixel* address = this->fDevice.getAddr(x, y);
						memcpy(&dPixel, address, sizeof(GPixel));
						rPixel = generateRPixel(mode, sPixel, dPixel);
						memcpy(address, &rPixel, sizeof(GPixel));
					}
				}

 				this->currentDevice = &this->fDevice;
 			}
 		}
 	}

 	static void setup_bitmap(GBitmap* bitmap, int w, int h) {
	    size_t rb = w * sizeof(GPixel);
	    bitmap->reset(w, h, rb, (GPixel*)calloc(h, rb), GBitmap::kNo_IsOpaque);
	}

protected:
	void onSaveLayer(const GRect* bounds, const GPaint& paint) {		save();
		if (bounds) {
			if (this->layerStack.empty()) {
				ctm.postTranslate(0 - bounds->left(), this->fDevice.height() - bounds->top());
			} else {
				ctm.postTranslate(this->layerStack.top().fBounds->left() - bounds->left(), this->layerStack.top().fBounds->top() - bounds->top());
			}
		}
		GBitmap newLayerBitmap;
		setup_bitmap(&newLayerBitmap, this->fDevice.width(), this->fDevice.height());
		Layer newLayer(newLayerBitmap, bounds, paint);
		this->layerStack.push(newLayer);
		this->currentDevice = &this->layerStack.top().fBitmap;
	}

private:
	GBitmap fDevice;
	GBitmap* currentDevice;
	GMatrix ctm;
	std::stack<GMatrix> ctmStack;
	std::stack<Layer> layerStack;
};

std::unique_ptr<GCanvas> GCreateCanvas(const GBitmap& device) {
    if (!device.pixels()) {
        return nullptr;
    }
    return std::unique_ptr<GCanvas>(new EmptyCanvas(device));
}

