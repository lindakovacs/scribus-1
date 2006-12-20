/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#include <qwidget.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qregexp.h>
#include <qcursor.h>
#include <qdragobject.h>
#include <qdir.h>
#include <qstring.h>
#include <qdom.h>
#include <cmath>

#include "oodrawimp.h"
#include "oodrawimp.moc"

#include "scconfig.h"

#include "scribuscore.h"
#include "scribusdoc.h"
#include "pageitem.h"
#include "fpointarray.h"
#include "customfdialog.h"
#include "commonstrings.h"
#include "color.h"
#include "scribusXml.h"
#include "mpalette.h"
#include "prefsmanager.h"
#include "prefsfile.h"
#include "prefscontext.h"
#include "prefstable.h"
#include "fileunzip.h"
#include "selection.h"
#include "serializer.h"
#include "undomanager.h"
#include "pluginmanager.h"
#include "util.h"
#include "stylestack.h"
#include "scraction.h"
#include "menumanager.h"
#include "sccolorengine.h"

using namespace std;

int oodrawimp_getPluginAPIVersion()
{
	return PLUGIN_API_VERSION;
}

ScPlugin* oodrawimp_getPlugin()
{
	OODrawImportPlugin* plug = new OODrawImportPlugin();
	Q_CHECK_PTR(plug);
	return plug;
}

void oodrawimp_freePlugin(ScPlugin* plugin)
{
	OODrawImportPlugin* plug = dynamic_cast<OODrawImportPlugin*>(plugin);
	Q_ASSERT(plug);
	delete plug;
}

OODrawImportPlugin::OODrawImportPlugin() :
	LoadSavePlugin(),
	importAction(new ScrAction(ScrAction::DLL, QIconSet(), "", QKeySequence(), this, "ImportOpenOfficeDraw"))
{
	// Set action info in languageChange, so we only have to do
	// it in one place. This includes registering file formats.
	languageChange();
}

void OODrawImportPlugin::addToMainWindowMenu(ScribusMainWindow *mw)
{
	// Then hook up the action
	importAction->setEnabled(true);
	connect( importAction, SIGNAL(activated()), SLOT(import()) );
	mw->scrMenuMgr->addMenuItem(importAction, "FileImport");
}

OODrawImportPlugin::~OODrawImportPlugin()
{
	unregisterAll();
	// note: importAction is automatically deleted by Qt
};

void OODrawImportPlugin::languageChange()
{
	importAction->setMenuText( tr("Import &OpenOffice.org Draw..."));
	// (Re)register file formats
	unregisterAll();
	registerFormats();
}

const QString OODrawImportPlugin::fullTrName() const
{
	return QObject::tr("OpenOffice.org Draw Importer");
}

const ScActionPlugin::AboutData* OODrawImportPlugin::getAboutData() const
{
	AboutData* about = new AboutData;
	about->authors = "Franz Schmid <franz@scribus.info>";
	about->shortDescription = tr("Imports OpenOffice.org Draw Files");
	about->description = tr("Imports most OpenOffice.org Draw files into the current document, converting their vector data into Scribus objects.");
	about->license = "GPL";
	Q_CHECK_PTR(about);
	return about;
}

void OODrawImportPlugin::deleteAboutData(const AboutData* about) const
{
	Q_ASSERT(about);
	delete about;
}

void OODrawImportPlugin::registerFormats()
{
	QString odtName = tr("OpenDocument 1.0 Draw", "Import/export format name");
	FileFormat odtformat(this);
	odtformat.trName = odtName; // Human readable name
	odtformat.formatId = FORMATID_ODGIMPORT;
	odtformat.filter = odtName + " (*.odg *.ODG)"; // QFileDialog filter
	odtformat.nameMatch = QRegExp("\\.odg$", false);
	odtformat.load = true;
	odtformat.save = false;
	odtformat.mimeTypes = QStringList("application/vnd.oasis.opendocument.graphics"); // MIME types
	odtformat.priority = 64; // Priority
	registerFormat(odtformat);

	QString sxdName = tr("OpenOffice.org 1.x Draw", "Import/export format name");
	FileFormat sxdformat(this);
	sxdformat.trName = sxdName; // Human readable name
	sxdformat.formatId = FORMATID_SXDIMPORT;
	sxdformat.filter = sxdName + " (*.sxd *.SXD)"; // QFileDialog filter
	sxdformat.nameMatch = QRegExp("\\.sxd$", false);
	sxdformat.load = true;
	sxdformat.save = false;
	sxdformat.mimeTypes = QStringList("application/vnd.sun.xml.draw"); // MIME types
	sxdformat.priority = 64; // Priority
	registerFormat(sxdformat);
}

bool OODrawImportPlugin::fileSupported(QIODevice* /* file */, const QString & fileName) const
{
	// TODO: try to identify .sxd / .odt files
	return true;
}

bool OODrawImportPlugin::loadFile(const QString & fileName, const FileFormat &, int flags, int /*index*/)
{
	// For this plugin, right now "load" and "import" are the same thing
	return import(fileName, flags);
}

bool OODrawImportPlugin::import(QString fileName, int flags)
{
	if (!checkFlags(flags))
		return false;
	if (fileName.isEmpty())
	{
		flags |= lfInteractive;
		PrefsContext* prefs = PrefsManager::instance()->prefsFile->getPluginContext("OODrawImport");
		QString wdir = prefs->get("wdir", ".");
		CustomFDialog diaf(ScCore->primaryMainWindow(), wdir, QObject::tr("Open"), QObject::tr("OpenOffice.org Draw (*.sxd *.odg);;All Files (*)"));
		if (diaf.exec())
		{
			fileName = diaf.selectedFile();
			prefs->set("wdir", fileName.left(fileName.findRev("/")));
		}
		else
			return true;
	}
	m_Doc=ScCore->primaryMainWindow()->doc;
	if (UndoManager::undoEnabled() && m_Doc)
	{
		UndoManager::instance()->beginTransaction(m_Doc->currentPage()->getUName(),
													Um::IImageFrame,
													Um::ImportOOoDraw,
													fileName, Um::IImportOOoDraw);
	}
	else if (UndoManager::undoEnabled() && !m_Doc)
		UndoManager::instance()->setUndoEnabled(false);
	OODPlug dia(m_Doc);
	bool importDone = dia.import(fileName, flags);
	if (UndoManager::undoEnabled())
		UndoManager::instance()->commit();
	else
		UndoManager::instance()->setUndoEnabled(true);
	if (dia.unsupported)
		QMessageBox::warning(ScCore->primaryMainWindow(), CommonStrings::trWarning, tr("This file contains some unsupported features"), 1, 0, 0);
	return importDone;
}

OODPlug::OODPlug(ScribusDoc* doc)
{
	m_Doc=doc;
	unsupported = false;
	interactive = false;
}

bool OODPlug::import( QString fileName, int flags )
{
	bool importDone = false;
	interactive = (flags & LoadSavePlugin::lfInteractive);
	QString f, f2, f3;
	if ( !QFile::exists(fileName) )
		return false;
	m_styles.setAutoDelete( true );
	FileUnzip* fun = new FileUnzip(fileName);
	stylePath   = fun->getFile("styles.xml");
	contentPath = fun->getFile("content.xml");
	metaPath = fun->getFile("meta.xml");
	delete fun;
	if ((stylePath != NULL) && (contentPath != NULL))
	{
		QString docname = fileName.right(fileName.length() - fileName.findRev("/") - 1);
		docname = docname.left(docname.findRev("."));
		loadText(stylePath, &f);
		if(!inpStyles.setContent(f))
			return false;
		loadText(contentPath, &f2);
		if(!inpContents.setContent(f2))
			return false;
		QFile f1(stylePath);
		f1.remove();
		QFile f2(contentPath);
		f2.remove();
		if (metaPath != NULL)
		{
			HaveMeta = true;
			loadText(metaPath, &f3);
			if(!inpMeta.setContent(f3))
				HaveMeta = false;
			QFile f3(metaPath);
			f3.remove();
		}
		else
			HaveMeta = false;
	}
	else if ((stylePath == NULL) && (contentPath != NULL))
	{
		QFile f2(contentPath);
		f2.remove();
	}
	else if ((stylePath != NULL) && (contentPath == NULL))
	{
		QFile f1(stylePath);
		f1.remove();
	}
	QString CurDirP = QDir::currentDirPath();
	QFileInfo efp(fileName);
	QDir::setCurrent(efp.dirPath());
	importDone = convert(flags);
	QDir::setCurrent(CurDirP);
	return importDone;
}

bool OODPlug::convert(int flags)
{
	bool ret = false;
	bool isOODraw2 = false;
	QDomNode drawPagePNode;
	int PageCounter = 0;
	createStyleMap( inpStyles );
	QDomElement docElem = inpContents.documentElement();
	QDomNode automaticStyles = docElem.namedItem( "office:automatic-styles" );
	if( !automaticStyles.isNull() )
		insertStyles( automaticStyles.toElement() );
	QDomNode body = docElem.namedItem( "office:body" );
	QDomNode drawPage = body.namedItem( "draw:page" );
	if ( drawPage.isNull() )
	{
		QDomNode offDraw = body.namedItem( "office:drawing" );
		drawPage = offDraw.namedItem( "draw:page" );
		if (drawPage.isNull())
		{
			QMessageBox::warning( m_Doc->scMW(), CommonStrings::trWarning, tr("This document does not seem to be an OpenOffice Draw file.") );
			return false;
		}
		else
		{
			isOODraw2 = true;
			drawPagePNode = body.namedItem( "office:drawing" );
		}
	}
	else 
		drawPagePNode = body;
	StyleStack::Mode mode = isOODraw2 ? StyleStack::OODraw2x : StyleStack::OODraw1x;
	m_styleStack.setMode( mode );
	QDomElement dp = drawPage.toElement();
	QDomElement *master = m_styles[dp.attribute( "draw:master-page-name" )];
	QDomElement *style = NULL;
	QDomElement properties;
	if (isOODraw2)
	{
		style = m_styles[master->attribute( "style:page-layout-name" )];
		properties = style->namedItem( "style:page-layout-properties" ).toElement();
	}
	else
	{
		style = m_styles[master->attribute( "style:page-master-name" )];
		properties = style->namedItem( "style:properties" ).toElement();
	}
	double width = !properties.attribute( "fo:page-width" ).isEmpty() ? parseUnit(properties.attribute( "fo:page-width" ) ) : 550.0;
	double height = !properties.attribute( "fo:page-height" ).isEmpty() ? parseUnit(properties.attribute( "fo:page-height" ) ) : 841.0;
	if (!interactive || (flags & LoadSavePlugin::lfInsertPage))
		m_Doc->setPage(width, height, 0, 0, 0, 0, 0, 0, false, false);
	else
	{
		if (!m_Doc || (flags & LoadSavePlugin::lfCreateDoc))
		{
			m_Doc=ScCore->primaryMainWindow()->doFileNew(width, height, 0, 0, 0, 0, 0, 0, false, false, 0, false, 0, 1, "Custom", true);
			ScCore->primaryMainWindow()->HaveNewDoc();
			ret = true;
		}
	}
	if ((ret) || (!interactive))
	{
		if (width > height)
			m_Doc->PageOri = 1;
		else
			m_Doc->PageOri = 0;
		m_Doc->m_pageSize = "Custom";
		QDomNode mpg;
		QDomElement metaElem = inpMeta.documentElement();
		QDomElement mp = metaElem.namedItem( "office:meta" ).toElement();
		mpg = mp.namedItem( "dc:title" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setTitle(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "meta:initial-creator" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setAuthor(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "dc:description" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setComments(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "dc:language" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setLangInfo(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "meta:creation-date" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setDate(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "dc:creator" );
		if (!mpg.isNull())
			m_Doc->documentInfo.setContrib(QString::fromUtf8(mpg.toElement().text()));
		mpg = mp.namedItem( "meta:keywords" );
		if (!mpg.isNull())
		{
			QString Keys = "";
			for( QDomNode n = mpg.firstChild(); !n.isNull(); n = n.nextSibling() )
			{
				Keys += QString::fromUtf8(n.toElement().text())+", ";
			}
			if (Keys.length() > 2)
				m_Doc->documentInfo.setKeywords(Keys.left(Keys.length()-2));
		}
	}
	FPoint minSize = m_Doc->minCanvasCoordinate;
	FPoint maxSize = m_Doc->maxCanvasCoordinate;
	m_Doc->view()->Deselect();
	Elements.clear();
	m_Doc->setLoading(true);
	m_Doc->DoDrawing = false;
	m_Doc->view()->updatesOn(false);
	m_Doc->scMW()->ScriptRunning = true;
	qApp->setOverrideCursor(QCursor(Qt::waitCursor), true);
	if (!m_Doc->PageColors.contains("Black"))
		m_Doc->PageColors.insert("Black", ScColor(0, 0, 0, 255));
	for( QDomNode drawPag = drawPagePNode.firstChild(); !drawPag.isNull(); drawPag = drawPag.nextSibling() )
	{
		QDomElement dpg = drawPag.toElement();
		if (!interactive)
		{
			m_Doc->addPage(PageCounter);
			m_Doc->view()->addPage(PageCounter);
		}
		PageCounter++;
		m_styleStack.clear();
		fillStyleStack( dpg );
		parseGroup( dpg );
		if ((interactive) && (PageCounter == 1))
			break;
	}
	m_Doc->m_Selection->clear();
	if ((Elements.count() > 1) && (interactive))
	{
		bool isGroup = true;
		int firstElem = -1;
		if (Elements.at(0)->Groups.count() != 0)
			firstElem = Elements.at(0)->Groups.top();
		for (uint bx = 0; bx < Elements.count(); ++bx)
		{
			PageItem* bxi = Elements.at(bx);
			if (bxi->Groups.count() != 0)
			{
				if (bxi->Groups.top() != firstElem)
					isGroup = false;
			}
			else
				isGroup = false;
		}
		if (!isGroup)
		{
			for (uint a = 0; a < Elements.count(); ++a)
			{
				Elements.at(a)->Groups.push(m_Doc->GroupCounter);
			}
			m_Doc->GroupCounter++;
		}
	}
	m_Doc->DoDrawing = true;
	m_Doc->scMW()->ScriptRunning = false;
	if (interactive)
		m_Doc->setLoading(false);
	qApp->setOverrideCursor(QCursor(Qt::arrowCursor), true);
	if ((Elements.count() > 0) && (!ret) && (interactive))
	{
		m_Doc->DragP = true;
		m_Doc->DraggedElem = 0;
		m_Doc->DragElements.clear();
		for (uint dre=0; dre<Elements.count(); ++dre)
		{
			m_Doc->DragElements.append(Elements.at(dre)->ItemNr);
			m_Doc->m_Selection->addItem(Elements.at(dre), true);
		}
		ScriXmlDoc *ss = new ScriXmlDoc();
		m_Doc->view()->setGroupRect();
		QDragObject *dr = new QTextDrag(ss->WriteElem(m_Doc, m_Doc->view(), m_Doc->m_Selection), m_Doc->view()->viewport());
#ifndef QT_MAC
// see #2196, #2526
		m_Doc->itemSelection_DeleteItem();
#endif
		m_Doc->view()->resizeContents(qRound((maxSize.x() - minSize.x()) * m_Doc->view()->scale()), qRound((maxSize.y() - minSize.y()) * m_Doc->view()->scale()));
		m_Doc->view()->scrollBy(qRound((m_Doc->minCanvasCoordinate.x() - minSize.x()) * m_Doc->view()->scale()), qRound((m_Doc->minCanvasCoordinate.y() - minSize.y()) * m_Doc->view()->scale()));
		m_Doc->minCanvasCoordinate = minSize;
		m_Doc->maxCanvasCoordinate = maxSize;
		m_Doc->view()->updatesOn(true);
		dr->setPixmap(loadIcon("DragPix.xpm"));
		if (!dr->drag())
			qDebug("oodraw import: couldn't start drag operation!");
		delete ss;
		m_Doc->DragP = false;
		m_Doc->DraggedElem = 0;
		m_Doc->DragElements.clear();
	}
	else
	{
		bool loadF = m_Doc->isLoading();
		m_Doc->setLoading(false);
		m_Doc->changed();
		m_Doc->reformPages();
		m_Doc->view()->updatesOn(true);
		m_Doc->setLoading(loadF);
	}
	return true;
}

QPtrList<PageItem> OODPlug::parseGroup(const QDomElement &e)
{
	QPtrList<PageItem> GElements;
	FPointArray ImgClip;
	ImgClip.resize(0);
	VGradient gradient;
	double GradientAngle = 0;
	double xGoff = 0;
	double yGoff= 0;
	bool HaveGradient = false;
	int GradientType = 0;
	double BaseX = m_Doc->currentPage()->xOffset();
	double BaseY = m_Doc->currentPage()->yOffset();
	double lwidth = 0;
	double x, y, w, h;
	double FillTrans = 0;
	double StrokeTrans = 0;
	QValueList<double> dashes;
	for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling())
	{
		QString StrokeColor = CommonStrings::None;
		QString FillColor = CommonStrings::None;
		FillTrans = 0;
		StrokeTrans = 0;
		int z = -1;
		dashes.clear();
		QDomElement b = n.toElement();
		if( b.isNull() )
			continue;
		QString STag = b.tagName();
		QString drawID = b.attribute("draw:name");
		storeObjectStyles(b);
		if( m_styleStack.hasAttribute("draw:stroke") )
		{
			if( m_styleStack.attribute( "draw:stroke" ) == "none" )
				lwidth = 0.0;
			else
			{
				if( m_styleStack.hasAttribute("svg:stroke-width"))
				{
					lwidth = parseUnit(m_styleStack.attribute("svg:stroke-width"));
					if (lwidth == 0)
						lwidth = 1;
				}
				if( m_styleStack.hasAttribute("svg:stroke-color"))
					StrokeColor = parseColor(m_styleStack.attribute("svg:stroke-color"));
				if( m_styleStack.hasAttribute( "svg:stroke-opacity" ) )
					StrokeTrans = m_styleStack.attribute( "svg:stroke-opacity" ).remove( '%' ).toDouble() / 100.0;
				if( m_styleStack.attribute( "draw:stroke" ) == "dash" )
				{
					QString style = m_styleStack.attribute( "draw:stroke-dash" );
					if( style == "Ultrafine Dashed")
						dashes << 1.4 << 1.4;
					else if( style == "Fine Dashed" )
						dashes << 14.4 << 14.4;
					else if( style == "Fine Dotted")
						dashes << 13 << 13;
					else if( style == "Ultrafine 2 Dots 3 Dashes")
						dashes << 1.45 << 3.6 << 1.45 << 3.6 << 7.2 << 3.6 << 7.2 << 3.6 << 7.2 << 3.6;
					else if( style == "Line with Fine Dots")
					{
						dashes << 56.9 << 4.31;
						for (int dd = 0; dd < 10; ++ dd)
						{
							dashes << 8.6 << 4.31;
						}
					}
					else if( style == "2 Dots 1 Dash" )
						dashes << 2.8 << 5.75 << 2.8 << 5.75 << 5.75 << 5.75;
				}
			}
		}
		if( m_styleStack.hasAttribute( "draw:fill" ) )
		{
			QString fill = m_styleStack.attribute( "draw:fill" );
			if( fill == "solid" )
			{
				if( m_styleStack.hasAttribute( "draw:fill-color" ) )
					FillColor = parseColor( m_styleStack.attribute("draw:fill-color"));
				if( m_styleStack.hasAttribute( "draw:transparency" ) )
					FillTrans = m_styleStack.attribute( "draw:transparency" ).remove( '%' ).toDouble() / 100.0;
			}
			else if( fill == "gradient" )
			{
				HaveGradient = true;
				GradientAngle = 0;
				gradient.clearStops();
				gradient.setRepeatMethod( VGradient::none );
				QString style = m_styleStack.attribute( "draw:fill-gradient-name" );
				QDomElement* draw = m_draws[style];
				if( draw )
				{
					double border = 0.0;
					int shadeS = 100;
					int shadeE = 100;
					if( draw->hasAttribute( "draw:border" ) )
						border += draw->attribute( "draw:border" ).remove( '%' ).toDouble() / 100.0;
					if( draw->hasAttribute( "draw:start-intensity" ) )
						shadeS = draw->attribute( "draw:start-intensity" ).remove( '%' ).toInt();
					if( draw->hasAttribute( "draw:end-intensity" ) )
						shadeE = draw->attribute( "draw:end-intensity" ).remove( '%' ).toInt();
					QString type = draw->attribute( "draw:style" );
					if( type == "linear" || type == "axial" )
					{
						gradient.setType( VGradient::linear );
						GradientAngle = draw->attribute( "draw:angle" ).toDouble() / 10;
						GradientType = 1;
					}
					else if( type == "radial" || type == "ellipsoid" )
					{
						if( draw->hasAttribute( "draw:cx" ) )
							xGoff = draw->attribute( "draw:cx" ).remove( '%' ).toDouble() / 100.0;
						else
							xGoff = 0.5;
						if( draw->hasAttribute( "draw:cy" ) )
							yGoff = draw->attribute( "draw:cy" ).remove( '%' ).toDouble() / 100.0;
						else
							yGoff = 0.5;
						GradientType = 2;
					}
					QString c, c2;
					c = parseColor( draw->attribute( "draw:start-color" ) );
					c2 = parseColor( draw->attribute( "draw:end-color" ) );
					const ScColor& col1 = m_Doc->PageColors[c];
					const ScColor& col2 = m_Doc->PageColors[c2];
					if (((GradientAngle > 90) && (GradientAngle < 271)) || (GradientType == 2))
					{
						const ScColor& col1 = m_Doc->PageColors[c];
						const ScColor& col2 = m_Doc->PageColors[c2];
						gradient.addStop( ScColorEngine::getRGBColor(col2, m_Doc), 0.0, 0.5, 1, c2, shadeE );
						gradient.addStop( ScColorEngine::getRGBColor(col1, m_Doc), 1.0 - border, 0.5, 1, c, shadeS );
					}
					else
					{
						gradient.addStop( ScColorEngine::getRGBColor(col1, m_Doc), border, 0.5, 1, c, shadeS );
						gradient.addStop( ScColorEngine::getRGBColor(col2, m_Doc), 1.0, 0.5, 1, c2, shadeE );
					}
				}
			}
		}
		if( STag == "draw:g" )
		{
			QPtrList<PageItem> gElements = parseGroup( b );
			for (uint gr = 0; gr < gElements.count(); ++gr)
			{
				gElements.at(gr)->Groups.push(m_Doc->GroupCounter);
				GElements.append(gElements.at(gr));
			}
			m_Doc->GroupCounter++;
		}
		else if( STag == "draw:rect" )
		{
			x = parseUnit(b.attribute("svg:x"));
			y = parseUnit(b.attribute("svg:y")) ;
			w = parseUnit(b.attribute("svg:width"));
			h = parseUnit(b.attribute("svg:height"));
			double corner = parseUnit(b.attribute("draw:corner-radius"));
			z = m_Doc->itemAdd(PageItem::Polygon, PageItem::Rectangle, BaseX+x, BaseY+y, w, h, lwidth, FillColor, StrokeColor, true);
			PageItem* ite = m_Doc->Items->at(z);
			if (corner != 0)
			{
				ite->setCornerRadius(corner);
				ite->SetFrameRound();
				m_Doc->setRedrawBounding(ite);
			}
		}
		else if( STag == "draw:circle" || STag == "draw:ellipse" )
		{
			x = parseUnit(b.attribute("svg:x"));
			y = parseUnit(b.attribute("svg:y")) ;
			w = parseUnit(b.attribute("svg:width"));
			h = parseUnit(b.attribute("svg:height"));
			z = m_Doc->itemAdd(PageItem::Polygon, PageItem::Ellipse, BaseX+x, BaseY+y, w, h, lwidth, FillColor, StrokeColor, true);
		}
		else if( STag == "draw:line" ) // line
		{
			double x1 = b.attribute( "svg:x1" ).isEmpty() ? 0.0 : parseUnit( b.attribute( "svg:x1" ) );
			double y1 = b.attribute( "svg:y1" ).isEmpty() ? 0.0 : parseUnit( b.attribute( "svg:y1" ) );
			double x2 = b.attribute( "svg:x2" ).isEmpty() ? 0.0 : parseUnit( b.attribute( "svg:x2" ) );
			double y2 = b.attribute( "svg:y2" ).isEmpty() ? 0.0 : parseUnit( b.attribute( "svg:y2" ) );
			z = m_Doc->itemAdd(PageItem::PolyLine, PageItem::Unspecified, BaseX, BaseY, 10, 10, lwidth, CommonStrings::None, StrokeColor, true);
			PageItem* ite = m_Doc->Items->at(z);
			ite->PoLine.resize(4);
			ite->PoLine.setPoint(0, FPoint(x1, y1));
			ite->PoLine.setPoint(1, FPoint(x1, y1));
			ite->PoLine.setPoint(2, FPoint(x2, y2));
			ite->PoLine.setPoint(3, FPoint(x2, y2));
			FPoint wh = getMaxClipF(&ite->PoLine);
			ite->setWidthHeight(wh.x(), wh.y());
			ite->ClipEdited = true;
			ite->FrameType = 3;
			if (!b.hasAttribute("draw:transform"))
			{
				ite->Clip = FlattenPath(ite->PoLine, ite->Segments);
				m_Doc->view()->AdjustItemSize(ite);
			}
			HaveGradient = false;
		}
		else if ( STag == "draw:polygon" )
		{
			z = m_Doc->itemAdd(PageItem::Polygon, PageItem::Unspecified, BaseX, BaseY, 10, 10, lwidth, FillColor, StrokeColor, true);
			PageItem* ite = m_Doc->Items->at(z);
			ite->PoLine.resize(0);
			appendPoints(&ite->PoLine, b);
			FPoint wh = getMaxClipF(&ite->PoLine);
			ite->setWidthHeight(wh.x(), wh.y());
			ite->ClipEdited = true;
			ite->FrameType = 3;
			if (!b.hasAttribute("draw:transform"))
			{
				ite->Clip = FlattenPath(ite->PoLine, ite->Segments);
				m_Doc->view()->AdjustItemSize(ite);
			}
		}
		else if( STag == "draw:polyline" )
		{
			z = m_Doc->itemAdd(PageItem::PolyLine, PageItem::Unspecified, BaseX, BaseY, 10, 10, lwidth, CommonStrings::None, StrokeColor, true);
			PageItem* ite = m_Doc->Items->at(z);
			ite->PoLine.resize(0);
			appendPoints(&ite->PoLine, b);
			FPoint wh = getMaxClipF(&ite->PoLine);
			ite->setWidthHeight(wh.x(), wh.y());
			ite->ClipEdited = true;
			ite->FrameType = 3;
			if (!b.hasAttribute("draw:transform"))
			{
				ite->Clip = FlattenPath(ite->PoLine, ite->Segments);
				m_Doc->view()->AdjustItemSize(ite);
			}
		}
		else if( STag == "draw:path" )
		{
			FPointArray pArray;
			PageItem::ItemType itype = parseSVG(b.attribute("svg:d"), &pArray) ? PageItem::PolyLine : PageItem::Polygon;
			z = m_Doc->itemAdd(itype, PageItem::Unspecified, BaseX, BaseY, 10, 10, lwidth, FillColor, StrokeColor, true);
			PageItem* ite = m_Doc->Items->at(z);
			ite->PoLine.resize(0);
			ite->PoLine = pArray;
			if (ite->PoLine.size() < 4)
			{
				m_Doc->m_Selection->addItem(ite);
				m_Doc->itemSelection_DeleteItem();
				z = -1;
			}
			else
			{
				x = parseUnit(b.attribute("svg:x"));
				y = parseUnit(b.attribute("svg:y")) ;
				w = parseUnit(b.attribute("svg:width"));
				h = parseUnit(b.attribute("svg:height"));
				double vx = 0;
				double vy = 0;
				double vw = 1;
				double vh = 1;
				parseViewBox(b, &vx, &vy, &vw, &vh);
				QWMatrix mat;
				mat.translate(x, y);
				mat.scale(w / vw, h / vh);
				ite->PoLine.map(mat);
				FPoint wh = getMaxClipF(&ite->PoLine);
				ite->setWidthHeight(wh.x(), wh.y());
				ite->ClipEdited = true;
				ite->FrameType = 3;
				if (!b.hasAttribute("draw:transform"))
				{
					ite->Clip = FlattenPath(ite->PoLine, ite->Segments);
					m_Doc->view()->AdjustItemSize(ite);
				}
			}
		}
		else if ( STag == "draw:text-box" )
		{
			x = parseUnit(b.attribute("svg:x"));
			y = parseUnit(b.attribute("svg:y")) ;
			w = parseUnit(b.attribute("svg:width"));
			h = parseUnit(b.attribute("svg:height"));
			z = m_Doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified, BaseX+x, BaseY+y, w, h+(h*0.1), lwidth, CommonStrings::None, StrokeColor, true);
		}
		else if ( STag == "draw:frame" )
		{
			x = parseUnit(b.attribute("svg:x"));
			y = parseUnit(b.attribute("svg:y")) ;
			w = parseUnit(b.attribute("svg:width"));
			h = parseUnit(b.attribute("svg:height"));
			QDomNode n = b.firstChild();
			QString STag2 = n.toElement().tagName();
			if ( STag2 == "draw:text-box" )
			{
				z = m_Doc->itemAdd(PageItem::TextFrame, PageItem::Unspecified, BaseX+x, BaseY+y, w, h+(h*0.1), lwidth, CommonStrings::None, StrokeColor, true);
				PageItem* ite = m_Doc->Items->at(z);
				ite->setTextToFrameDist(0.0, 0.0, 0.0, 0.0);
				ite->setFillTransparency(FillTrans);
				ite->setLineTransparency(StrokeTrans);
				if (!drawID.isEmpty())
					ite->setItemName(drawID);
				GElements.append(ite);
				Elements.append(ite);
				bool firstPa = false;
				for ( QDomNode n2 = n.firstChild(); !n2.isNull(); n2 = n2.nextSibling() )
				{
					if ( !n2.hasAttributes() && !n2.hasChildNodes() )
						continue;
					QDomElement e = n2.toElement();
					if ( e.text().isEmpty() )
						continue;
					storeObjectStyles(n2.toElement());
					int FontSize = m_Doc->toolSettings.defSize;
					int AbsStyle = 0;
/*					if( m_styleStack.hasAttribute("fo:text-align"))
					{
						if (m_styleStack.attribute("fo:text-align") == "left")
							AbsStyle = 0;
						if (m_styleStack.attribute("fo:text-align") == "center")
							AbsStyle = 1;
						if (m_styleStack.attribute("fo:text-align") == "right")
							AbsStyle = 2;
					} */
					if( m_styleStack.hasAttribute("fo:font-size"))
						FontSize = m_styleStack.attribute("fo:font-size").remove( "pt" ).toInt();
					Serializer *ss = new Serializer("");
					ss->Objekt = QString::fromUtf8(e.text())+QChar(10);
					ss->GetText(ite, AbsStyle, m_Doc->toolSettings.defFont, FontSize*10, firstPa);
					delete ss;
					firstPa = true;
				}
			}
			z = -1;
		}
		else
		{
			unsupported = true;
			qDebug("Not supported yet: %s", STag.local8Bit().data());
			continue;
		}
		if (z != -1)
		{
			PageItem* ite = m_Doc->Items->at(z);
			ite->setTextToFrameDist(0.0, 0.0, 0.0, 0.0);
			bool firstPa = false;
			for ( QDomNode n = b.firstChild(); !n.isNull(); n = n.nextSibling() )
			{
				if ( !n.hasAttributes() && !n.hasChildNodes() )
					continue;
				QDomElement e = n.toElement();
				if ( e.text().isEmpty() )
					continue;
				int FontSize = m_Doc->toolSettings.defSize;
				int AbsStyle = 0;
/*				if( m_styleStack.hasAttribute("fo:text-align"))
				{
					if (m_styleStack.attribute("fo:text-align") == "left")
						AbsStyle = 0;
					if (m_styleStack.attribute("fo:text-align") == "center")
						AbsStyle = 1;
					if (m_styleStack.attribute("fo:text-align") == "right")
						AbsStyle = 2;
				} */
				if( m_styleStack.hasAttribute("fo:font-size"))
				{
					FontSize = m_styleStack.attribute("fo:font-size").remove( "pt" ).toInt();
				}
/* ToDo: Add reading of Textstyles here */
//FIXME:av				ite->setLineSpacing(FontSize + FontSize * 0.2);
				Serializer *ss = new Serializer("");
				ss->Objekt = QString::fromUtf8(e.text())+QChar(10);
				ss->GetText(ite, AbsStyle, m_Doc->toolSettings.defFont, FontSize*10, firstPa);
				delete ss;
				firstPa = true;
				if (! ite->asPolyLine())
					ite->convertTo(PageItem::TextFrame);
			}
			ite->setFillTransparency(FillTrans);
			ite->setLineTransparency(StrokeTrans);
			if (dashes.count() != 0)
				ite->DashValues = dashes;
			if (!drawID.isEmpty())
				ite->setItemName(drawID);
			if (b.hasAttribute("draw:transform"))
			{
				parseTransform(&ite->PoLine, b.attribute("draw:transform"));
				ite->ClipEdited = true;
				ite->FrameType = 3;
				FPoint wh = getMaxClipF(&ite->PoLine);
				ite->setWidthHeight(wh.x(), wh.y());
				ite->Clip = FlattenPath(ite->PoLine, ite->Segments);
				m_Doc->view()->AdjustItemSize(ite);
			}
			ite->OwnPage = m_Doc->OnPage(ite);
			if (HaveGradient)
			{
				ite->GrType = 0;
				if (gradient.Stops() > 1)
				{
					ite->fill_gradient = gradient;
					if (GradientType == 1)
					{
						bool flipped = false;
						if ((GradientAngle == 0) || (GradientAngle == 180) || (GradientAngle == 90) || (GradientAngle == 270))
						{
							if ((GradientAngle == 0) || (GradientAngle == 180))
							{
								ite->GrType = 2;
								//m_Doc->view()->updateGradientVectors(ite);
								ite->updateGradientVectors();
							}
							else if ((GradientAngle == 90) || (GradientAngle == 270))
							{
								ite->GrType = 1;
								//m_Doc->view()->updateGradientVectors(ite);
								ite->updateGradientVectors();
							}
						}
						else
						{
							if ((GradientAngle > 90) && (GradientAngle < 270))
								GradientAngle -= 180;
							else if ((GradientAngle > 270) && (GradientAngle < 360))
							{
								GradientAngle = 360 - GradientAngle;
								flipped = true;
							}
							double xpos;
							xpos = (ite->width() / 2) * tan(GradientAngle* M_PI / 180.0) * (ite->height() / ite->width()) + (ite->width() / 2);
							if ((xpos < 0) || (xpos > ite->width()))
							{
								xpos = (ite->height() / 2)- (ite->height() / 2) * tan(GradientAngle* M_PI / 180.0) * (ite->height() / ite->width());
								if (flipped)
								{
									ite->GrEndX = ite->width();
									ite->GrEndY = ite->height() - xpos;
									ite->GrStartX = 0;
									ite->GrStartY = xpos;
								}
								else
								{
									ite->GrEndY = xpos;
									ite->GrEndX = ite->width();
									ite->GrStartX = 0;
									ite->GrStartY = ite->height() - xpos;
								}
							}
							else
							{
								ite->GrEndX = xpos;
								ite->GrEndY = ite->height();
								ite->GrStartX = ite->width() - xpos;
								ite->GrStartY = 0;
							}
							if (flipped)
							{
								ite->GrEndX = ite->width() - xpos;
								ite->GrEndY = ite->height();
								ite->GrStartX = xpos;
								ite->GrStartY = 0;
							}
							ite->GrType = 6;
						}
					}
					if (GradientType == 2)
					{
						ite->GrType = 7;
						ite->GrStartX = ite->width() * xGoff;
						ite->GrStartY = ite->height()* yGoff;
						if (ite->width() >= ite->height())
						{
							ite->GrEndX = ite->width();
							ite->GrEndY = ite->height() / 2.0;
						}
						else
						{
							ite->GrEndX = ite->width() / 2.0;
							ite->GrEndY = ite->height();
						}
						//m_Doc->view()->updateGradientVectors(ite);
						ite->updateGradientVectors();
					}
				}
				else
				{
					QPtrVector<VColorStop> cstops = gradient.colorStops();
					ite->setFillColor(cstops.at(0)->name);
					ite->setFillShade(cstops.at(0)->shade);
				}
				HaveGradient = false;
			}
			GElements.append(ite);
			Elements.append(ite);
		}
	}
	return GElements;
}

void OODPlug::createStyleMap( QDomDocument &docstyles )
{
	QDomElement styles = docstyles.documentElement();
	if( styles.isNull() )
		return;

	QDomNode fixedStyles = styles.namedItem( "office:styles" );
	if( !fixedStyles.isNull() )
	{
		insertDraws( fixedStyles.toElement() );
		insertStyles( fixedStyles.toElement() );
	}
	QDomNode automaticStyles = styles.namedItem( "office:automatic-styles" );
	if( !automaticStyles.isNull() )
		insertStyles( automaticStyles.toElement() );

	QDomNode masterStyles = styles.namedItem( "office:master-styles" );
	if( !masterStyles.isNull() )
		insertStyles( masterStyles.toElement() );
}

void OODPlug::insertDraws( const QDomElement& styles )
{
	for( QDomNode n = styles.firstChild(); !n.isNull(); n = n.nextSibling() )
	{
		QDomElement e = n.toElement();
		if( !e.hasAttribute( "draw:name" ) )
			continue;
		QString name = e.attribute( "draw:name" );
		m_draws.insert( name, new QDomElement( e ) );
	}
}

void OODPlug::insertStyles( const QDomElement& styles )
{
	for ( QDomNode n = styles.firstChild(); !n.isNull(); n = n.nextSibling() )
	{
		QDomElement e = n.toElement();
		if( !e.hasAttribute( "style:name" ) )
			continue;
		QString name = e.attribute( "style:name" );
		m_styles.insert( name, new QDomElement( e ) );
	}
}

void OODPlug::fillStyleStack( const QDomElement& object )
{
	if( object.hasAttribute( "presentation:style-name" ) )
		addStyles( m_styles[object.attribute( "presentation:style-name" )] );
	if( object.hasAttribute( "draw:style-name" ) )
		addStyles( m_styles[object.attribute( "draw:style-name" )] );
	if( object.hasAttribute( "draw:text-style-name" ) )
		addStyles( m_styles[object.attribute( "draw:text-style-name" )] );
	if( object.hasAttribute( "text:style-name" ) )
		addStyles( m_styles[object.attribute( "text:style-name" )] );
}

void OODPlug::addStyles( const QDomElement* style )
{
	if( style->hasAttribute( "style:parent-style-name" ) )
		addStyles( m_styles[style->attribute( "style:parent-style-name" )] );
	m_styleStack.push( *style );
}

void OODPlug::storeObjectStyles( const QDomElement& object )
{
	fillStyleStack( object );
}

double OODPlug::parseUnit(const QString &unit)
{
	QString unitval=unit;
	if (unit.isEmpty())
		return 0.0;
	if( unit.right( 2 ) == "pt" )
		unitval.replace( "pt", "" );
	else if( unit.right( 2 ) == "cm" )
		unitval.replace( "cm", "" );
	else if( unit.right( 2 ) == "mm" )
		unitval.replace( "mm" , "" );
	else if( unit.right( 2 ) == "in" )
		unitval.replace( "in", "" );
	else if( unit.right( 2 ) == "px" )
		unitval.replace( "px", "" );
	double value = unitval.toDouble();
	if( unit.right( 2 ) == "pt" )
		value = value;
	else if( unit.right( 2 ) == "cm" )
		value = ( value / 2.54 ) * 72;
	else if( unit.right( 2 ) == "mm" )
		value = ( value / 25.4 ) * 72;
	else if( unit.right( 2 ) == "in" )
		value = value * 72;
	else if( unit.right( 2 ) == "px" )
		value = value;
	return value;
}

QColor OODPlug::parseColorN( const QString &rgbColor )
{
	int r, g, b;
	keywordToRGB( rgbColor, r, g, b );
	return QColor( r, g, b );
}

QString OODPlug::parseColor( const QString &s )
{
	QColor c;
	QString ret = CommonStrings::None;
	if( s.startsWith( "rgb(" ) )
	{
		QString parse = s.stripWhiteSpace();
		QStringList colors = QStringList::split( ',', parse );
		QString r = colors[0].right( ( colors[0].length() - 4 ) );
		QString g = colors[1];
		QString b = colors[2].left( ( colors[2].length() - 1 ) );
		if( r.contains( "%" ) )
		{
			r = r.left( r.length() - 1 );
			r = QString::number( static_cast<int>( ( static_cast<double>( 255 * r.toDouble() ) / 100.0 ) ) );
		}
		if( g.contains( "%" ) )
		{
			g = g.left( g.length() - 1 );
			g = QString::number( static_cast<int>( ( static_cast<double>( 255 * g.toDouble() ) / 100.0 ) ) );
		}
		if( b.contains( "%" ) )
		{
			b = b.left( b.length() - 1 );
			b = QString::number( static_cast<int>( ( static_cast<double>( 255 * b.toDouble() ) / 100.0 ) ) );
		}
		c = QColor(r.toInt(), g.toInt(), b.toInt());
	}
	else
	{
		QString rgbColor = s.stripWhiteSpace();
		if( rgbColor.startsWith( "#" ) )
			c.setNamedColor( rgbColor );
		else
			c = parseColorN( rgbColor );
	}
	ColorList::Iterator it;
	bool found = false;
	int r, g, b;
	QColor tmpR;
	for (it = m_Doc->PageColors.begin(); it != m_Doc->PageColors.end(); ++it)
	{
		m_Doc->PageColors[it.key()].getRGB(&r, &g, &b);
		tmpR.setRgb(r, g, b);
		if (c == tmpR && m_Doc->PageColors[it.key()].getColorModel() == colorModelRGB)
		{
			ret = it.key();
			found = true;
		}
	}
	if (!found)
	{
		ScColor tmp;
		tmp.fromQColor(c);
		m_Doc->PageColors.insert("FromOODraw"+c.name(), tmp);
		m_Doc->scMW()->propertiesPalette->updateColorList();
		ret = "FromOODraw"+c.name();
	}
	return ret;
}

void OODPlug::parseTransform(FPointArray *composite, const QString &transform)
{
	double dx, dy;
	QWMatrix result;
	QStringList subtransforms = QStringList::split(')', transform);
	QStringList::ConstIterator it = subtransforms.begin();
	QStringList::ConstIterator end = subtransforms.end();
	for (; it != end; ++it)
	{
		QStringList subtransform = QStringList::split('(', (*it));
		subtransform[0] = subtransform[0].stripWhiteSpace().lower();
		subtransform[1] = subtransform[1].simplifyWhiteSpace();
		QRegExp reg("[,( ]");
		QStringList params = QStringList::split(reg, subtransform[1]);
		if(subtransform[0].startsWith(";") || subtransform[0].startsWith(","))
			subtransform[0] = subtransform[0].right(subtransform[0].length() - 1);
		if(subtransform[0] == "rotate")
		{
			result = QWMatrix();
			result.rotate(-parseUnit(params[0]) * 180 / M_PI);
			composite->map(result);
		}
		else if(subtransform[0] == "translate")
		{
			if(params.count() == 2)
			{
				dx = parseUnit(params[0]);
				dy = parseUnit(params[1]);
			}
			else
			{
				dx = parseUnit(params[0]);
				dy =0.0;
			}
			result = QWMatrix();
			result.translate(dx, dy);
			composite->map(result);
		}
		else if(subtransform[0] == "skewx")
		{
			result = QWMatrix();
			result.shear(-tan(params[0].toDouble()), 0.0);
			composite->map(result);
		}
		else if(subtransform[0] == "skewy")
		{
			result = QWMatrix();
			result.shear(0.0, -tan(params[0].toDouble()));
			composite->map(result);
		}
	}
}

void OODPlug::parseViewBox( const QDomElement& object, double *x, double *y, double *w, double *h )
{
	if( !object.attribute( "svg:viewBox" ).isEmpty() )
	{
		QString viewbox( object.attribute( "svg:viewBox" ) );
		QStringList points = QStringList::split( ' ', viewbox.replace( QRegExp(","), " ").simplifyWhiteSpace() );
		*x = points[0].toDouble();
		*y = points[1].toDouble();
		*w = points[2].toDouble();
		*h = points[3].toDouble();
	}
}

void OODPlug::appendPoints(FPointArray *composite, const QDomElement& object)
{
	double x = parseUnit(object.attribute("svg:x"));
	double y = parseUnit(object.attribute("svg:y")) ;
	double w = parseUnit(object.attribute("svg:width"));
	double h = parseUnit(object.attribute("svg:height"));
	double vx = 0;
	double vy = 0;
	double vw = 1;
	double vh = 1;
	parseViewBox(object, &vx, &vy, &vw, &vh);
	QStringList ptList = QStringList::split( ' ', object.attribute( "draw:points" ) );
	FPoint point, firstP;
	bool bFirst = true;
	for( QStringList::Iterator it = ptList.begin(); it != ptList.end(); ++it )
	{
		point = FPoint((*it).section( ',', 0, 0 ).toDouble(), (*it).section( ',', 1, 1 ).toDouble());
		if (bFirst)
		{
			composite->addPoint(point);
			composite->addPoint(point);
			firstP = point;
			bFirst = false;
		}
		else
		{
			composite->addPoint(point);
			composite->addPoint(point);
			composite->addPoint(point);
			composite->addPoint(point);
		}
    }
	composite->addPoint(firstP);
	composite->addPoint(firstP);
	QWMatrix mat;
	mat.translate(x, y);
	mat.scale(w / vw, h / vh);
	composite->map(mat);
}

const char * OODPlug::getCoord( const char *ptr, double &number )
{
	int integer, exponent;
	double decimal, frac;
	int sign, expsign;

	exponent = 0;
	integer = 0;
	frac = 1.0;
	decimal = 0;
	sign = 1;
	expsign = 1;

	// read the sign
	if(*ptr == '+')
		ptr++;
	else if(*ptr == '-')
	{
		ptr++;
		sign = -1;
	}

	// read the integer part
	while(*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
		integer = (integer * 10) + *(ptr++) - '0';
	if(*ptr == '.') // read the decimals
	{
		ptr++;
		while(*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
			decimal += (*(ptr++) - '0') * (frac *= 0.1);
	}

	if(*ptr == 'e' || *ptr == 'E') // read the exponent part
	{
		ptr++;

		// read the sign of the exponent
		if(*ptr == '+')
			ptr++;
		else if(*ptr == '-')
		{
			ptr++;
			expsign = -1;
		}

		exponent = 0;
		while(*ptr != '\0' && *ptr >= '0' && *ptr <= '9')
		{
			exponent *= 10;
			exponent += *ptr - '0';
			ptr++;
		}
	}
	number = integer + decimal;
	number *= sign * pow( static_cast<double>(10), static_cast<double>( expsign * exponent ) );

	// skip the following space
	if(*ptr == ' ')
		ptr++;

	return ptr;
}

bool OODPlug::parseSVG( const QString &s, FPointArray *ite )
{
	QString d = s;
	d = d.replace( QRegExp( "," ), " ");
	bool ret = false;
	if( !d.isEmpty() )
	{
		d = d.simplifyWhiteSpace();
		const char *ptr = d.latin1();
		const char *end = d.latin1() + d.length() + 1;
		double contrlx, contrly, curx, cury, subpathx, subpathy, tox, toy, x1, y1, x2, y2, xc, yc;
		double px1, py1, px2, py2, px3, py3;
		bool relative;
		FirstM = true;
		char command = *(ptr++), lastCommand = ' ';
		subpathx = subpathy = curx = cury = contrlx = contrly = 0.0;
		while( ptr < end )
		{
			if( *ptr == ' ' )
				ptr++;
			relative = false;
			switch( command )
			{
			case 'm':
				relative = true;
			case 'M':
				{
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					WasM = true;
					subpathx = curx = relative ? curx + tox : tox;
					subpathy = cury = relative ? cury + toy : toy;
					svgMoveTo(curx, cury );
					break;
				}
			case 'l':
				relative = true;
			case 'L':
				{
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					curx = relative ? curx + tox : tox;
					cury = relative ? cury + toy : toy;
					svgLineTo(ite, curx, cury );
					break;
				}
			case 'h':
				{
					ptr = getCoord( ptr, tox );
					curx = curx + tox;
					svgLineTo(ite, curx, cury );
					break;
				}
			case 'H':
				{
					ptr = getCoord( ptr, tox );
					curx = tox;
					svgLineTo(ite, curx, cury );
					break;
				}
			case 'v':
				{
					ptr = getCoord( ptr, toy );
					cury = cury + toy;
					svgLineTo(ite, curx, cury );
					break;
				}
			case 'V':
				{
					ptr = getCoord( ptr, toy );
					cury = toy;
					svgLineTo(ite,  curx, cury );
					break;
				}
			case 'z':
			case 'Z':
				{
//					curx = subpathx;
//					cury = subpathy;
					svgClosePath(ite);
					break;
				}
			case 'c':
				relative = true;
			case 'C':
				{
					ptr = getCoord( ptr, x1 );
					ptr = getCoord( ptr, y1 );
					ptr = getCoord( ptr, x2 );
					ptr = getCoord( ptr, y2 );
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					px1 = relative ? curx + x1 : x1;
					py1 = relative ? cury + y1 : y1;
					px2 = relative ? curx + x2 : x2;
					py2 = relative ? cury + y2 : y2;
					px3 = relative ? curx + tox : tox;
					py3 = relative ? cury + toy : toy;
					svgCurveToCubic(ite, px1, py1, px2, py2, px3, py3 );
					contrlx = relative ? curx + x2 : x2;
					contrly = relative ? cury + y2 : y2;
					curx = relative ? curx + tox : tox;
					cury = relative ? cury + toy : toy;
					break;
				}
			case 's':
				relative = true;
			case 'S':
				{
					ptr = getCoord( ptr, x2 );
					ptr = getCoord( ptr, y2 );
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					px1 = 2 * curx - contrlx;
					py1 = 2 * cury - contrly;
					px2 = relative ? curx + x2 : x2;
					py2 = relative ? cury + y2 : y2;
					px3 = relative ? curx + tox : tox;
					py3 = relative ? cury + toy : toy;
					svgCurveToCubic(ite, px1, py1, px2, py2, px3, py3 );
					contrlx = relative ? curx + x2 : x2;
					contrly = relative ? cury + y2 : y2;
					curx = relative ? curx + tox : tox;
					cury = relative ? cury + toy : toy;
					break;
				}
			case 'q':
				relative = true;
			case 'Q':
				{
					ptr = getCoord( ptr, x1 );
					ptr = getCoord( ptr, y1 );
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					px1 = relative ? (curx + 2 * (x1 + curx)) * (1.0 / 3.0) : (curx + 2 * x1) * (1.0 / 3.0);
					py1 = relative ? (cury + 2 * (y1 + cury)) * (1.0 / 3.0) : (cury + 2 * y1) * (1.0 / 3.0);
					px2 = relative ? ((curx + tox) + 2 * (x1 + curx)) * (1.0 / 3.0) : (tox + 2 * x1) * (1.0 / 3.0);
					py2 = relative ? ((cury + toy) + 2 * (y1 + cury)) * (1.0 / 3.0) : (toy + 2 * y1) * (1.0 / 3.0);
					px3 = relative ? curx + tox : tox;
					py3 = relative ? cury + toy : toy;
					svgCurveToCubic(ite, px1, py1, px2, py2, px3, py3 );
					contrlx = relative ? curx + x1 : (tox + 2 * x1) * (1.0 / 3.0);
					contrly = relative ? cury + y1 : (toy + 2 * y1) * (1.0 / 3.0);
					curx = relative ? curx + tox : tox;
					cury = relative ? cury + toy : toy;
					break;
				}
			case 't':
				relative = true;
			case 'T':
				{
					ptr = getCoord(ptr, tox);
					ptr = getCoord(ptr, toy);
					xc = 2 * curx - contrlx;
					yc = 2 * cury - contrly;
					px1 = relative ? (curx + 2 * xc) * (1.0 / 3.0) : (curx + 2 * xc) * (1.0 / 3.0);
					py1 = relative ? (cury + 2 * yc) * (1.0 / 3.0) : (cury + 2 * yc) * (1.0 / 3.0);
					px2 = relative ? ((curx + tox) + 2 * xc) * (1.0 / 3.0) : (tox + 2 * xc) * (1.0 / 3.0);
					py2 = relative ? ((cury + toy) + 2 * yc) * (1.0 / 3.0) : (toy + 2 * yc) * (1.0 / 3.0);
					px3 = relative ? curx + tox : tox;
					py3 = relative ? cury + toy : toy;
					svgCurveToCubic(ite, px1, py1, px2, py2, px3, py3 );
					contrlx = xc;
					contrly = yc;
					curx = relative ? curx + tox : tox;
					cury = relative ? cury + toy : toy;
					break;
				}
			case 'a':
				relative = true;
			case 'A':
				{
					bool largeArc, sweep;
					double angle, rx, ry;
					ptr = getCoord( ptr, rx );
					ptr = getCoord( ptr, ry );
					ptr = getCoord( ptr, angle );
					ptr = getCoord( ptr, tox );
					largeArc = tox == 1;
					ptr = getCoord( ptr, tox );
					sweep = tox == 1;
					ptr = getCoord( ptr, tox );
					ptr = getCoord( ptr, toy );
					calculateArc(ite, relative, curx, cury, angle, tox, toy, rx, ry, largeArc, sweep );
				}
			}
			lastCommand = command;
			if(*ptr == '+' || *ptr == '-' || (*ptr >= '0' && *ptr <= '9'))
			{
				// there are still coords in this command
				if(command == 'M')
					command = 'L';
				else if(command == 'm')
					command = 'l';
			}
			else
				command = *(ptr++);

			if( lastCommand != 'C' && lastCommand != 'c' &&
			        lastCommand != 'S' && lastCommand != 's' &&
			        lastCommand != 'Q' && lastCommand != 'q' &&
			        lastCommand != 'T' && lastCommand != 't')
			{
				contrlx = curx;
				contrly = cury;
			}
		}
		if ((lastCommand != 'z') && (lastCommand != 'Z'))
			ret = true;
		if (ite->size() > 2)
		{
			if ((ite->point(0).x() == ite->point(ite->size()-2).x()) && (ite->point(0).y() == ite->point(ite->size()-2).y()))
				ret = false;
		}
	}
	return ret;
}

void OODPlug::calculateArc(FPointArray *ite, bool relative, double &curx, double &cury, double angle, double x, double y, double r1, double r2, bool largeArcFlag, bool sweepFlag)
{
	double sin_th, cos_th;
	double a00, a01, a10, a11;
	double x0, y0, x1, y1, xc, yc;
	double d, sfactor, sfactor_sq;
	double th0, th1, th_arc;
	int i, n_segs;
	sin_th = sin(angle * (M_PI / 180.0));
	cos_th = cos(angle * (M_PI / 180.0));
	double dx;
	if(!relative)
		dx = (curx - x) / 2.0;
	else
		dx = -x / 2.0;
	double dy;
	if(!relative)
		dy = (cury - y) / 2.0;
	else
		dy = -y / 2.0;
	double _x1 =  cos_th * dx + sin_th * dy;
	double _y1 = -sin_th * dx + cos_th * dy;
	double Pr1 = r1 * r1;
	double Pr2 = r2 * r2;
	double Px = _x1 * _x1;
	double Py = _y1 * _y1;
	double check = Px / Pr1 + Py / Pr2;
	if(check > 1)
	{
		r1 = r1 * sqrt(check);
		r2 = r2 * sqrt(check);
	}
	a00 = cos_th / r1;
	a01 = sin_th / r1;
	a10 = -sin_th / r2;
	a11 = cos_th / r2;
	x0 = a00 * curx + a01 * cury;
	y0 = a10 * curx + a11 * cury;
	if(!relative)
		x1 = a00 * x + a01 * y;
	else
		x1 = a00 * (curx + x) + a01 * (cury + y);
	if(!relative)
		y1 = a10 * x + a11 * y;
	else
		y1 = a10 * (curx + x) + a11 * (cury + y);
	d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
	sfactor_sq = 1.0 / d - 0.25;
	if(sfactor_sq < 0)
		sfactor_sq = 0;
	sfactor = sqrt(sfactor_sq);
	if(sweepFlag == largeArcFlag)
		sfactor = -sfactor;
	xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
	yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);

	th0 = atan2(y0 - yc, x0 - xc);
	th1 = atan2(y1 - yc, x1 - xc);
	th_arc = th1 - th0;
	if(th_arc < 0 && sweepFlag)
		th_arc += 2 * M_PI;
	else if(th_arc > 0 && !sweepFlag)
		th_arc -= 2 * M_PI;
	n_segs = static_cast<int>(ceil(fabs(th_arc / (M_PI * 0.5 + 0.001))));
	for(i = 0; i < n_segs; i++)
	{
		{
			double sin_th, cos_th;
			double a00, a01, a10, a11;
			double x1, y1, x2, y2, x3, y3;
			double t;
			double th_half;
			double _th0 = th0 + i * th_arc / n_segs;
			double _th1 = th0 + (i + 1) * th_arc / n_segs;
			sin_th = sin(angle * (M_PI / 180.0));
			cos_th = cos(angle * (M_PI / 180.0));
			a00 = cos_th * r1;
			a01 = -sin_th * r2;
			a10 = sin_th * r1;
			a11 = cos_th * r2;
			th_half = 0.5 * (_th1 - _th0);
			t = (8.0 / 3.0) * sin(th_half * 0.5) * sin(th_half * 0.5) / sin(th_half);
			x1 = xc + cos(_th0) - t * sin(_th0);
			y1 = yc + sin(_th0) + t * cos(_th0);
			x3 = xc + cos(_th1);
			y3 = yc + sin(_th1);
			x2 = x3 + t * sin(_th1);
			y2 = y3 - t * cos(_th1);
			svgCurveToCubic(ite, a00 * x1 + a01 * y1, a10 * x1 + a11 * y1, a00 * x2 + a01 * y2, a10 * x2 + a11 * y2, a00 * x3 + a01 * y3, a10 * x3 + a11 * y3 );
		}
	}
	if(!relative)
		curx = x;
	else
		curx += x;
	if(!relative)
		cury = y;
	else
		cury += y;
}

void OODPlug::svgMoveTo(double x1, double y1)
{
	CurrX = x1;
	CurrY = y1;
	StartX = x1;
	StartY = y1;
	PathLen = 0;
}

void OODPlug::svgLineTo(FPointArray *i, double x1, double y1)
{
	if ((!FirstM) && (WasM))
	{
		i->setMarker();
		PathLen += 4;
	}
	FirstM = false;
	WasM = false;
	if (i->size() > 3)
	{
		FPoint b1 = i->point(i->size()-4);
		FPoint b2 = i->point(i->size()-3);
		FPoint b3 = i->point(i->size()-2);
		FPoint b4 = i->point(i->size()-1);
		FPoint n1 = FPoint(CurrX, CurrY);
		FPoint n2 = FPoint(x1, y1);
		if ((b1 == n1) && (b2 == n1) && (b3 == n2) && (b4 == n2))
			return;
	}
	i->addPoint(FPoint(CurrX, CurrY));
	i->addPoint(FPoint(CurrX, CurrY));
	i->addPoint(FPoint(x1, y1));
	i->addPoint(FPoint(x1, y1));
	CurrX = x1;
	CurrY = y1;
	PathLen += 4;
}

void OODPlug::svgCurveToCubic(FPointArray *i, double x1, double y1, double x2, double y2, double x3, double y3)
{
	if ((!FirstM) && (WasM))
	{
		i->setMarker();
		PathLen += 4;
	}
	FirstM = false;
	WasM = false;
	if (PathLen > 3)
	{
		FPoint b1 = i->point(i->size()-4);
		FPoint b2 = i->point(i->size()-3);
		FPoint b3 = i->point(i->size()-2);
		FPoint b4 = i->point(i->size()-1);
		FPoint n1 = FPoint(CurrX, CurrY);
		FPoint n2 = FPoint(x1, y1);
		FPoint n3 = FPoint(x3, y3);
		FPoint n4 = FPoint(x2, y2);
		if ((b1 == n1) && (b2 == n2) && (b3 == n3) && (b4 == n4))
			return;
	}
	i->addPoint(FPoint(CurrX, CurrY));
	i->addPoint(FPoint(x1, y1));
	i->addPoint(FPoint(x3, y3));
	i->addPoint(FPoint(x2, y2));
	CurrX = x3;
	CurrY = y3;
	PathLen += 4;
}

void OODPlug::svgClosePath(FPointArray *i)
{
	if (PathLen > 2)
	{
		if ((PathLen == 4) || (i->point(i->size()-2).x() != StartX) || (i->point(i->size()-2).y() != StartY))
		{
			i->addPoint(i->point(i->size()-2));
			i->addPoint(i->point(i->size()-3));
			i->addPoint(FPoint(StartX, StartY));
			i->addPoint(FPoint(StartX, StartY));
		}
	}
}

OODPlug::~OODPlug()
{}

