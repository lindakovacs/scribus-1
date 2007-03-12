/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/
#ifndef INSERTTABLE_H
#define INSERTTABLE_H

#include <qvariant.h>
#include <qdialog.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <q3whatsthis.h>
//Added by qt3to4:
#include <Q3GridLayout>
#include <Q3HBoxLayout>
#include <Q3VBoxLayout>

#include "scribusapi.h"

class SCRIBUS_API InsertTable : public QDialog
{
	Q_OBJECT

public:
	InsertTable( QWidget* parent, int maxRow, int maxCol );
	~InsertTable() {};

	QSpinBox* Cols;
	QSpinBox* Rows;

protected:
	Q3VBoxLayout* InsertTableLayout;
	Q3GridLayout* layout2;
	Q3HBoxLayout* layout1;
	QLabel* Text1;
	QLabel* Text2;
	QPushButton* okButton;
	QPushButton* cancelButton;
};

#endif // INSERTTABLE_H
