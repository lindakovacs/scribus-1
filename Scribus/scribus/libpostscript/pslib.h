/***************************************************************************
                          pslib.h  -  description
                             -------------------
    begin                : Sat May 26 2001
    copyright            : (C) 2001 by Franz Schmid
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

#ifndef PSLIB_H
#define PSLIB_H
#include <qstring.h>
#include <qcstring.h>
#include <qpen.h>
#include <qfile.h>
#include "scribusdoc.h"

/**
  *@author Franz Schmid
  * Diese Klasse erzeugt Postscript-Dateien
  */

class PSLib {
public:
	PSLib(bool psart, SCFonts &AllFonts, QMap<QString,QFont> DocFonts, CListe DocColors, bool pdf = false);
	virtual ~PSLib() {};
	virtual bool PS_set_file(QString fn);
	virtual void PS_set_Info(QString art, QString was);
	virtual void PS_begin_doc(int Ori, double breite, double hoehe, int numpage);
	virtual void PS_begin_page(double breite, double hoehe, struct Margs* Ma);
	virtual void PS_end_page();
	virtual void PS_curve(double x1, double y1, double x2, double y2, double x3, double y3);
	virtual void PS_moveto(double x, double y);
	virtual void PS_lineto(double x, double y);
	virtual void PS_closepath();
	virtual void PS_translate(double x, double y);
	virtual void PS_scale(double x, double y);
	virtual void PS_rotate(double x);
	virtual void PS_clip(bool mu);
	virtual void PS_save();
	virtual void PS_restore();
	virtual void PS_setcmykcolor_fill(double c, double m, double y, double k);
	virtual void PS_setcmykcolor_dummy();
	virtual void PS_setcmykcolor_stroke(double c, double m, double y, double k);
	virtual void PS_setlinewidth(double w);
	virtual void PS_setdash(Qt::PenStyle st, Qt::PenCapStyle ca, Qt::PenJoinStyle jo);
	virtual void PS_selectfont(QString f, int s);
	virtual void PS_fill(bool mu);
	virtual void PS_stroke();
	virtual void PS_fill_stroke(bool mu);
	virtual void PS_newpath();
	virtual void PS_GradientCol1(double c, double m, double y, double k);
	virtual void PS_GradientCol2(double c, double m, double y, double k);
	virtual void PS_LinGradient(double w, double h, int item, int grad, bool mu);
	virtual void PS_RadGradient(double w, double h, int item, bool mu);
	virtual void PS_show(double x, double y);
	virtual void PS_showSub(uint chr, QString font, int size, bool stroke);
	virtual void PS_show_xyG(QString font, QString ch, double x, double y);
	virtual void PS_image(bool inver, double x, double y, QString fn, double scalex, double scaley, QString Prof, bool UseEmbedded, bool UseProf, QString Name = "");
	virtual void PS_plate(int nr);
	virtual void PS_setGray();
	virtual void PDF_Bookmark(QString text, uint Seite);
	virtual void PDF_Annotation(QString text, double x, double y, double b, double h);
	virtual void PS_close();
	virtual void PS_insert(QString i);
	virtual void PS_TemplateStart(QString Name, double breite, double hoehe);
	virtual void PS_TemplateEnd();
	virtual void PS_UseTemplate(QString Name);
	virtual void PS_ImageData(bool inver, QString fn, QString Name, QString Prof, bool UseEmbedded, bool UseProf);
private:
	void PutSeite(QString c);
	void PutDoc(QString c);
	QString ToStr(double c);
	QString IToStr(int c);
	QString PSEncode(QString in);
	bool Art;
	QString Prolog;
	QString Header;
	QString Creator;
	QString User;
	QString Titel;
	QString BBox;
	QString BBoxH;
	QString Farben;
	QString FNamen;
	QString PDev;
	QString GrayCalc;
	bool GraySc;
	int Seiten;
	QString FillColor;
	QString StrokeColor;
	QString GrColor1;
	QString GrColor2;
	double LineW;
	QString Fonts;
	QString FontDesc;
	QMap<QString, QString> UsedFonts;
	typedef QMap<uint, QString> GListe;
	QMap<QString, GListe> GlyphsOfFont;
	bool isPDF;
	QFile Spool;
	bool CompAvail;
	int Plate;
	bool DoSep;
};

#endif
