#ifndef CMDPAGE_H
#define CMDPAGE_H


/** Page related Commands */
PyObject *scribus_newpage(PyObject *self, PyObject* args);
PyObject *scribus_actualpage(PyObject *self);
PyObject *scribus_redraw(PyObject *self);
PyObject *scribus_savepageeps(PyObject *self, PyObject* args);
PyObject *scribus_deletepage(PyObject *self, PyObject* args);
PyObject *scribus_gotopage(PyObject *self, PyObject* args);
PyObject *scribus_pagecount(PyObject *self);
PyObject *scribus_getHguides(PyObject *self);
PyObject *scribus_setHguides(PyObject *self, PyObject* args);
PyObject *scribus_getVguides(PyObject *self);
PyObject *scribus_setVguides(PyObject *self, PyObject* args);
/** 
returns a tuple with page domensions in used system
e.g. when is the doc in picas returns picas ;)
(Petr Vanek 02/17/04) 
*/
PyObject *scribus_pagedimension(PyObject *self);
/**
returns a list of tuples with items on the actual page
TODO: solve utf/iso chars in object names
(Petr Vanek 03/02/2004)
*/
PyObject *scribus_getpageitems(PyObject *self);
/**
returns a tuple with page margins
Craig Ringer, Petr Vanek 09/25/2004
*/
PyObject *scribus_getpagemargins(PyObject *self);

#endif

