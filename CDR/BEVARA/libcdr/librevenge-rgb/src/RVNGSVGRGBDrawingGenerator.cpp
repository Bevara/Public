/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* librevenge
 * Version: MPL 2.0 / LGPLv2.1+
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Major Contributor(s):
 * Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU Lesser General Public License Version 2.1 or later
 * (LGPLv2.1+), in which case the provisions of the LGPLv2.1+ are
 * applicable instead of those above.
 */

 
 // Bevara April 2, 2019  MEB
 // 	Adding this new class that outputs both SVG and RGBA
 // 	We anticipate paring down severely, and provding SVG/RGBA/SVG+RGBA options
 // 	so rather than overloading the SVG, we are temporarily creating
 // 	this extra class
 //
 //		See embedded TODOs and TBAs for long list of features to be finished
 // Bevara June 3, 2019 MEB
 //     Added RGBA Arc support via traslation of SVG to Cairo
 

// for debugging 
//#define BEV_DBG 1
 
#include <stdio.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <librevenge-generators/librevenge-generators.h>

#include "librevenge_internal.h"

#include <CDR_support_codes.h>

#include <cairo.h>
#include <stdio.h>
#include <math.h>


#include "cairoint.h"
#include "cairo-types-private.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-output-stream-private.h"

namespace librevenge
{

cairo_surface_t *surface;
cairo_t *cr;

namespace DrawingSVGRGB
{



// BEVARA: this is adapted from the Cairo png writer
// it should really be in a cairo file, but it's convenient here
int
cairo_write_surface_rgba(cairo_surface_t *msurface, int *outHeight, int *outWidth, unsigned char * outImg)
{
	
	int i,j;
    cairo_status_t status;
    cairo_image_surface_t *image;
    cairo_image_surface_t * volatile clone = NULL;
    void *image_extra;

	
    status = _cairo_surface_acquire_source_image (msurface,&image, &image_extra);

	
	/* Handle the various fallback formats (e.g. low bit-depth XServers)
     * by coercing them to a simpler format using pixman.
     */
    clone = _cairo_image_surface_coerce (image); // this should conver to ARGB32, RGB24 or just A8 channel
    status = clone->base.status;
 


	if ((clone->format != CAIRO_FORMAT_ARGB32) && (clone->format != CAIRO_FORMAT_RGB24))
	{
		// TODO: graceful exit handling
		printf("we need ARGB or RGB cairo format -- exiting\n"); exit(1);
		
	}
	
	// because we're basing this on the cairo PNG output, we'll follow their format
	// and use their functions for internal conversion

	// now we have to ouput
		
	
	#ifdef BEV_DBG
		printf("imgW=%d imgH = %d, cloneW=%d, cloneH=%d\n",image->width,image->height,clone->width,clone->height);
		printf("output format RGBA?  = %d\n",clone->format == CAIRO_FORMAT_ARGB32 ? 1:0);
		printf("output format RGB?  = %d\n",clone->format == CAIRO_FORMAT_RGB24 ? 1:0);
	#endif
		
	*outHeight = clone->height;
	*outWidth = clone->width;
	
	char *tmpptr;
	uint8_t tmpout;
	uint32_t pixel;
    uint8_t  alpha;
	
	
	// we are working in a local space at the moment, so malloc here and then 
	// we'll copy over to an externally malloc'd space
	//outImg = (unsigned char *) malloc(clone->height*clone->width*4);
	// nevermind....
	unsigned char* tmpoutptr = outImg;

	
	if (clone->format == CAIRO_FORMAT_ARGB32) 
	{
		for (i = 0; i < clone->height; i++)
		{
			// we need to do the ARGB => RGBA conversion
			
			
			tmpptr = (char *)  clone->data + i * clone->stride;
			for (j = 0; j < image->width; ++j)
			{
				memcpy (&pixel, tmpptr, sizeof (uint32_t));
				alpha = (pixel & 0xff000000) >> 24;
				if (alpha == 0) {
					*tmpoutptr = 0; ++tmpoutptr;
					*tmpoutptr = 0; ++tmpoutptr;
					*tmpoutptr = 0; ++tmpoutptr;
					*tmpoutptr = 0; ++tmpoutptr;
					
				} else {
				
					tmpout = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
					*tmpoutptr = tmpout; ++tmpoutptr;
					
					tmpout = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
					*tmpoutptr = tmpout; ++tmpoutptr;
					
					tmpout = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
					*tmpoutptr = tmpout; ++tmpoutptr;
					
					tmpout = alpha;
					*tmpoutptr = tmpout; ++tmpoutptr;
					
				}
				tmpptr += 4;				
			}
			
		}
	}
	else
	{
		// we shouldn't actually get here because we're committed to RGBA
		printf("RGB needs to handle the internal xRGB => RGBx conversion\n");
	}
				
	  _cairo_surface_release_source_image (surface, image, image_extra);
	  
	  return 0;
}

// BEVARA: given SVG arc parameters transform to a circular arc, use Cairo's arc function to draw, and
// transform back to elliptical arc 
// this uses a portion of the compute function from 
//	https://stackoverflow.com/questions/41537950/converting-an-svg-arc-to-lines
//	Inputs: 
//	lx0 ly0 = start point
//  lx1 ly1 = end point
//  rx  ry = x & y radii
//  alfa = xrot off x-axis
// sweep larc = weep and large arc flags
//	
// Note all corner cases are not yet tested, use with caution
//
int arcViaTransform(double lx0,double ly0, double lx1,double ly1,double rx, double ry, double alfa, int sweep, int larc)
{
	

	// setup some local vals	
	double PI = 3.14159; // pick resolution you need 		
    double  ax,ay,bx,by;            // this will be the set of rotated, then rotated and scaled endpoints of the arc
    double  vx,vy,l,db;
	double a0, a1; // final arc angles
	double ang; //  the angle for the inverse rotation
	double sx,sy; // center of the chord of arc

    double  c,s,e;

	ang = -1*alfa*(3.14159/180.0); // the angle for the inverse rotation -- need to switch to rad
  

	if (ry !=0) // really, we shouldn't have a radius of 0, but just in case
		e=rx/ry;
	else return 1;
    c=cos(ang);
    s=sin(ang);
    ax=lx0*c-ly0*s;    // these are the inv rotated arc endpoints
    ay=lx0*s+ly0*c;
    bx=lx1*c-ly1*s;
    by=lx1*s+ly1*c;

	#if BEV_DBG
	printf("CIRCULAR: circle scale factor e=%f, rot start points (%f,%f) , rot end points(%f,%f) \n",e, ax, ay, bx, by);
	#endif
	
    ay*=e;                  // transform to circle by scaling 
    by*=e;
	
	

	// now find the center point of the circle
    sx=0.5*(ax+bx);         // first find mid point between A,B; this is midpoint of the chord
    sy=0.5*(ay+by);
	
	#if BEV_DBG
	printf("CIRCULAR: circle scale factor e=%f, rot/scaled start (%f,%f) , rot/scaled end(%f,%f) \n",e, ax, ay, bx, by);
	printf("CIRCULAR: chord center sx=%f, sy=%f\n",sx,sy);
	#endif
	

	
    vx=(ay-by);  // going for the perp bisector
    vy=(bx-ax);
	
	// Corner Case
	// Recall that we are converting from SVG. SVG doesn't seem to care
	// about the specified radii, but rather the ratio of the radii. That is, we have 
	// to take care of the 
	// case where the given start/stop points aren't actually on a arc with the given radii. 
	// Thus, to draw an arc between the
	// points, we have to adapt the radii to fit the arc chord we're given. 
	double h = sqrt((vx*vx)+(vy*vy))/2; // half of the chord will possibly now be the rx value
	if ((rx < h ) && (ry < h))
		
		{
			
			double einc, edec;
		
			int resetRx = 0.0;
			#if BEV_DBG
			printf("CIRCULAR: chord half length h=%f\n",h);
			printf("CIRCULAR: rx was %f, ry was %f\n",rx,ry);
			#endif
			if (rx<h)
			{
				einc = h/rx;
				rx = h;
				ry = einc*ry;
				resetRx = 1.0;
				
				
			}
	
			// start from beginning and reset everything instead of scaling -- tedious but helps with debugging
			if (ry !=0) // really, we shouldn't have a radius of 0, but just in case
				e=rx/ry;
			else return 1;
			c=cos(ang);
			s=sin(ang);
			ax=lx0*c-ly0*s;    // these are the inv rotated arc endpoints
			ay=lx0*s+ly0*c;
			bx=lx1*c-ly1*s;
			by=lx1*s+ly1*c;

			
			ay*=e;                  // transform to circle by scaling 
			by*=e;
			

			// now find the center point of the circle
			sx=0.5*(ax+bx);         // first find mid point between A,B; this is midpoint of the chord
			sy=0.5*(ay+by);
			
				
			vx=(ay-by);  // going for the perp bisector yet again
			vy=(bx-ax);
		}
	

	
	// now have to scale it using the sagitta  l^2 = (a-s)^2/c^2 where c is the chord length
	if ((vx*vx)+(vy*vy) != 0)
		l=(rx*rx)/((vx*vx)+(vy*vy))-0.25;
	else return 1;
    if (l<0) l=0; // this takes care of the reset case, or when the chord is very small
    l=sqrt(l);
    vx*=l;
    vy*=l;


	
	// Decide which direction the arc faces, and add the scaled perp bisect to the
	// chord's center point; actually, we need to mirror the coords if we have 
	// a positive sweep value and then add the sagitta-based adjustment
	// initialize to a positive arc then figure out which one we're doing
	int posArc = 1; 
    if (sweep) // sweep is 1
        {
			if (larc) //larc is 1
			{
				sx-=vx;
				sy-=vy;
				
			}
			else
			{
				sx+=vx;
				sy+=vy;
				//posArc = 0;
				
			}
				
        }
    else{  // sweep is 0
        	if (larc) //larc is 1
			{
				sx+=vx;
				sy+=vy;
				posArc = 0;
			}
			else
			{
				sx-=vx;
				sy-=vy;
				posArc = 0;
				
			}
        }



		// find start and end angles
		a0=atan2(ay-sy,ax-sx);
		a1=atan2(by-sy,bx-sx);

		#if BEV_DBG
		printf("CIRCULAR: start and end angles in rad a0=%f a1=%f \n",  a0, a1);	
		#endif
	

	
		// now draw the circle and then transform		
		cairo_new_sub_path(cr);
		
		
		#if BEV_DBG	
		printf("\nGeneral Transformed Arc info\n");
		printf("\t CIRCULAR: rotated end points are x1=%f,y1=%f  x2=%f, y2=%f\n", ax,ay,bx,by);
		printf("\t CIRCULAR: large arc larc=%d  final sweep =%d\n", larc, sweep);
		printf("\t CIRCULAR: center points are sx=%f  sy=%f\n", sx, sy);
		printf("\t CIRCULAR: radii are rx = %f, ry=%f \n", rx,ry);
		printf("\t CIRCULAR: start and end angles in degrees a0=%f a1=%f  rot angle in rad ang =%f \n",  a0*180.0/3.1415, a1*180.0/3.1415,ang);
		#endif
		
		cairo_save(cr);				
		cairo_rotate(cr, -ang);		
		cairo_scale(cr, 1.0, 1/e);	
		if (posArc)
			cairo_arc (cr, sx, sy, rx, a0, a1);		
		else 
			cairo_arc_negative (cr, sx, sy, rx, a0, a1);		
		cairo_restore(cr);		
	
}



static double getInchValue(librevenge::RVNGProperty const &prop)
{
	double value=prop.getDouble();
	switch (prop.getUnit())
	{
	case librevenge::RVNG_GENERIC: // assume inch
	case librevenge::RVNG_INCH:
		return value;
	case librevenge::RVNG_POINT:
		value /= 72.;
		return value;
	case librevenge::RVNG_TWIP:
		value /= 1440.;
		return value;
	case librevenge::RVNG_PERCENT:
	case librevenge::RVNG_UNIT_ERROR:
	default:
	{
		static bool first=true;
		if (first)
		{
			RVNG_DEBUG_MSG(("librevenge::getInchValue: call with no double value\n"));
			first=false;
		}
		break;
	}
	}
	return value;
}

static std::string doubleToString(const double value)
{
	RVNGProperty *prop = RVNGPropertyFactory::newDoubleProp(value);
	std::string retVal = prop->getStr().cstr();
	delete prop;
	return retVal;
}

static unsigned stringToColour(const RVNGString &s)
{
	std::string str(s.cstr());
	if (str[0] == '#')
	{
		if (str.length() != 7)
			return 0;
		else
			str.erase(str.begin());
	}
	else
		return 0;

	std::istringstream istr(str);
	unsigned val = 0;
	istr >> std::hex >> val;
	return val;
}

//! basic class used to store table information
struct Table
{
	//! constructor
	Table(const RVNGPropertyList &propList) : m_column(0), m_row(0), m_x(0), m_y(0), m_columnsDistanceFromOrigin(), m_rowsDistanceFromOrigin()
	{
		if (propList["svg:x"])
			m_x=getInchValue(*propList["svg:x"]);
		if (propList["svg:y"])
			m_y=getInchValue(*propList["svg:y"]);
		// we do not actually use height/width, so...

		m_columnsDistanceFromOrigin.push_back(0);
		m_rowsDistanceFromOrigin.push_back(0);

		const librevenge::RVNGPropertyListVector *columns = propList.child("librevenge:table-columns");
		if (columns)
		{
			double actDist=0;
			for (unsigned long i=0; i<columns->count(); ++i)
			{
				if ((*columns)[i]["style:column-width"])
					actDist+=getInchValue(*(*columns)[i]["style:column-width"]);
				m_columnsDistanceFromOrigin.push_back(actDist);
			}
		}
		else
		{
			RVNG_DEBUG_MSG(("librevenge::DrawingSVG::Table::Table: can not find any columns\n"));
		}
	}
	//! calls to open a row
	void openRow(const RVNGPropertyList &propList)
	{
		double height=0;
		if (propList["style:row-height"])
			height=getInchValue(*propList["style:row-height"]);
		// changeme
		else if (propList["style:min-row-height"])
			height=getInchValue(*propList["style:min-row-height"]);
		else
		{
			RVNG_DEBUG_MSG(("librevenge::DrawingSVG::Table::openRow: can not determine row height\n"));
		}
		m_rowsDistanceFromOrigin.push_back(m_rowsDistanceFromOrigin.back()+height);
	}
	//! call to close a row
	void closeRow()
	{
		++m_row;
	}
	//! returns the position of a cellule
	bool getPosition(int column, int row, double &x, double &y) const
	{
		bool ok=true;
		if (column>=0 && column <int(m_columnsDistanceFromOrigin.size()))
			x=m_x+m_columnsDistanceFromOrigin[size_t(column)];
		else
		{
			ok=false;
			RVNG_DEBUG_MSG(("librevenge::DrawingSVG::Table::getPosition: the column %d seems bad\n", column));
			x=(column<0 || m_columnsDistanceFromOrigin.empty()) ? m_x : m_x + m_columnsDistanceFromOrigin.back();
		}
		if (row>=0 && row <int(m_rowsDistanceFromOrigin.size()))
			y=m_y+m_rowsDistanceFromOrigin[size_t(row)];
		else
		{
			ok=false;
			RVNG_DEBUG_MSG(("librevenge::DrawingSVG::Table::getPosition: the row %d seems bad\n", row));
			y=(row<0 || m_rowsDistanceFromOrigin.empty()) ? m_y : m_y + m_rowsDistanceFromOrigin.back();
		}
		return ok;
	}
	//! the actual column
	int m_column;
	//! the actual row
	int m_row;
	//! the origin table position in inches
	double m_x, m_y;
	//! the distance of each begin column in inches from origin
	std::vector<double> m_columnsDistanceFromOrigin;
	//! the distance of each begin row in inches from origin
	std::vector<double> m_rowsDistanceFromOrigin;
};

} // DrawingSVG namespace

using namespace DrawingSVGRGB;

struct RVNGSVGRGBDrawingGeneratorPrivate
{
	RVNGSVGRGBDrawingGeneratorPrivate(RVNGStringVector &vec, unsigned char *rgbaOutbuf, int *rgbaOutWidth, int *rgbaOutHeight, int *nsFeatures, const RVNGString &nmSpace);

	void setStyle(const RVNGPropertyList &propList);
	void writeStyle(bool isClosed=true);
	void drawPolySomething(const RVNGPropertyListVector &vertices, bool isClosed);

	//! return the namespace and the delimiter
	std::string const &getNamespaceAndDelim() const
	{
		return m_nmSpaceAndDelim;
	}

	std::map<int, RVNGPropertyList> m_idSpanMap;

	RVNGPropertyListVector m_gradient;
	RVNGPropertyList m_style;
	int m_gradientIndex, m_shadowIndex;
	//! index uses when fill=bitmap
	int m_patternIndex;
	int m_arrowStartIndex /** start arrow index*/, m_arrowEndIndex /** end arrow index */;
	//! groupId used if svg:id is not defined when calling openGroup
	int m_groupId;
	//! layerId used if svg:id is not defined when calling startLayer
	int m_layerId;
	//! a prefix used to define the svg namespace
	std::string m_nmSpace;
	//! a prefix used to define the svg namespace with delimiter
	std::string m_nmSpaceAndDelim;
	std::ostringstream m_outputSink;
	// BEVARA: added  TODO: consolidate into an output struct
	unsigned char * m_rgbaOutput;
	int *m_rgbOutWidth;
	int *m_rgbOutHeight;
	int *m_nsFeatures;
	
	RVNGStringVector &m_vec;
	//! the actual master name
	RVNGString m_masterName;
	//! a map master name to master content
	std::map<RVNGString, std::string> m_masterNameToContentMap;
	//! the actual opened table
	boost::shared_ptr<Table> m_table;
};

RVNGSVGRGBDrawingGeneratorPrivate::RVNGSVGRGBDrawingGeneratorPrivate(RVNGStringVector &vec, unsigned char *rgbaOutbuf, int *rgbaOutWidth, int *rgbaOutHeight, int *nsFeatures, const RVNGString &nmSpace) :
	m_idSpanMap(),
	m_gradient(),
	m_style(),
	m_gradientIndex(1),
	m_shadowIndex(1),
	m_patternIndex(1),
	m_arrowStartIndex(1),
	m_arrowEndIndex(1),
	m_groupId(1000), m_layerId(1000),
	m_nmSpace(nmSpace.cstr()),
	m_nmSpaceAndDelim(""),
	m_outputSink(),
	m_rgbaOutput(rgbaOutbuf),
	m_rgbOutWidth(rgbaOutWidth),
	m_rgbOutHeight(rgbaOutHeight),
	m_nsFeatures(nsFeatures),
	m_vec(vec),
	m_masterName(), m_masterNameToContentMap(),
	m_table()
{
	if (!m_nmSpace.empty())
		m_nmSpaceAndDelim = m_nmSpace+":";
}

void RVNGSVGRGBDrawingGeneratorPrivate::drawPolySomething(const RVNGPropertyListVector &vertices, bool isClosed)
{
	
	//BEVARA:TODO
	
	if (vertices.count() < 2)
		return;

	if (vertices.count() == 2)
	{
		if (!vertices[0]["svg:x"]||!vertices[0]["svg:y"]||!vertices[1]["svg:x"]||!vertices[1]["svg:y"])
			return;
		
		//Bevara: just add a line
		cairo_move_to (cr, 72*getInchValue(*vertices[0]["svg:x"]), 72*getInchValue(*vertices[0]["svg:y"]));
		cairo_line_to (cr, 72*getInchValue(*vertices[1]["svg:x"]), 72*getInchValue(*vertices[1]["svg:y"]));		
		#ifdef BEV_DBG 
			printf("adding a single polyline to %f,%f\n",72*getInchValue(*vertices[1]["svg:x"]), 72*getInchValue(*vertices[1]["svg:y"]));
		#endif
		
		
		m_outputSink << "<" << getNamespaceAndDelim() << "line ";
		m_outputSink << "x1=\"" << doubleToString(72*getInchValue(*vertices[0]["svg:x"])) << "\"  y1=\"" << doubleToString(72*getInchValue(*vertices[0]["svg:y"])) << "\" ";
		m_outputSink << "x2=\"" << doubleToString(72*getInchValue(*vertices[1]["svg:x"])) << "\"  y2=\"" << doubleToString(72*getInchValue(*vertices[1]["svg:y"])) << "\"\n";
		writeStyle();
		m_outputSink << "/>\n";
		

	}
	else
	{
		if (isClosed)
			m_outputSink << "<" << getNamespaceAndDelim() << "polygon ";
		else
			m_outputSink << "<" << getNamespaceAndDelim() << "polyline ";

		m_outputSink << "points=\"";
		for (unsigned i = 0; i < vertices.count(); i++)
		{
			if (!vertices[i]["svg:x"]||!vertices[i]["svg:y"])
				continue;
			
			//Bevara: draw polyline in segments via move to and then draw to
			m_outputSink << doubleToString(72*getInchValue(*vertices[i]["svg:x"])) << " " << doubleToString(72*getInchValue(*vertices[i]["svg:y"]));
			if (i < vertices.count()-1)
				m_outputSink << ", ";
			
			
		}
		m_outputSink << "\"\n";
		writeStyle(isClosed);
		m_outputSink << "/>\n";
	}
}

void RVNGSVGRGBDrawingGeneratorPrivate::setStyle(const RVNGPropertyList &propList)
{
	m_style.clear();
	m_style = propList;

	
	//BEVARA:TODO, but not currently using for CDR RGBA/SVG
	
	const librevenge::RVNGPropertyListVector *gradient = propList.child("svg:linearGradient");
	if (!gradient)
		gradient = propList.child("svg:radialGradient");
	m_gradient = gradient ? *gradient : librevenge::RVNGPropertyListVector();
	if (m_style["draw:shadow"] && m_style["draw:shadow"]->getStr() == "visible" && m_style["draw:shadow-opacity"])
	{
		double shadowRed = 0.0;
		double shadowGreen = 0.0;
		double shadowBlue = 0.0;
		if (m_style["draw:shadow-color"])
		{
			unsigned shadowColour = stringToColour(m_style["draw:shadow-color"]->getStr());
			shadowRed = (double)((shadowColour & 0x00ff0000) >> 16)/255.0;
			shadowGreen = (double)((shadowColour & 0x0000ff00) >> 8)/255.0;
			shadowBlue = (double)(shadowColour & 0x000000ff)/255.0;
		}
		m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
		m_outputSink << "<" << getNamespaceAndDelim() << "filter filterUnits=\"userSpaceOnUse\" id=\"shadow" << m_shadowIndex++ << "\">";
		m_outputSink << "<" << getNamespaceAndDelim() << "feOffset in=\"SourceGraphic\" result=\"offset\" ";
		if (m_style["draw:shadow-offset-x"])
			m_outputSink << "dx=\"" << doubleToString(72*getInchValue(*m_style["draw:shadow-offset-x"])) << "\" ";
		if (m_style["draw:shadow-offset-y"])
			m_outputSink << "dy=\"" << doubleToString(72*getInchValue(*m_style["draw:shadow-offset-y"])) << "\" ";
		m_outputSink << "/>";
		m_outputSink << "<" << getNamespaceAndDelim() << "feColorMatrix in=\"offset\" result=\"offset-color\" type=\"matrix\" values=\"";
		m_outputSink << "0 0 0 0 " << doubleToString(shadowRed) ;
		m_outputSink << " 0 0 0 0 " << doubleToString(shadowGreen);
		m_outputSink << " 0 0 0 0 " << doubleToString(shadowBlue);
		if (m_style["draw:opacity"] && m_style["draw:opacity"]->getDouble() < 1)
			m_outputSink << " 0 0 0 "   << doubleToString(m_style["draw:shadow-opacity"]->getDouble()/m_style["draw:opacity"]->getDouble()) << " 0\"/>";
		else
			m_outputSink << " 0 0 0 "   << doubleToString(m_style["draw:shadow-opacity"]->getDouble()) << " 0\"/>";

		m_outputSink << "<" << getNamespaceAndDelim() << "feMerge>";
		m_outputSink << "<" << getNamespaceAndDelim() << "feMergeNode in=\"offset-color\" />";
		m_outputSink << "<" << getNamespaceAndDelim() << "feMergeNode in=\"SourceGraphic\" />";
		m_outputSink << "</" << getNamespaceAndDelim() << "feMerge>";
		m_outputSink << "</" << getNamespaceAndDelim() << "filter>";
		m_outputSink << "</" << getNamespaceAndDelim() << "defs>";
	}

	if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "gradient")
	{
		double angle = (m_style["draw:angle"] ? m_style["draw:angle"]->getDouble() : 0.0);
		angle *= -1.0;
		while (angle < 0)
			angle += 360;
		while (angle > 360)
			angle -= 360;

		if (m_style["draw:style"] && (m_style["draw:style"]->getStr() == "radial" || m_style["draw:style"]->getStr() == "rectangular" ||
		                              m_style["draw:style"]->getStr() == "square" || m_style["draw:style"]->getStr() == "ellipsoid"))
		{
			m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
			m_outputSink << "  <" << getNamespaceAndDelim() << "radialGradient id=\"grad" << m_gradientIndex++ << "\"";

			if (m_style["svg:cx"])
				m_outputSink << " cx=\"" << m_style["svg:cx"]->getStr().cstr() << "\"";
			else if (m_style["draw:cx"])
				m_outputSink << " cx=\"" << m_style["draw:cx"]->getStr().cstr() << "\"";

			if (m_style["svg:cy"])
				m_outputSink << " cy=\"" << m_style["svg:cy"]->getStr().cstr() << "\"";
			else if (m_style["draw:cy"])
				m_outputSink << " cy=\"" << m_style["draw:cy"]->getStr().cstr() << "\"";
			if (m_style["svg:r"])
				m_outputSink << " r=\"" << m_style["svg:r"]->getStr().cstr() << "\"";
			else if (m_style["draw:border"])
				m_outputSink << " r=\"" << doubleToString((1 - m_style["draw:border"]->getDouble())*100.0) << "%\"";
			else
				m_outputSink << " r=\"100%\"";
			m_outputSink << " >\n";
			if (m_gradient.count())
			{
				for (unsigned c = 0; c < m_gradient.count(); c++)
				{
					RVNGPropertyList const &grad=m_gradient[c];
					m_outputSink << "    <" << getNamespaceAndDelim() << "stop";
					if (grad["svg:offset"])
						m_outputSink << " offset=\"" << grad["svg:offset"]->getStr().cstr() << "\"";
					if (grad["svg:stop-color"])
						m_outputSink << " stop-color=\"" << grad["svg:stop-color"]->getStr().cstr() << "\"";
					if (grad["svg:stop-opacity"])
						m_outputSink << " stop-opacity=\"" << doubleToString(grad["svg:stop-opacity"]->getDouble()) << "\"";
					m_outputSink << "/>" << std::endl;
				}
			}
			else if (m_style["draw:start-color"] && m_style["draw:end-color"])
			{
				m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"0%\"";
				m_outputSink << " stop-color=\"" << m_style["draw:end-color"]->getStr().cstr() << "\"";
				m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:end-opacity"] ? m_style["librevenge:end-opacity"]->getDouble() : 1) << "\" />" << std::endl;

				m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"100%\"";
				m_outputSink << " stop-color=\"" << m_style["draw:start-color"]->getStr().cstr() << "\"";
				m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:start-opacity"] ? m_style["librevenge:start-opacity"]->getDouble() : 1) << "\" />" << std::endl;
			}
			m_outputSink << "  </" << getNamespaceAndDelim() << "radialGradient>\n";
			m_outputSink << "</" << getNamespaceAndDelim() << "defs>\n";
		}
		else if (!m_style["draw:style"] || m_style["draw:style"]->getStr() == "linear" || m_style["draw:style"]->getStr() == "axial")
		{
			m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
			m_outputSink << "  <" << getNamespaceAndDelim() << "linearGradient id=\"grad" << m_gradientIndex++ << "\" >\n";

			if (m_gradient.count())
			{
				bool canBuildAxial = false;
				if (m_style["draw:style"] && m_style["draw:style"]->getStr() == "axial")
				{
					// check if we can reconstruct the linear offset, ie. if each offset is a valid percent%
					canBuildAxial = true;
					for (unsigned c = 0; c < m_gradient.count(); ++c)
					{
						RVNGPropertyList const &grad=m_gradient[c];
						if (!grad["svg:offset"] || grad["svg:offset"]->getDouble()<0 || grad["svg:offset"]->getDouble() > 1)
						{
							canBuildAxial=false;
							break;
						}
						RVNGString str=grad["svg:offset"]->getStr();
						int len=str.len();
						if (len<1 || str.cstr()[len-1]!='%')
						{
							canBuildAxial=false;
							break;
						}
					}
				}
				if (canBuildAxial)
				{
					for (unsigned long c = m_gradient.count(); c>0 ;)
					{
						RVNGPropertyList const &grad=m_gradient[--c];
						m_outputSink << "    <" << getNamespaceAndDelim() << "stop ";
						if (grad["svg:offset"])
							m_outputSink << "offset=\"" << doubleToString(50.-50.*grad["svg:offset"]->getDouble()) << "%\"";
						if (grad["svg:stop-color"])
							m_outputSink << " stop-color=\"" << grad["svg:stop-color"]->getStr().cstr() << "\"";
						if (grad["svg:stop-opacity"])
							m_outputSink << " stop-opacity=\"" << doubleToString(grad["svg:stop-opacity"]->getDouble()) << "\"";
						m_outputSink << "/>" << std::endl;
					}
					for (unsigned long c = 0; c < m_gradient.count(); ++c)
					{
						RVNGPropertyList const &grad=m_gradient[c];
						if (c==0 && grad["svg:offset"] && grad["svg:offset"]->getDouble() <= 0)
							continue;
						m_outputSink << "    <" << getNamespaceAndDelim() << "stop ";
						if (grad["svg:offset"])
							m_outputSink << "offset=\"" << doubleToString(50.+50.*grad["svg:offset"]->getDouble()) << "%\"";
						if (grad["svg:stop-color"])
							m_outputSink << " stop-color=\"" << grad["svg:stop-color"]->getStr().cstr() << "\"";
						if (grad["svg:stop-opacity"])
							m_outputSink << " stop-opacity=\"" << doubleToString(grad["svg:stop-opacity"]->getDouble()) << "\"";
						m_outputSink << "/>" << std::endl;
					}
				}
				else
				{
					for (unsigned c = 0; c < m_gradient.count(); c++)
					{
						RVNGPropertyList const &grad=m_gradient[c];
						m_outputSink << "    <" << getNamespaceAndDelim() << "stop";
						if (grad["svg:offset"])
							m_outputSink << " offset=\"" << grad["svg:offset"]->getStr().cstr() << "\"";
						if (grad["svg:stop-color"])
							m_outputSink << " stop-color=\"" << grad["svg:stop-color"]->getStr().cstr() << "\"";
						if (grad["svg:stop-opacity"])
							m_outputSink << " stop-opacity=\"" << doubleToString(grad["svg:stop-opacity"]->getDouble()) << "\"";
						m_outputSink << "/>" << std::endl;
					}
				}
			}
			else if (m_style["draw:start-color"] && m_style["draw:end-color"])
			{
				if (!m_style["draw:style"] || m_style["draw:style"]->getStr() == "linear")
				{
					m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"0%\"";
					m_outputSink << " stop-color=\"" << m_style["draw:start-color"]->getStr().cstr() << "\"";
					m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:start-opacity"] ? m_style["librevenge:start-opacity"]->getDouble() : 1) << "\" />" << std::endl;

					m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"100%\"";
					m_outputSink << " stop-color=\"" << m_style["draw:end-color"]->getStr().cstr() << "\"";
					m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:end-opacity"] ? m_style["librevenge:end-opacity"]->getDouble() : 1) << "\" />" << std::endl;
				}
				else
				{
					m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"0%\"";
					m_outputSink << " stop-color=\"" << m_style["draw:end-color"]->getStr().cstr() << "\"";
					m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:end-opacity"] ? m_style["librevenge:end-opacity"]->getDouble() : 1) << "\" />" << std::endl;

					m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"50%\"";
					m_outputSink << " stop-color=\"" << m_style["draw:start-color"]->getStr().cstr() << "\"";
					m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:start-opacity"] ? m_style["librevenge:start-opacity"]->getDouble() : 1) << "\" />" << std::endl;

					m_outputSink << "    <" << getNamespaceAndDelim() << "stop offset=\"100%\"";
					m_outputSink << " stop-color=\"" << m_style["draw:end-color"]->getStr().cstr() << "\"";
					m_outputSink << " stop-opacity=\"" << doubleToString(m_style["librevenge:end-opacity"] ? m_style["librevenge:end-opacity"]->getDouble() : 1) << "\" />" << std::endl;
				}
			}
			m_outputSink << "  </" << getNamespaceAndDelim() << "linearGradient>\n";

			// not a simple horizontal gradient
			if (angle<270 || angle>270)
			{
				m_outputSink << "  <" << getNamespaceAndDelim() << "linearGradient xlink:href=\"#grad" << m_gradientIndex-1 << "\"";
				m_outputSink << " id=\"grad" << m_gradientIndex++ << "\" ";
				m_outputSink << "x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\" ";
				m_outputSink << "gradientTransform=\"rotate(" << angle << " .5 .5)\" ";
				m_outputSink << "gradientUnits=\"objectBoundingBox\" >\n";
				m_outputSink << "  </" << getNamespaceAndDelim() << "linearGradient>\n";
			}

			m_outputSink << "</" << getNamespaceAndDelim() << "defs>\n";
		}
	}
	else if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "bitmap" && m_style["draw:fill-image"] && m_style["librevenge:mime-type"])
	{
		m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
		m_outputSink << "  <" << getNamespaceAndDelim() << "pattern id=\"img" << m_patternIndex++ << "\" patternUnits=\"userSpaceOnUse\" ";
		if (m_style["svg:width"])
			m_outputSink << "width=\"" << doubleToString(72*getInchValue(*m_style["svg:width"])) << "\" ";
		else
			m_outputSink << "width=\"100\" ";

		if (m_style["svg:height"])
			m_outputSink << "height=\"" << doubleToString(72*getInchValue(*m_style["svg:height"])) << "\">" << std::endl;
		else
			m_outputSink << "height=\"100\">" << std::endl;
		m_outputSink << "<" << getNamespaceAndDelim() << "image ";

		if (m_style["svg:x"])
			m_outputSink << "x=\"" << doubleToString(72*getInchValue(*m_style["svg:x"])) << "\" ";
		else
			m_outputSink << "x=\"0\" ";

		if (m_style["svg:y"])
			m_outputSink << "y=\"" << doubleToString(72*getInchValue(*m_style["svg:y"])) << "\" ";
		else
			m_outputSink << "y=\"0\" ";

		if (m_style["svg:width"])
			m_outputSink << "width=\"" << doubleToString(72*getInchValue(*m_style["svg:width"])) << "\" ";
		else
			m_outputSink << "width=\"100\" ";

		if (m_style["svg:height"])
			m_outputSink << "height=\"" << doubleToString(72*getInchValue(*m_style["svg:height"])) << "\" ";
		else
			m_outputSink << "height=\"100\" ";

		m_outputSink << "xlink:href=\"data:" << m_style["librevenge:mime-type"]->getStr().cstr() << ";base64,";
		m_outputSink << m_style["draw:fill-image"]->getStr().cstr();
		m_outputSink << "\" />\n";
		m_outputSink << "  </" << getNamespaceAndDelim() << "pattern>\n";
		m_outputSink << "</" << getNamespaceAndDelim() << "defs>\n";
	}

	// check for arrow and if find some, define a basic arrow
	if (m_style["draw:marker-start-path"])
	{
		m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
		m_outputSink << "<" << getNamespaceAndDelim() << "marker id=\"startMarker" << m_arrowStartIndex++ << "\" ";
		m_outputSink << " markerUnits=\"strokeWidth\" orient=\"auto\" markerWidth=\"8\" markerHeight=\"6\"\n";
		m_outputSink << " viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\">\n";
		m_outputSink << "<" << getNamespaceAndDelim() << "polyline points=\"10,0 0,5 10,10 9,5\" fill=\"solid\" />\n";
		m_outputSink << "</" << getNamespaceAndDelim() << "marker>\n";
		m_outputSink << "</" << getNamespaceAndDelim() << "defs>\n";
	}
	if (m_style["draw:marker-end-path"])
	{
		m_outputSink << "<" << getNamespaceAndDelim() << "defs>\n";
		m_outputSink << "<" << getNamespaceAndDelim() << "marker id=\"endMarker" << m_arrowEndIndex++ << "\" ";
		m_outputSink << " markerUnits=\"strokeWidth\" orient=\"auto\" markerWidth=\"8\" markerHeight=\"6\"\n";
		m_outputSink << " viewBox=\"0 0 10 10\" refX=\"1\" refY=\"5\">\n";
		m_outputSink << "<" << getNamespaceAndDelim() << "polyline points=\"0,0 10,5 0,10 1,5\" fill=\"solid\" />\n";
		m_outputSink << "</" << getNamespaceAndDelim() << "marker>\n";
		m_outputSink << "</" << getNamespaceAndDelim() << "defs>\n";
	}
}

// create "style" attribute based on current pen and brush
void RVNGSVGRGBDrawingGeneratorPrivate::writeStyle(bool /* isClosed */)
{
	//BEVARA: set up for stroke and fill commands then execute at the end
	bool strokeFlag = 0; // if we see any stroke commands, set this flag
	bool fillFlag = 0; // if we see any fill commands, set this flag
	double fillOpacity = 1.0; // default SVG opacity should be 1.0
	double fillColorR = 255.0;
	double fillColorG = 255.0;
	double fillColorB = 255.0;
	cairo_fill_rule_t fillRule =  CAIRO_FILL_RULE_WINDING; //CAIRO_FILL_RULE_EVEN_ODD	
	double strokeWidth = 0.0;
	double strokeOpacity = 1.0; // default SVG opacity should be 1.0
	double strokeColorR = 255.0;
	double strokeColorG = 255.0;
	double strokeColorB = 255.0;
	std::vector <double> strokeDashPatternVec;
	

	int strokeNumDashes = 0;
	double strokeDashOffset = 0.0;  // not used in CDR?
	cairo_line_cap_t strokeLineCap = CAIRO_LINE_CAP_BUTT; // CAIRO_LINE_CAP_ROUND or CAIRO_LINE_CAP_SQUARE
	cairo_line_join_t strokeLineJoin = CAIRO_LINE_JOIN_MITER; //CAIRO_LINE_JOIN_BEVEL or CAIRO_LINE_JOIN_ROUND
	
	
	m_outputSink << "style=\"";

	double width = 1.0 / 72.0;
	if (m_style["svg:stroke-width"])
	{
		width = getInchValue(*m_style["svg:stroke-width"]);
#if 0
		// add me in libmspub and libcdr
		if (width <= 0.0 && m_style["draw:stroke"] && m_style["draw:stroke"]->getStr() != "none")
			width = 0.2 / 72.0; // reasonable hairline
#endif
		m_outputSink << "stroke-width: " << doubleToString(72*width) << "; ";
		
		strokeWidth = 72*width;
		strokeFlag = 1;
		
	}

	if (m_style["draw:stroke"] && m_style["draw:stroke"]->getStr() != "none")
	{
		strokeFlag = 1;
		
		if (m_style["svg:stroke-color"])
		{
			m_outputSink << "stroke: " << m_style["svg:stroke-color"]->getStr().cstr()  << "; ";
			unsigned strokeColour = stringToColour(m_style["svg:stroke-color"]->getStr());
			strokeColorR = (double)((strokeColour & 0x00ff0000) >> 16)/255.0;
			strokeColorG =(double)((strokeColour & 0x0000ff00) >> 8)/255.0;
			strokeColorB = (double)(strokeColour & 0x000000ff)/255.0;
			
		}
		if (m_style["svg:stroke-opacity"] &&  m_style["svg:stroke-opacity"]->getInt()!= 1)
		{
			m_outputSink << "stroke-opacity: " << doubleToString(m_style["svg:stroke-opacity"]->getDouble()) << "; ";
			strokeOpacity = m_style["svg:stroke-opacity"]->getDouble();
		}
	}

	if (m_style["draw:stroke"] && m_style["draw:stroke"]->getStr() == "solid")
	{
		
		m_outputSink << "stroke-dasharray: none; ";
		
		strokeFlag = 1;
	}
	else if (m_style["draw:stroke"] && m_style["draw:stroke"]->getStr() == "dash")
	{
		strokeFlag = 1;
		
		int dots1 = m_style["draw:dots1"] ? m_style["draw:dots1"]->getInt() : 0;
		int dots2 = m_style["draw:dots2"] ? m_style["draw:dots2"]->getInt() : 0;
		double dots1len = 72.*width, dots2len = 72.*width, gap = 72.*width;
		if (m_style["draw:dots1-length"])
		{
			if (m_style["draw:dots1-length"]->getUnit()==librevenge::RVNG_PERCENT)
				dots1len=72*m_style["draw:dots1-length"]->getDouble()*width;
			else
				dots1len=72*getInchValue(*m_style["draw:dots1-length"]);
		}
		if (m_style["draw:dots2-length"])
		{
			if (m_style["draw:dots2-length"]->getUnit()==librevenge::RVNG_PERCENT)
				dots2len=72*m_style["draw:dots2-length"]->getDouble()*width;
			else
				dots2len=72*getInchValue(*m_style["draw:dots2-length"]);
		}
		if (m_style["draw:distance"])
		{
			if (m_style["draw:distance"]->getUnit()==librevenge::RVNG_PERCENT)
				gap=72*m_style["draw:distance"]->getDouble()*width;
			else
				gap=72*getInchValue(*m_style["draw:distance"]);
		}
		m_outputSink << "stroke-dasharray: ";
		for (int i = 0; i < dots1; i++)
		{
			if (i)
				m_outputSink << ", ";
			m_outputSink << doubleToString(dots1len);
			
			m_outputSink << ", ";
			m_outputSink << doubleToString(gap);
			
			
			//strokeDashPatternVec.push_back (dots1len);
			++strokeNumDashes; 
			//strokeDashPatternVec.push_back(gap); // it seems we always have a dot then gap
			++strokeNumDashes;
		}
		for (int j = 0; j < dots2; j++)
		{
			m_outputSink << ", ";
			m_outputSink << doubleToString(dots2len);
			m_outputSink << ", ";
			m_outputSink << doubleToString(gap);
			
			strokeDashPatternVec.push_back (dots2len);
			++strokeNumDashes; 
			strokeDashPatternVec.push_back(gap); // it seems we always have a dot then gap
			++strokeNumDashes;
		}
		m_outputSink << "; ";				
	}
	
		

	if (m_style["svg:stroke-linecap"])
	{
		m_outputSink << "stroke-linecap: " << m_style["svg:stroke-linecap"]->getStr().cstr() << "; ";
		
		strokeFlag = 1;
		if (m_style["svg:stroke-linecap"]->getStr() == "round")
			strokeLineCap = CAIRO_LINE_CAP_ROUND;
		else if (m_style["svg:stroke-linecap"]->getStr() == "square")
			strokeLineCap = CAIRO_LINE_CAP_SQUARE;
	}

	if (m_style["svg:stroke-linejoin"])
	{
		m_outputSink << "stroke-linejoin: " << m_style["svg:stroke-linejoin"]->getStr().cstr() << "; ";
		
		strokeFlag = 1;
		if (m_style["svg:stroke-linejoin"]->getStr() == "round")
			strokeLineJoin = CAIRO_LINE_JOIN_MITER;
		else if (m_style["svg:stroke-linejoin"]->getStr() == "bevel")
			strokeLineJoin = CAIRO_LINE_JOIN_BEVEL;
	}

	
	
	if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "none")
	{
		m_outputSink << "fill: none; ";
		
		fillFlag = 0;
	}
	else if (m_style["svg:fill-rule"])
	{
		
		
		m_outputSink << "fill-rule: " << m_style["svg:fill-rule"]->getStr().cstr() << "; ";
		
		
		fillFlag = 1;
		if (m_style["svg:fill-rule"]->getStr() == "evenodd") 
			fillRule = CAIRO_FILL_RULE_EVEN_ODD;
	}

	if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "gradient")
	{
		m_outputSink << "fill: url(#grad" << m_gradientIndex-1 << "); ";		
		//TODO: add fill gradient
		fillFlag = 1;
	}
	else if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "bitmap")
	{
		m_outputSink << "fill: url(#img" << m_patternIndex-1 << "); ";
		// TODO: add fill bitmap
		fillFlag = 1;
	}
	if (m_style["draw:shadow"] && m_style["draw:shadow"]->getStr() == "visible")
	{
		m_outputSink << "filter:url(#shadow" << m_shadowIndex-1 << "); ";
		// TODO: add fill shadow
		fillFlag = 1;
	}

	if (m_style["draw:fill"] && m_style["draw:fill"]->getStr() == "solid")
	{
		fillFlag = 1;
		if (m_style["draw:fill-color"])
		{
			m_outputSink << "fill: " << m_style["draw:fill-color"]->getStr().cstr() << "; ";
			
			unsigned fillColour = stringToColour(m_style["draw:fill-color"]->getStr());
			fillColorR = (double)((fillColour & 0x00ff0000) >> 16)/255.0;
			fillColorG =(double)((fillColour & 0x0000ff00) >> 8)/255.0;
			fillColorB = (double)(fillColour & 0x000000ff)/255.0;
			
		}
	}
	if (m_style["draw:opacity"] && m_style["draw:opacity"]->getDouble() < 1)
	{
		m_outputSink << "fill-opacity: " << doubleToString(m_style["draw:opacity"]->getDouble()) << "; ";
		
		fillFlag = 1;
		fillOpacity = m_style["draw:opacity"]->getDouble();
		
	}

	if (m_style["draw:marker-start-path"])
	{
		m_outputSink << "marker-start: url(#startMarker" << m_arrowStartIndex-1 << "); ";
		// TODO: add marker start
	}
	if (m_style["draw:marker-end-path"])
	{
		m_outputSink << "marker-end: url(#endMarker" << m_arrowEndIndex-1 << "); ";
		// TODO: add marker end
	}

	m_outputSink << "\""; // style
	
	// BEVARA: have set up for either stroke or fill, now draw it
	if (strokeFlag)
	{
		
		
				
		cairo_set_line_width (cr, strokeWidth);
		cairo_set_line_cap  (cr, strokeLineCap);
		cairo_set_line_join  (cr, strokeLineJoin);
		if (strokeNumDashes > 0) // not bothering unless have dashes
			{
			const double *strokeDashPattern = strokeDashPatternVec.data();
			cairo_set_dash (cr,  strokeDashPattern, strokeNumDashes, strokeDashOffset);
			}
		cairo_set_source_rgba (cr, strokeColorR, strokeColorG, strokeColorB, strokeOpacity);
		cairo_stroke_preserve (cr);
	}
	
	if (fillFlag)
	{
		

		cairo_set_source_rgba (cr, fillColorR, fillColorG, fillColorB, fillOpacity);
		// TODO: add remainder of fill info
		cairo_set_fill_rule (cr, fillRule);
		cairo_fill (cr);
	}
	
}


RVNGSVGRGBDrawingGenerator::RVNGSVGRGBDrawingGenerator(RVNGStringVector &vec,  unsigned char *rgbaOutbuf, int *rgbaOutWidth, int *rgbaOutHeight, int *nsFeatures, const RVNGString &nmSpace) :
	m_pImpl(new RVNGSVGRGBDrawingGeneratorPrivate(vec, rgbaOutbuf, rgbaOutWidth, rgbaOutHeight, nsFeatures, nmSpace))
{
}

RVNGSVGRGBDrawingGenerator::~RVNGSVGRGBDrawingGenerator()
{
	delete m_pImpl;
}

void RVNGSVGRGBDrawingGenerator::startDocument(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::endDocument() {}
void RVNGSVGRGBDrawingGenerator::setDocumentMetaData(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::defineEmbeddedFont(const RVNGPropertyList & /*propList*/) {}

void RVNGSVGRGBDrawingGenerator::startPage(const RVNGPropertyList &propList)
{
	if (propList["librevenge:master-page-name"])
	{
		if (m_pImpl->m_masterNameToContentMap.find(propList["librevenge:master-page-name"]->getStr())!=
		        m_pImpl->m_masterNameToContentMap.end())
		{
			m_pImpl->m_outputSink << m_pImpl->m_masterNameToContentMap.find(propList["librevenge:master-page-name"]->getStr())->second;
			return;
		}
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::startPage: can not find page with given master name\n"));
	}

	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "svg version=\"1.1\" xmlns";
	m_pImpl->m_outputSink << (m_pImpl->m_nmSpace.empty() ? "" : ":") << m_pImpl->m_nmSpace << "=\"http://www.w3.org/2000/svg\" ";
	m_pImpl->m_outputSink << "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ";
	if (propList["svg:width"])
		m_pImpl->m_outputSink << "width=\"" << doubleToString(72*getInchValue(*propList["svg:width"])) << "\" ";
	if (propList["svg:height"])
		m_pImpl->m_outputSink << "height=\"" << doubleToString(72*getInchValue(*propList["svg:height"])) << "\"";
	m_pImpl->m_outputSink << " >\n";
	
	// BEVARA: make a new Cairo surface and init it
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 72*getInchValue(*propList["svg:width"]), 72*getInchValue(*propList["svg:height"]));
    cr = cairo_create (surface);
	// set surface to white
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_paint (cr);
}

void RVNGSVGRGBDrawingGenerator::endPage()
{
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "svg>\n";
	m_pImpl->m_vec.append(m_pImpl->m_outputSink.str().c_str());
	m_pImpl->m_outputSink.str("");
	
	// BEVARA: on endPage write out the surface
	// init out variables, just in case
	*(m_pImpl->m_rgbOutWidth) = 0;
	*(m_pImpl->m_rgbOutHeight) = 0;
	int imgHeight, imgWidth;
	unsigned char *outImg;
	
	#ifdef BEV_DBG
		printf("heading to write surface\n");
	#endif
	
	//Bevara: TODO
	// This is an absurd malloc, we need to pass in the m_rgbaOutput and not rely on local copies
	outImg = (unsigned char *) malloc(10000*10000*4);
	// temporary end of absurdity
	
	
	cairo_write_surface_rgba(surface, &imgHeight, &imgWidth, outImg);

	#ifdef BEV_DBG
		printf("post surface write imgH=%d imgW= %d going into loc %d\n",imgHeight, imgWidth,m_pImpl->m_rgbaOutput);
	#endif
	
	// we have the RBGA image in local space, copy over
	*(m_pImpl->m_rgbOutWidth) = imgWidth;
	*(m_pImpl->m_rgbOutHeight) = imgHeight;
	memcpy(m_pImpl->m_rgbaOutput,outImg,imgWidth*imgHeight*4);
	
	#ifdef BEV_DBG
		printf("post copy from local \n");
		printf("results of feature test = 0x%02x \n",*(m_pImpl->m_nsFeatures));
	#endif
	
	// get rid of the Cairo surface
	cairo_destroy (cr);       
    cairo_surface_destroy (surface);
}

void RVNGSVGRGBDrawingGenerator::startMasterPage(const RVNGPropertyList &propList)
{
	
	
	if (!m_pImpl->m_masterName.empty())
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::startMasterPage: a master page is already opened\n"));
	}
	else if (propList["librevenge:master-page-name"])
	{
		m_pImpl->m_masterName=propList["librevenge:master-page-name"]->getStr();
		RVNGPropertyList pList(propList);
		pList.remove("librevenge:master-page-name");
		startPage(pList);
	}
	else
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::startMasterPage: can not find the master name\n"));
	}
}

void RVNGSVGRGBDrawingGenerator::endMasterPage()
{
	if (m_pImpl->m_masterName.empty())
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::endMasterPage: no master pages are opened\n"));
	}
	else
	{
		if (m_pImpl->m_masterNameToContentMap.find(m_pImpl->m_masterName)!=m_pImpl->m_masterNameToContentMap.end())
		{
			RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::endMasterPage: a master page with name %s already exists\n",
			                m_pImpl->m_masterName.cstr()));
		}
		// no need to close the page, this will be done when the master page will be used
		m_pImpl->m_masterNameToContentMap[m_pImpl->m_masterName]=m_pImpl->m_outputSink.str();
		m_pImpl->m_masterName.clear();
	}
	// reset the content
	m_pImpl->m_outputSink.str("");
}

void RVNGSVGRGBDrawingGenerator::startLayer(const RVNGPropertyList &propList)
{
	
	//BEVARA: TODO impl layer
	
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "g";
	librevenge::RVNGString layer("Layer");
	if (propList["draw:layer"])
		layer.append(propList["draw:layer"]->getStr());
	else if (propList["svg:id"])
		layer.append(propList["svg:id"]->getStr());
	else
		layer.sprintf("Layer%d", m_pImpl->m_layerId++);
	librevenge::RVNGString finalName("");
	finalName.appendEscapedXML(layer);
	m_pImpl->m_outputSink << " id=\"" << finalName.cstr() << "\"";
	if (propList["svg:fill-rule"])
		m_pImpl->m_outputSink << " fill-rule=\"" << propList["svg:fill-rule"]->getStr().cstr() << "\"";
	m_pImpl->m_outputSink << " >\n";
	
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_LAYER;
}

void RVNGSVGRGBDrawingGenerator::endLayer()
{
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "g>\n";

		//BEVARA: TODO handle layer
		*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_LAYER;
	
}

void RVNGSVGRGBDrawingGenerator::startEmbeddedGraphics(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::endEmbeddedGraphics() {}

void RVNGSVGRGBDrawingGenerator::openGroup(const RVNGPropertyList & /*propList*/)
{
	
	//BEVARA:TODO
	
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "g";
	librevenge::RVNGString group;
	group.sprintf("Group%d", m_pImpl->m_groupId++);
	m_pImpl->m_outputSink << " id=\"" << group.cstr() << "\"";
	m_pImpl->m_outputSink << " >\n";
}

void RVNGSVGRGBDrawingGenerator::closeGroup()
{
	//BEVARA:TODO
	
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "g>\n";
}

void RVNGSVGRGBDrawingGenerator::setStyle(const RVNGPropertyList &propList)
{
	m_pImpl->setStyle(propList);
}

void RVNGSVGRGBDrawingGenerator::drawRectangle(const RVNGPropertyList &propList)
{
	
	//BEVARA:TODO -- not needed for CDR
	
	if (!propList["svg:x"] || !propList["svg:y"] || !propList["svg:width"] || !propList["svg:height"])
		return;
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "rect ";
	m_pImpl->m_outputSink << "x=\"" << doubleToString(72*getInchValue(*propList["svg:x"])) << "\" y=\"" << doubleToString(72*getInchValue(*propList["svg:y"])) << "\" ";
	m_pImpl->m_outputSink << "width=\"" << doubleToString(72*getInchValue(*propList["svg:width"])) << "\" height=\"" << doubleToString(72*getInchValue(*propList["svg:height"])) << "\" ";
	if (propList["svg:rx"] && propList["svg:rx"]->getDouble() > 0 && propList["svg:ry"] && propList["svg:ry"]->getDouble()>0)
		m_pImpl->m_outputSink << "rx=\"" << doubleToString(72*getInchValue(*propList["svg:rx"])) << "\" ry=\"" << doubleToString(72*getInchValue(*propList["svg:ry"])) << "\" ";
	m_pImpl->writeStyle();
	m_pImpl->m_outputSink << "/>\n";
}

void RVNGSVGRGBDrawingGenerator::drawEllipse(const RVNGPropertyList &propList)
{
	
	//BEVARA:TODO --not needed for CDR?
	
	if (!propList["svg:cx"] || !propList["svg:cy"] || !propList["svg:rx"] || !propList["svg:ry"])
		return;
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "ellipse ";
	m_pImpl->m_outputSink << "cx=\"" << doubleToString(72*getInchValue(*propList["svg:cx"])) << "\" cy=\"" << doubleToString(72*getInchValue(*propList["svg:cy"])) << "\" ";
	m_pImpl->m_outputSink << "rx=\"" << doubleToString(72*getInchValue(*propList["svg:rx"])) << "\" ry=\"" << doubleToString(72*getInchValue(*propList["svg:ry"])) << "\" ";
	m_pImpl->writeStyle();
	if (propList["librevenge:rotate"] && (propList["librevenge:rotate"]->getDouble()<0||propList["librevenge:rotate"]->getDouble()>0))
		m_pImpl->m_outputSink << " transform=\" rotate(" << doubleToString(-propList["librevenge:rotate"]->getDouble())
		                      << ", " << doubleToString(72*getInchValue(*propList["svg:cx"]))
		                      << ", " << doubleToString(72*getInchValue(*propList["svg:cy"]))
		                      << ")\" ";
	m_pImpl->m_outputSink << "/>\n";
}

void RVNGSVGRGBDrawingGenerator::drawPolyline(const RVNGPropertyList &propList)
{
	
	//BEVARA: TODO add drawPath -- not needed for CDR 
	const RVNGPropertyListVector *vertices = propList.child("svg:points");
	if (vertices && vertices->count())
		m_pImpl->drawPolySomething(*vertices, false);
}

void RVNGSVGRGBDrawingGenerator::drawPolygon(const RVNGPropertyList &propList)
{
	//BEVARA: TODO add drawPath -- not needed for CDR
	
	const RVNGPropertyListVector *vertices = propList.child("svg:points");
	if (vertices && vertices->count())
		m_pImpl->drawPolySomething(*vertices, true);
}

void RVNGSVGRGBDrawingGenerator::drawPath(const RVNGPropertyList &propList)
{
	//BEVARA:TODO
	
	
	const RVNGPropertyListVector *path = propList.child("svg:d");
	if (!path)
		return;
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "path d=\" ";
	
	// BEVARA: start new cairo path; draw commands will interspersed below
	cairo_new_path(cr);
	// BEVARA: for elements like H & V we need to record the current X & Y
	double currX = 0.0;
	double currY = 0.0;
	// BEVARA: for some elements we need the previous X & Y as a starting point
	double prevX = -1.0;
	double prevY = -1.0;
	// BEVARA: record the previous control point
	double currXctl = 0.0;
	double currYctl = 0.0;
	// BEVARA: need to also track previous command
	std::string prevAction ("M");
	
	bool isClosed = false;
	unsigned i=0;
	for (i=0; i < path->count(); i++)
	{
		RVNGPropertyList pList((*path)[i]);
		if (!pList["librevenge:path-action"]) continue;
		std::string action=pList["librevenge:path-action"]->getStr().cstr();
		if (action.length()!=1) continue;
		bool coordOk=pList["svg:x"]&&pList["svg:y"];
		bool coord1Ok=coordOk && pList["svg:x1"]&&pList["svg:y1"];
		bool coord2Ok=coord1Ok && pList["svg:x2"]&&pList["svg:y2"];
		if (pList["svg:x"] && action[0] == 'H')
		{
			m_pImpl->m_outputSink << "\nH" << doubleToString(72*getInchValue(*pList["svg:x"]));
			currX = 72*getInchValue(*pList["svg:x"]);
			cairo_line_to(cr, currX, currY);
			//prevAction ="H";
		}
		else if (pList["svg:y"] && action[0] == 'V')
		{
			m_pImpl->m_outputSink << "\nV" << doubleToString(72*getInchValue(*pList["svg:y"]));
			currY = 72*getInchValue(*pList["svg:y"]);
			cairo_line_to(cr, currX, currY);
			//prevAction ="V";
			
		}
		else if (coordOk && (action[0] == 'M' || action[0] == 'L' || action[0] == 'T'))
		{
			m_pImpl->m_outputSink << "\n" << action;
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x"])) << "," << doubleToString(72*getInchValue(*pList["svg:y"]));
			
			currX = 72*getInchValue(*pList["svg:x"]);
			currY = 72*getInchValue(*pList["svg:y"]);
			
			if (action[0] == 'M') // move to 
			{
				cairo_move_to (cr, currX, currY);
				//prevAction ="M";
			}
			else if (action[0] == 'L') // line to 
			{
				cairo_line_to (cr, currX, currY);
				//prevAction ="L";
				
			}
				
			else if (action[0] == 'T') // quadratic Bezier shortcut
			{

			//Bevara: TODO
			printf("\n\n  SVG T command TBA\n\n");
			//prevAction ="T";
			
			}
			
		}
		else if (coord1Ok && (action[0] == 'Q' || action[0] == 'S'))
		{
			m_pImpl->m_outputSink << "\n" << action;
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x1"])) << "," << doubleToString(72*getInchValue(*pList["svg:y1"])) << " ";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x"])) << "," << doubleToString(72*getInchValue(*pList["svg:y"]));
			
			if (action[0] == 'Q') // quadratic, needs conversion
			{
			// Note: from Cairo listserv, turn the quadratic points into the cubic Bezier supported by Cairo
			// To turn quad(p0, p1, p2) into cubic(q0, q1, q2, q3):

			// q0 = p0
			// q1 = (p0+2*p1)/3
			// q2 = (p2+2*p1)/3
			// q3 = p2

			// Ascii diagram, q1 and q2 are 2/3 the distance along the lines:

			// p0         p2
			 // \         /
			  // \       /
			   // \     /
				// q1  q2
				 // \ /
				  // p1
				  
				  
				  
			// Bevara: curve
			
			// double p0X =	currX;  //start point is previous end point
			// double p0Y = 	currY;
			
			// double p1X = 72*getInchValue(*pList["svg:x1"]); //  quad control point
			// double p1Y = 72*getInchValue(*pList["svg:x1"]); 
			
			// double q1X = (p0X+2*p1X)/3; // first cubic control
			// double q1Y = (p0Y+2*p1Y)/3;
			
			// currX = 72*getInchValue(*pList["svg:x"]); // new end point
			// currY = 72*getInchValue(*pList["svg:y"]);
			
			// double q2X = (currX+2*p1X)/3; // second cubic control
			// double q2Y = (currY+2*p1Y)/3;
					 
			// cairo_curve_to (cr,  q1X, q1Y, q2X, q2Y, currX, currY);
			
			// prevAction ="Q";
				  
			}
			else if (action[0] == 'S')
			{
				//Bevara: TODO
				printf("\n\n  SVG S command TBA\n\n");
				//prevAction ="S";
			}
				
			
		}
		else if (coord2Ok && action[0] == 'C')
		{
			m_pImpl->m_outputSink << "\nC";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x1"])) << "," << doubleToString(72*getInchValue(*pList["svg:y1"])) << " ";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x2"])) << "," << doubleToString(72*getInchValue(*pList["svg:y2"])) << " ";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x"])) << "," << doubleToString(72*getInchValue(*pList["svg:y"]));
			
			// Bevara: curve
			currX = 72*getInchValue(*pList["svg:x"]);
			currY = 72*getInchValue(*pList["svg:y"]);
			cairo_curve_to (cr, 72*getInchValue(*pList["svg:x1"]), 72*getInchValue(*pList["svg:y1"]),  72*getInchValue(*pList["svg:x2"]), 72*getInchValue(*pList["svg:y2"]), currX, currY);
			//prevAction ="C";
			
		}
		else if (coordOk && pList["svg:rx"] && pList["svg:ry"] && action[0] == 'A')
		{
			m_pImpl->m_outputSink << "\nA";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:rx"])) << "," << doubleToString(72*getInchValue(*pList["svg:ry"])) << " ";
			m_pImpl->m_outputSink << doubleToString(pList["librevenge:rotate"] ? pList["librevenge:rotate"]->getDouble() : 0) << " ";
			m_pImpl->m_outputSink << (pList["librevenge:large-arc"] ? pList["librevenge:large-arc"]->getInt() : 1) << ",";
			m_pImpl->m_outputSink << (pList["librevenge:sweep"] ? pList["librevenge:sweep"]->getInt() : 1) << " ";
			m_pImpl->m_outputSink << doubleToString(72*getInchValue(*pList["svg:x"])) << "," << doubleToString(72*getInchValue(*pList["svg:y"]));
			
			// arc conversion is a bit tricky - extract the SVG params first			
			prevX = currX; //starting point
			prevY = currY; 
			currX = 72*getInchValue(*pList["svg:x"]); //ending point
			currY = 72*getInchValue(*pList["svg:y"]);
			double rx = 72*getInchValue(*pList["svg:rx"]); // radius 
			double ry = 72*getInchValue(*pList["svg:ry"]);
			double xrot = (pList["librevenge:rotate"] ? pList["librevenge:rotate"]->getDouble() : 0); // xaxis rot
			double largeArcFlag = (pList["librevenge:large-arc"] ? pList["librevenge:large-arc"]->getInt() : 1);
			double sweepFlag = (pList["librevenge:sweep"] ? pList["librevenge:sweep"]->getInt() : 1);
			// convert to Cairo form and transform to make elliptical
			
			// draw the arc
			arcViaTransform(prevX,prevY,currX,currY,rx,ry,xrot, sweepFlag, largeArcFlag );
			cairo_move_to (cr, currX, currY);			
			//prevAction ="A";
			
		}
		else if (action[0] == 'Z')
		{
			isClosed = true;
			m_pImpl->m_outputSink << "\nZ";
			cairo_close_path(cr);
			//prevAction ="Z";
		}
	}

	m_pImpl->m_outputSink << "\" \n";	
	m_pImpl->writeStyle(isClosed);
	m_pImpl->m_outputSink << "/>\n";
}

void RVNGSVGRGBDrawingGenerator::drawGraphicObject(const RVNGPropertyList &propList)
{
	
	//BEVARA:TODO
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_GRAPHIC;
	
	if (!propList["librevenge:mime-type"] || propList["librevenge:mime-type"]->getStr().len() <= 0)
		return;
	if (!propList["office:binary-data"])
		return;
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "image ";
	if (propList["svg:x"] && propList["svg:y"] && propList["svg:width"] && propList["svg:height"])
	{
		double x=getInchValue(*propList["svg:x"]);
		double y=getInchValue(*propList["svg:y"]);
		double width=getInchValue(*propList["svg:width"]);
		double height=getInchValue(*propList["svg:height"]);
		bool flipX(propList["draw:mirror-horizontal"] && propList["draw:mirror-horizontal"]->getInt());
		bool flipY(propList["draw:mirror-vertical"] && propList["draw:mirror-vertical"]->getInt());

		m_pImpl->m_outputSink << "x=\"" << doubleToString(72*x) << "\" y=\"" << doubleToString(72*y) << "\" ";
		m_pImpl->m_outputSink << "width=\"" << doubleToString(72*width) << "\" height=\"" << doubleToString(72*height) << "\" ";
		if (flipX || flipY || propList["librevenge:rotate"])
		{
			double xmiddle = x + width / 2.0;
			double ymiddle = y + height / 2.0;
			m_pImpl->m_outputSink << "transform=\"";
			m_pImpl->m_outputSink << " translate(" << doubleToString(72*xmiddle) << ", " << doubleToString(72*ymiddle) << ") ";
			m_pImpl->m_outputSink << " scale(" << (flipX ? "-1" : "1") << ", " << (flipY ? "-1" : "1") << ") ";
			// rotation is around the center of the object's bounding box
			if (propList["librevenge:rotate"])
			{
				double angle(propList["librevenge:rotate"]->getDouble());
				while (angle > 180.0)
					angle -= 360.0;
				while (angle < -180.0)
					angle += 360.0;
				m_pImpl->m_outputSink << " rotate(" << doubleToString(angle) << ") ";
			}
			m_pImpl->m_outputSink << " translate(" << doubleToString(-72*xmiddle) << ", " << doubleToString(-72*ymiddle) << ") ";
			m_pImpl->m_outputSink << "\" ";
		}
	}
	m_pImpl->m_outputSink << "xlink:href=\"data:" << propList["librevenge:mime-type"]->getStr().cstr() << ";base64,";
	m_pImpl->m_outputSink << propList["office:binary-data"]->getStr().cstr();
	m_pImpl->m_outputSink << "\" />\n";
}

void RVNGSVGRGBDrawingGenerator::drawConnector(const RVNGPropertyList &/*propList*/)
{
	// TODO: implement me
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_CONN;
}

void RVNGSVGRGBDrawingGenerator::startTextObject(const RVNGPropertyList &propList)
{
	//BEVARA:TODO
	
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_TEXT;
	
	double x = 0.0;
	double y = 0.0;
	double height = 0.0;
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "text ";
	if (propList["svg:x"] && propList["svg:y"])
	{
		x = getInchValue(*propList["svg:x"]);
		y = getInchValue(*propList["svg:y"]);
	}

	double xmiddle = x;
	double ymiddle = y;

	if (propList["svg:width"])
	{
		double width = getInchValue(*propList["svg:width"]);
		xmiddle += width / 2.0;
	}

	if (propList["svg:height"])
	{
		height = getInchValue(*propList["svg:height"]);
		ymiddle += height / 2.0;
	}

	if (propList["draw:textarea-vertical-align"])
	{
		if (propList["draw:textarea-vertical-align"]->getStr() == "middle")
			y = ymiddle;
		if (propList["draw:textarea-vertical-align"]->getStr() == "bottom")
		{
			y += height;
			if (propList["fo:padding-bottom"])
				y -= propList["fo:padding-bottom"]->getDouble();
		}
	}
	else
		y += height;

	if (propList["fo:padding-left"])
		x += propList["fo:padding-left"]->getDouble();

	m_pImpl->m_outputSink << "x=\"" << doubleToString(72*x) << "\" y=\"" << doubleToString(72*y) << "\"";

	// rotation is around the center of the object's bounding box
	if (propList["librevenge:rotate"] && (propList["librevenge:rotate"]->getDouble()<0||propList["librevenge:rotate"]->getDouble()>0))
	{
		double angle(propList["librevenge:rotate"]->getDouble());
		while (angle > 180.0)
			angle -= 360.0;
		while (angle < -180.0)
			angle += 360.0;
		m_pImpl->m_outputSink << " transform=\"rotate(" << doubleToString(angle) << ", " << doubleToString(72*xmiddle) << ", " << doubleToString(72*ymiddle) << ")\" ";
	}
	m_pImpl->m_outputSink << ">\n";

}

void RVNGSVGRGBDrawingGenerator::endTextObject()
{
	
	//BEVARA:TODO
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "text>\n";
}

void RVNGSVGRGBDrawingGenerator::openOrderedListLevel(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::closeOrderedListLevel() {}

void RVNGSVGRGBDrawingGenerator::openUnorderedListLevel(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::closeUnorderedListLevel() {}

void RVNGSVGRGBDrawingGenerator::openListElement(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::closeListElement() {}

void RVNGSVGRGBDrawingGenerator::defineParagraphStyle(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::openParagraph(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::closeParagraph() {}

void RVNGSVGRGBDrawingGenerator::defineCharacterStyle(const RVNGPropertyList &propList)
{
	if (!propList["librevenge:span-id"])
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::defineCharacterStyle: can not find the span-id\n"));
		return;
	}
	m_pImpl->m_idSpanMap[propList["librevenge:span-id"]->getInt()]=propList;
}

void RVNGSVGRGBDrawingGenerator::openSpan(const RVNGPropertyList &propList)
{
	
	//BEVARA:TODO
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_TEXT;
	
	
	RVNGPropertyList pList(propList);
	if (propList["librevenge:span-id"] &&
	        m_pImpl->m_idSpanMap.find(propList["librevenge:span-id"]->getInt())!=m_pImpl->m_idSpanMap.end())
		pList=m_pImpl->m_idSpanMap.find(propList["librevenge:span-id"]->getInt())->second;

	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "tspan ";
	if (pList["style:font-name"])
		m_pImpl->m_outputSink << "font-family=\"" << pList["style:font-name"]->getStr().cstr() << "\" ";
	if (pList["fo:font-style"])
		m_pImpl->m_outputSink << "font-style=\"" << pList["fo:font-style"]->getStr().cstr() << "\" ";
	if (pList["fo:font-weight"])
		m_pImpl->m_outputSink << "font-weight=\"" << pList["fo:font-weight"]->getStr().cstr() << "\" ";
	if (pList["fo:font-variant"])
		m_pImpl->m_outputSink << "font-variant=\"" << pList["fo:font-variant"]->getStr().cstr() << "\" ";
	if (pList["fo:font-size"])
		m_pImpl->m_outputSink << "font-size=\"" << doubleToString(pList["fo:font-size"]->getDouble()) << "\" ";
	if (pList["fo:color"])
		m_pImpl->m_outputSink << "fill=\"" << pList["fo:color"]->getStr().cstr() << "\" ";
	if (pList["fo:text-transform"])
		m_pImpl->m_outputSink << "text-transform=\"" << pList["fo:text-transform"]->getStr().cstr() << "\" ";
	if (pList["svg:fill-opacity"])
		m_pImpl->m_outputSink << "fill-opacity=\"" << doubleToString(pList["svg:fill-opacity"]->getDouble()) << "\" ";
	if (pList["svg:stroke-opacity"])
		m_pImpl->m_outputSink << "stroke-opacity=\"" << doubleToString(pList["svg:stroke-opacity"]->getDouble()) << "\" ";
	m_pImpl->m_outputSink << ">\n";
}

void RVNGSVGRGBDrawingGenerator::closeSpan()
{
	
	//BEVARA:TODO
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "tspan>\n";
}

void RVNGSVGRGBDrawingGenerator::openLink(const RVNGPropertyList & /*propList*/) {}
void RVNGSVGRGBDrawingGenerator::closeLink() {}

void RVNGSVGRGBDrawingGenerator::insertText(const RVNGString &str)
{
	//BEVARA:TODO
	m_pImpl->m_outputSink << RVNGString::escapeXML(str).cstr();
}

void RVNGSVGRGBDrawingGenerator::insertTab()
{
	//BEVARA:TODO
	m_pImpl->m_outputSink << "\t";
}

void RVNGSVGRGBDrawingGenerator::insertSpace()
{
	//BEVARA:TODO
	m_pImpl->m_outputSink << " ";
}

void RVNGSVGRGBDrawingGenerator::insertLineBreak()
{
	//BEVARA:TODO
	m_pImpl->m_outputSink << "\n";
}

void RVNGSVGRGBDrawingGenerator::insertField(const RVNGPropertyList & /*propList*/) {}

void RVNGSVGRGBDrawingGenerator::startTableObject(const RVNGPropertyList &propList)
{
	
	*(m_pImpl->m_nsFeatures) ^= BEV_CDR_NO_RGB_TABLE;
	if (m_pImpl->m_table)
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::startTableObject: a table is already opened\n"));
		return;
	}
	m_pImpl->m_table.reset(new Table(propList));
}

void RVNGSVGRGBDrawingGenerator::openTableRow(const RVNGPropertyList &propList)
{
	if (!m_pImpl->m_table) return;
	m_pImpl->m_table->openRow(propList);
}

void RVNGSVGRGBDrawingGenerator::closeTableRow()
{
	if (!m_pImpl->m_table) return;
	m_pImpl->m_table->closeRow();
}

void RVNGSVGRGBDrawingGenerator::openTableCell(const RVNGPropertyList &propList)
{
	
	//BEVARA:TODO
	*(m_pImpl->m_nsFeatures) |= BEV_CDR_NO_RGB_TABLE;
	if (!m_pImpl->m_table) return;

	if (propList["librevenge:column"])
		m_pImpl->m_table->m_column=propList["librevenge:column"]->getInt();
	if (propList["librevenge:row"])
		m_pImpl->m_table->m_row=propList["librevenge:row"]->getInt();

	double x = 0, y=0;
	m_pImpl->m_table->getPosition(m_pImpl->m_table->m_column, m_pImpl->m_table->m_row, x, y);
	m_pImpl->m_outputSink << "<" << m_pImpl->getNamespaceAndDelim() << "text ";
	m_pImpl->m_outputSink << "x=\"" << doubleToString(72*x) << "\" y=\"" << doubleToString(72*y) << "\"";
	m_pImpl->m_outputSink << ">\n";

	// time to update the next cell's column
	if (propList["table:number-columns-spanned"])
		m_pImpl->m_table->m_column += propList["librevenge:column"]->getInt();
	else
		++m_pImpl->m_table->m_column;
}

void RVNGSVGRGBDrawingGenerator::closeTableCell()
{
	//BEVARA:TODO
	if (!m_pImpl->m_table) return;
	m_pImpl->m_outputSink << "</" << m_pImpl->getNamespaceAndDelim() << "text>\n";
}

void RVNGSVGRGBDrawingGenerator::insertCoveredTableCell(const RVNGPropertyList &/*propList*/)
{
	if (!m_pImpl->m_table) return;
	// TODO: implement me
}

void RVNGSVGRGBDrawingGenerator::endTableObject()
{
	if (!m_pImpl->m_table)
	{
		RVNG_DEBUG_MSG(("RVNGSVGDrawingGenerator::endTableObject: no table is already opened\n"));
		return;
	}
	m_pImpl->m_table.reset();
}

}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
