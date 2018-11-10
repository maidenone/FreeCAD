/***************************************************************************
 *   Copyright (c) Jürgen Riegel          (juergen.riegel@web.de) 2002     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
#	include <cassert>
#endif

/// Here the FreeCAD includes sorted by Base,App,Gui......
#include "Property.h"
#include "ObjectIdentifier.h"
#include "PropertyContainer.h"
#include <Base/Exception.h>
#include "Application.h"

using namespace App;


//**************************************************************************
//**************************************************************************
// Property
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TYPESYSTEM_SOURCE_ABSTRACT(App::Property , Base::Persistence);

//**************************************************************************
// Construction/Destruction

// Here is the implementation! Description should take place in the header file!
Property::Property()
  :father(0)
{

}

Property::~Property()
{

}

const char* Property::getName(void) const
{
    return father->getPropertyName(this);
}

short Property::getType(void) const
{
    return father->getPropertyType(this);
}

const char* Property::getGroup(void) const
{
    return father->getPropertyGroup(this);
}

const char* Property::getDocumentation(void) const
{
    return father->getPropertyDocumentation(this);
}

void Property::setContainer(PropertyContainer *Father)
{
    father = Father;
}

void Property::setPathValue(const ObjectIdentifier &path, const boost::any &value)
{
    path.setValue(value);
}

const boost::any Property::getPathValue(const ObjectIdentifier &path) const
{
    return path.getValue();
}

void Property::getPaths(std::vector<ObjectIdentifier> &paths) const
{
    paths.push_back(App::ObjectIdentifier(*this));
}

const ObjectIdentifier Property::canonicalPath(const ObjectIdentifier &p) const
{
    return p;
}

void Property::touch()
{
    if (father)
        father->onChanged(this);
    StatusBits.set(Touched);
}

void Property::setReadOnly(bool readOnly)
{
    unsigned long status = this->getStatus();
    this->setStatus(App::Property::ReadOnly, readOnly);
    if (status != this->getStatus())
        App::GetApplication().signalChangePropertyEditor(*this);
}

void Property::hasSetValue(void)
{
    if (father)
        father->onChanged(this);
    StatusBits.set(Touched);
}

void Property::aboutToSetValue(void)
{
    if (father)
        father->onBeforeChange(this);
}

void Property::verifyPath(const ObjectIdentifier &p) const
{
    if (p.numSubComponents() != 1)
        throw Base::ValueError("Invalid property path: single component expected");
    if (!p.getPropertyComponent(0).isSimple())
        throw Base::ValueError("Invalid property path: simple component expected");
    if (p.getPropertyComponent(0).getName() != getName())
        throw Base::ValueError("Invalid property path: name mismatch");
}

Property *Property::Copy(void) const 
{
    // have to be reimplemented by a subclass!
    assert(0);
    return 0;
}

void Property::Paste(const Property& /*from*/)
{
    // have to be reimplemented by a subclass!
    assert(0);
}

//**************************************************************************
//**************************************************************************
// PropertyListsBase
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void PropertyListsBase::_setPyObject(PyObject *value) {
    std::vector<PyObject *> vals;
    std::vector<int> indices;
    if (PyDict_Check(value)) {
        PyObject* keyList = PyDict_Keys(value);
        PyObject* itemList = PyDict_Values(value);
        Py_ssize_t nSize = PyList_Size(keyList);
        vals.reserve(nSize);
        indices.reserve(nSize);
        int listSize = getSize();
        for (Py_ssize_t i=0; i<nSize;++i) {
            std::string keyStr;
            PyObject* key = PyList_GetItem(keyList, i);
#if PY_MAJOR_VERSION < 3
            if(!PyInt_Check(key)) 
#else
            if(!PyLong_Check(key))
#endif
                throw Base::TypeError("expect key type to be interger");
            auto idx = PyLong_AsLong(key);
            if(idx<-1 || idx>listSize) 
                throw Base::RuntimeError("index out of bound");
            if(idx==-1 || idx==listSize) {
                idx = listSize;
                ++listSize;
            }
            indices.push_back(idx);
            vals.push_back(PyList_GetItem(itemList,i));
        }
    }else if (PySequence_Check(value)) {
        Py_ssize_t nSize = PySequence_Size(value);
        vals.reserve(nSize);
        for (Py_ssize_t i=0; i<nSize;++i)
            vals.push_back(PySequence_GetItem(value, i));
    }else
        vals.push_back(value);
    setPyValues(vals,indices);
}

//**************************************************************************
//**************************************************************************
// PropertyLists
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TYPESYSTEM_SOURCE_ABSTRACT(App::PropertyLists , App::Property);

