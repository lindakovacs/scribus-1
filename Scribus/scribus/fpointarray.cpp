/***************************************************************************
                          fpointarray.cpp  -  description
                             -------------------
    begin                : Mit Jul 24 2002
    copyright            : (C) 2002 by Franz Schmid
    email                : Franz.Schmid@altmuehlnet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "fpointarray.h"
#include <cstdarg>

using namespace std;

void FPointArray::setPoint(uint i, double x, double y)
{
	QMemArray<FPoint>::at( i ) = FPoint( x, y );
}

void FPointArray::setPoint(uint i, FPoint p)
{
	setPoint(i, p.x(), p.y());
}

bool FPointArray::setPoints( int nPoints, double firstx, double firsty, ... )
{
	va_list ap;
	if ( !resize(nPoints) )
		return false;
	setPoint( 0, firstx, firsty );
	int i = 1;
	double x, y;
	nPoints--;
	va_start( ap, firsty );
	while ( nPoints-- )
	{
		x = static_cast<double>(va_arg( ap, double ));
		y = static_cast<double>(va_arg( ap, double ));
		setPoint( i++, x, y );
    }
	va_end( ap );
	return true;
}

bool FPointArray::putPoints( int index, int nPoints, double firstx, double firsty,  ... )
{
	va_list ap;
	if ( index + nPoints > static_cast<int>(size()) )
	{
		if ( !resize(index + nPoints) )
			return false;
	}
	if ( nPoints <= 0 )
		return true;
	setPoint( index, firstx, firsty );		// set first point
	int i = index + 1;
	double x, y;
	nPoints--;
	va_start( ap, firsty );
	while ( nPoints-- )
	{
		x = static_cast<double>(va_arg(ap, double));
		y = static_cast<double>(va_arg(ap, double));
		setPoint( i++, x, y );
	}
	va_end( ap );
	return true;
}

bool FPointArray::putPoints( int index, int nPoints, const FPointArray & from, int fromIndex )
{
	if ( index + nPoints > static_cast<int>(size()) )
	{	// extend array
		if ( !resize(index + nPoints) )
			return false;
	}
	if ( nPoints <= 0 )
		return true;
	int n = 0;
	while( n < nPoints )
	{
		setPoint( index+n, from[fromIndex+n] );
		n++;
    }
	return true;
}

void FPointArray::point(uint i, double *x, double *y)
{
	FPoint p = QMemArray<FPoint>::at(i);
	if (x)
		*x = p.x();
	if (y)
		*y = p.y();
}

FPoint FPointArray::point(uint i)
{
	return QMemArray<FPoint>::at(i);
}

QPoint FPointArray::pointQ(uint i)
{
	FPoint p = QMemArray<FPoint>::at(i);
	QPoint r(qRound(p.x()),qRound(p.y()));
	return r;
}

void FPointArray::translate( double dx, double dy )
{
	FPoint *p = data();
	int i = size();
	FPoint pt( dx, dy );
	while ( i-- )
    {
		if (p->x() < 900000)
     	*p += pt;
    	p++;
    }
}

FPoint FPointArray::WidthHeight()
{
	if ( isEmpty() )
		return FPoint( 0.0, 0.0 );		// null rectangle
	FPoint *pd = data();
	double minx, maxx, miny, maxy;
	minx = maxx = pd->x();
	miny = maxy = pd->y();
	pd++;
	for ( int i=1; i < static_cast<int>(size()); ++i )
	{	// find min+max x and y
		if ( pd->x() < minx )
			minx = pd->x();
		else
			if ( pd->x() > maxx )
		    	maxx = pd->x();
		if ( pd->y() < miny )
			miny = pd->y();
		else
			if ( pd->y() > maxy )
	    		maxy = pd->y();
		pd++;
    }
	return FPoint(maxx - minx,maxy - miny);
}

void FPointArray::map( QWMatrix m )
{
	FPoint *p = data();
	double mx, my;
	int i = size();
	while ( i-- )
	{
		if (p->x() > 900000)
		{
			mx = p->x();
			my = p->y();
		}
		else
		{
			mx = m.m11() * p->x() + m.m21() * p->y() + m.dx();
			my = m.m22() * p->y() + m.m12() * p->x() + m.dy();
		}
		*p = FPoint(mx, my);
		p++;
	}
}

void FPointArray::setMarker()
{
	addPoint(999999.0, 999999.0);
	addPoint(999999.0, 999999.0);
	addPoint(999999.0, 999999.0);
	addPoint(999999.0, 999999.0);
}

void FPointArray::addPoint(double x, double y)
{
	resize(size()+1);
	setPoint(size()-1, FPoint(x, y));
}

void FPointArray::addPoint(FPoint p)
{
	resize(size()+1);
	setPoint(size()-1, p.x(), p.y());
}

void FPointArray::addQuadPoint(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
{
	resize(size()+1);
	setPoint(size()-1, FPoint(x1, y1));
	resize(size()+1);
	setPoint(size()-1, FPoint(x2, y2));
	resize(size()+1);
	setPoint(size()-1, FPoint(x3, y3));
	resize(size()+1);
	setPoint(size()-1, FPoint(x4, y4));
}

void FPointArray::addQuadPoint(FPoint p1, FPoint p2, FPoint p3, FPoint p4)
{
	resize(size()+1);
	setPoint(size()-1, p1.x(), p1.y());
	resize(size()+1);
	setPoint(size()-1, p2.x(), p2.y());
	resize(size()+1);
	setPoint(size()-1, p3.x(), p3.y());
	resize(size()+1);
	setPoint(size()-1, p4.x(), p4.y());
}
