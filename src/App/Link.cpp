/****************************************************************************
 *   Copyright (c) 2017 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#endif

#include <boost/preprocessor/stringize.hpp>
#include "Application.h"
#include "Document.h"
#include "GeoFeatureGroupExtension.h"
#include "Link.h"
#include "LinkBaseExtensionPy.h"
#include <Base/Console.h>

FC_LOG_LEVEL_INIT("App::Link", true,true)

using namespace App;

EXTENSION_PROPERTY_SOURCE(App::LinkBaseExtension, App::DocumentObjectExtension)

LinkBaseExtension::LinkBaseExtension(void)
{
    initExtensionType(LinkBaseExtension::getExtensionClassTypeId());
    EXTENSION_ADD_PROPERTY_TYPE(_LinkRecomputed, (false), " Link", 
            PropertyType(Prop_Hidden|Prop_Transient),0);
    props.resize(PropMax,0);
}

LinkBaseExtension::~LinkBaseExtension()
{
}

PyObject* LinkBaseExtension::getExtensionPyObject(void) {
    if (ExtensionPythonObject.is(Py::_None())){
        // ref counter is set to 1
        ExtensionPythonObject = Py::Object(new LinkBaseExtensionPy(this),true);
    }
    return Py::new_reference_to(ExtensionPythonObject);
}

const std::vector<LinkBaseExtension::PropInfo> &LinkBaseExtension::getPropertyInfo() const {
    static std::vector<LinkBaseExtension::PropInfo> PropsInfo;
    if(PropsInfo.empty()) {
        BOOST_PP_SEQ_FOR_EACH(LINK_PROP_INFO,PropsInfo,LINK_PARAMS);
    }
    return PropsInfo;
}

LinkBaseExtension::PropInfoMap LinkBaseExtension::getPropertyInfoMap() const {
    const auto &infos = getPropertyInfo();
    PropInfoMap ret;
    for(const auto &info : infos) 
        ret[info.name] = info;
    return ret;
}

void LinkBaseExtension::setProperty(int idx, Property *prop) {
    const auto &infos = getPropertyInfo();
    if(idx<0 || idx>=(int)infos.size())
        throw Base::RuntimeError("App::LinkBaseExtension: property index out of range");
    if(!prop)
        throw Base::ValueError("invalid property");
    if(!prop->isDerivedFrom(infos[idx].type)) {
        std::ostringstream str;
        str << "App::LinkBaseExtension: expected property type '" << 
            infos[idx].type.getName() << "', instead of '" << 
            prop->getClassTypeId().getName() << "'";
        throw Base::TypeError(str.str().c_str());
    }
    props[idx] = prop;

    switch(idx) {
    case PropElementList:
    case PropElementCount:
        if(getElementListProperty() && getElementCountProperty())
            getElementListProperty()->setStatus(Property::Immutable,true);
        break;
    case PropLinkTransform:
    case PropLinkPlacement:
    case PropPlacement:
        if(getLinkTransformProperty() &&
           getLinkPlacementProperty() &&
           getPlacementProperty())
        {
            bool transform = getLinkTransformValue();
            getPlacementProperty()->setStatus(Property::Hidden,transform);
            getLinkPlacementProperty()->setStatus(Property::Hidden,!transform);
        }
        break;
    }

    if(FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_TRACE)) {
        const char *propName;
        if(!prop) 
            propName = "<null>";
        else if(prop->getContainer())
            propName = prop->getName();
        else 
            propName = extensionGetPropertyName(prop);
        if(!propName) 
            propName = "?";
        FC_TRACE("set property " << infos[idx].name << ": " << propName);
    }
}

App::DocumentObjectExecReturn *LinkBaseExtension::extensionExecute(void) {
    // The actual value of LinkRecompouted is not important, just to notify view
    // provider that the link (in fact, its dependents, i.e. linked ones) have
    // recomputed.
    _LinkRecomputed.touch();
    return inherited::extensionExecute();
}

short LinkBaseExtension::extensionMustExecute(void) {
    auto link = getLink();
    if(!link) return 0;
    return link->mustExecute();
}

bool LinkBaseExtension::hasElements() const {
    auto propElements = getElementListProperty();
    return propElements && propElements->getSize();
}

bool LinkBaseExtension::extensionHasChildElement() const {
    if(hasElements())
        return true;
    if(getElementCountValue())
        return false;
    DocumentObject *linked = getTrueLinkedObject(true);
    if(linked) {
        if(linked->hasChildElement())
            return true;
        return linked->hasExtension(App::GroupExtension::getExtensionClassTypeId(),true);
    }
    return false;
}

int LinkBaseExtension::extensionSetElementVisible(const char *element, bool visible) {
    int index = getElementIndex(element);
    if(index>=0) {
        auto propElementVis = getVisibilityListProperty();
        if(!propElementVis || !element || !element[0]) 
            return -1;
        if(propElementVis->getSize()<=index) {
            if(visible) return 1;
            propElementVis->setSize(index+1, true);
        }
        propElementVis->set1Value(index,visible,true);
        return 1;
    }
    DocumentObject *linked = getTrueLinkedObject(true);
    if(linked)
        return linked->setElementVisible(element,visible);
    return -1;
}

int LinkBaseExtension::extensionIsElementVisible(const char *element) {
    int index = getElementIndex(element);
    if(index>=0) {
        auto propElementVis = getVisibilityListProperty();
        if(propElementVis) {
            if(propElementVis->getSize()<=index || propElementVis->getValues()[index])
                return 1;
            return 0;
        }
        return -1;
    }
    DocumentObject *linked = getTrueLinkedObject(true);
    if(linked)
        return linked->isElementVisible(element);
    return -1;
}

const DocumentObject *LinkBaseExtension::getContainer() const {
    auto ext = getExtendedContainer();
    if(!ext || !ext->isDerivedFrom(DocumentObject::getClassTypeId()))
        throw Base::RuntimeError("Link: container not derived from document object");
    return static_cast<const DocumentObject *>(ext);
}

DocumentObject *LinkBaseExtension::getContainer(){
    auto ext = getExtendedContainer();
    if(!ext || !ext->isDerivedFrom(DocumentObject::getClassTypeId()))
        throw Base::RuntimeError("Link: container not derived from document object");
    return static_cast<DocumentObject *>(ext);
}

DocumentObject *LinkBaseExtension::getLink(int depth) const{
    GetApplication().checkLinkDepth(depth);
    if(getLinkedObjectProperty())
        return getLinkedObjectValue();
    return 0;
}

int LinkBaseExtension::getArrayIndex(const char *subname, const char **psubname) {
    if(!subname) return -1;
    const char *dot = strchr(subname,'.');
    if(!dot) dot= subname+strlen(subname);
    if(dot == subname) return -1;
    int idx = 0;
    for(const char *c=subname;c!=dot;++c) {
        if(!isdigit(*c)) return -1;
        idx = idx*10 + *c -'0';
    }
    if(psubname) {
        if(*dot)
            *psubname = dot+1;
        else
            *psubname = dot;
    }
    return idx;
}

int LinkBaseExtension::getElementIndex(const char *subname, const char **psubname) const {
    if(!subname) return -1;
    int idx;
    if(isdigit(subname[0])) {
        idx = getArrayIndex(subname,psubname);
        if(idx>=0) {
            if(getElementCountProperty()) {
                if(idx>=getElementCountValue())
                    idx = -1;
            }else if(!getElementListProperty() || idx>=getElementListProperty()->getSize())
                idx = -1;
        }
        return idx;
    }
    const char *dot = strchr(subname,'.');
    if(!dot) dot= subname+strlen(subname);
    if(subname[0]!='$') {
        auto prop = getElementListProperty();
        if(!prop || !prop->find(std::string(subname,dot-subname).c_str(),&idx))
            return -1;
        if(psubname)
            *psubname = dot+1;
        return idx;
    }
    ++subname;
    std::string name(subname,dot-subname);
    for(auto element : getElementListValue()) {
        if(element->Label.getStrValue() == name) {
            if(psubname)
                *psubname = dot[0]?dot+1:dot;
            return idx;
        }
        ++idx;
    }
    return -1;
}


Base::Matrix4D LinkBaseExtension::getTransform(bool transform) const {
    Base::Matrix4D mat;
    if(transform) {
        if(getLinkPlacementProperty())
            mat = getLinkPlacementValue().toMatrix();
        else if(getPlacementProperty())
            mat = getPlacementValue().toMatrix();
    }
    if(getScaleProperty()) {
        Base::Matrix4D s;
        s.scale(getScaleValue());
        mat *= s;
    }
    return mat;
}

bool LinkBaseExtension::extensionGetSubObjects(std::vector<std::string> &ret) const {
    if(hasElements()) {
        for(auto obj : getElementListValue()) {
            if(obj && obj->getNameInDocument()) {
                std::string name(obj->getNameInDocument());
                name+='.';
                ret.push_back(name);
            }
        }
    }else if(getElementCountValue()) {
        char index[30];
        for(int i=0,count=getElementCountValue();i<count;++i) {
            snprintf(index,sizeof(index),"%d.",i);
            ret.push_back(index);
        }
    }else {
        DocumentObject *linked = getTrueLinkedObject(true);
        if(linked)
            ret = linked->getSubObjects();
    }
    return true;
}

bool LinkBaseExtension::extensionGetSubObject(DocumentObject *&ret, const char *subname, 
        PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const 
{
    ret = 0;
    if(mat) *mat *= getTransform(transform);
    auto obj = getContainer();
    if(!subname || !subname[0]) {
        ret = const_cast<DocumentObject*>(obj);
        if(!hasElements() && !getElementCountValue() && pyObj) {
            Base::Matrix4D matNext;
            if(mat) {
                matNext = *mat;
                mat = &matNext;
            }
            auto linked = getTrueLinkedObject(true,mat,depth);
            if(linked) 
                linked->getSubObject(subname,pyObj,mat,false,depth+1);
        }
        return true;
    }

    DocumentObject *element = 0;
    bool isElement = false;
    int idx = getElementIndex(subname,&subname);
    if(idx>=0) {
        if(hasElements()) {
            const auto &elements = getElementListValue();
            if(idx>=(int)elements.size() || !elements[idx] || !elements[idx]->getNameInDocument())
                return true;
            ret = elements[idx]->getSubObject(subname,pyObj,mat,true,depth+1);
            // do not resolve the link if this element is the last referenced object
            if(!subname || !strchr(subname,'.'))
                ret = elements[idx];
            return true;
        }

        int elementCount = getElementCountValue();
        if(idx>=elementCount)
            return true;
        isElement = true;
        if(mat) {
            auto placementList = getPlacementListProperty();
            if(placementList && placementList->getSize()>idx)
                *mat *= (*placementList)[idx].toMatrix();
            auto scaleList = getScaleListProperty();
            if(scaleList && scaleList->getSize()>idx) {
                Base::Matrix4D s;
                s.scale((*scaleList)[idx]);
                *mat *= s;
            }
        }
    }

    auto linked = getTrueLinkedObject(true,mat,depth);
    if(!linked) 
        return true;

    Base::Matrix4D matNext;
    ret = linked->getSubObject(subname,pyObj,mat?&matNext:0,false,depth+1);
    if(ret) {
        // do not resolve the link if we are the last referenced object
        if(subname && strchr(subname,'.')) {
            if(mat)
                *mat *= matNext;
        }else if(element)
            ret = element;
        else if(!isElement)
            ret = const_cast<DocumentObject*>(obj);
        else if(mat)
            *mat *= matNext;
    }
    return true;
}

void LinkBaseExtension::onExtendedUnsetupObject () {
    auto objs = getElementListValue();
    for(auto obj : objs) {
        if(!obj->isDeleting())
            obj->getDocument()->remObject(obj->getNameInDocument());
    }
}

DocumentObject *LinkBaseExtension::getTrueLinkedObject(
        bool recurse, Base::Matrix4D *mat, int depth) const
{
    auto ret = getLink(depth);
    if(!ret) return 0;
    bool transform = getLinkTransformValue();
    if(mySub.size()) {
        ret = ret->getSubObject(mySub.c_str(),0,mat,transform,depth+1);
        transform = false;
    }
    if(ret && recurse)
        ret = ret->getLinkedObject(recurse,mat,transform,depth+1);
    if(ret && !ret->getNameInDocument())
        return 0;
    return ret;
}

bool LinkBaseExtension::extensionGetLinkedObject(DocumentObject *&ret, 
        bool recurse, Base::Matrix4D *mat, bool transform, int depth) const
{
    if(mat) 
        *mat *= getTransform(transform);
    ret = 0;
    if(!hasElements())
        ret = getTrueLinkedObject(recurse,mat,depth);
    if(!ret)
        ret = const_cast<DocumentObject*>(getContainer());
    // always return true to indicate we've handled getLinkObject() call
    return true;
}

void LinkBaseExtension::extensionOnChanged(const Property *prop) {
    auto parent = getContainer();
    if(parent && !parent->isRestoring() && prop && !prop->testStatus(Property::User3))
        update(parent,prop);
    inherited::extensionOnChanged(prop);
}

void LinkBaseExtension::update(App::DocumentObject *parent, const Property *prop) {
    if(!prop) return;
    if(prop == getLinkPlacementProperty() || prop == getPlacementProperty()) {
        auto src = getLinkPlacementProperty();
        auto dst = getPlacementProperty();
        if(src!=prop) std::swap(src,dst);
        if(src && dst) {
            dst->setStatus(Property::User3,true);
            dst->setValue(src->getValue());
            dst->setStatus(Property::User3,false);
        }
    }else if(prop == getElementCountProperty() || prop == getShowElementProperty()) {
        const auto &elementList = getElementListProperty();
        int elementCount = getElementCountValue();
        bool showElement = getShowElementValue();
        if(elementList) {
            if(elementCount<=0 || !showElement) {
                auto objs = elementList->getValues();
                elementList->setValues(std::vector<App::DocumentObject*>());
                for(auto obj : objs) 
                    obj->getDocument()->remObject(obj->getNameInDocument());
            }else if(showElement && elementCount>0) {
                const auto &placementList = getPlacementListValue();
                const auto &visibilityList = getVisibilityListValue();
                const auto &scaleList = getScaleListValue();
                if(elementCount>elementList->getSize()) {
                    std::string name = parent->getNameInDocument();
                    name += "_i";
                    name = parent->getDocument()->getUniqueObjectName(name.c_str());
                    if(name[name.size()-1] != 'i')
                        name += "_i";
                    auto offset = name.size();
                    int i = elementList->getSize();
                    elementList->setSize(elementCount);
                    for(;i<elementCount;++i) {
                        LinkElement *obj = new LinkElement;
                        obj->myOwner = this;
                        if((int)placementList.size()>i) {
                            obj->Placement.setStatus(Property::User3,true);
                            obj->Placement.setValue(placementList[i]);
                            obj->Placement.setStatus(Property::User3,false);
                        }else if(prop==getElementCountProperty()){
                            Base::Placement pla(Base::Vector3d(i,0,0),Base::Rotation());
                            obj->Placement.setValue(pla);
                        }

                        if((int)scaleList.size()>i) {
                            obj->Scale.setStatus(Property::User3,true);
                            obj->Scale.setValue(scaleList[i]);
                            obj->Scale.setStatus(Property::User3,false);
                        }
                        name.resize(offset);
                        name += std::to_string(i);
                        parent->getDocument()->addObject(obj,name.c_str());
                        elementList->set1Value(i,obj,i+1==elementCount);
                    }
                }else{
                    auto objs = elementList->getValues();
                    // NOTE setSize() won't touch() elementList. This is what we want. Or is it?
                    elementList->setSize(elementCount);
                    while((int)objs.size()>elementCount) {
                        parent->getDocument()->remObject(objs.back()->getNameInDocument());
                        objs.pop_back();
                    }
                }
            }
        }
    }else if(prop == getElementListProperty() ||
             prop == getPlacementListProperty() ||
             prop == getScaleListProperty())
    {
        int i;
        const auto &elementList = getElementListValue();
        const auto &placementList = getPlacementListValue();
        const auto &scaleList = getScaleListValue();
        for(i=0;i<(int)elementList.size();++i) {
            auto obj = dynamic_cast<LinkElement*>(elementList[i]);
            if(!obj) continue;
            obj->myOwner = this;
            obj->myIndex = i;
            const auto &pla = (i<(int)placementList.size())?placementList[i]:Base::Placement();
            if(obj->Placement.getValue() != pla) {
                obj->Placement.setStatus(Property::User3,true);
                obj->Placement.setValue(pla);
                obj->Placement.setStatus(Property::User3,false);
            }
            const auto &scale = (i<(int)scaleList.size())?scaleList[i]:Base::Vector3d(1,1,1);
            if(obj->Scale.getValue() != scale) {
                obj->Scale.setStatus(Property::User3,true);
                obj->Scale.setValue(scale);
                obj->Scale.setStatus(Property::User3,false);
            }
        }
    }else if(prop == getLinkedObjectProperty()) {
        auto xlink = dynamic_cast<const PropertyXLink*>(prop);
        if(xlink) {
            mySub = xlink->getSubName();
            if(mySub.size() && mySub[mySub.size()-1]!='.')
                mySub += '.';
        }
    }else if(prop == getLinkTransformProperty()) {
        auto linkPlacement = getLinkPlacementProperty();
        auto placement = getPlacementProperty();
        if(linkPlacement && placement) {
            bool transform = getLinkTransformValue();
            placement->setStatus(Property::Hidden,transform);
            linkPlacement->setStatus(Property::Hidden,!transform);
        }
    }
}

void LinkBaseExtension::extensionOnDocumentRestored() {
    inherited::extensionOnDocumentRestored();
    auto parent = getContainer();
    if(parent) {
        if(getLinkPlacementProperty())
            update(parent,getLinkPlacementProperty());
        else
            update(parent,getPlacementProperty());
        update(parent,getLinkTransformProperty());
        if(getElementListProperty()) {
            update(parent,getElementListProperty());
            for(auto obj : getElementListValue()) {
                auto element = dynamic_cast<LinkElement*>(obj);
                if(element) element->purgeTouched();
            }
        }
    }
}

void LinkBaseExtension::setLink(DocumentObject *obj, const char *subname,
        const std::vector<std::string> &subElements) 
{
    auto linkProp = getLinkedObjectProperty();
    auto xlink = dynamic_cast<PropertyXLink*>(linkProp);
    auto subElementProp = getSubElementsProperty();
    if(!linkProp)
        throw Base::RuntimeError("No PropertyLink configured");

    if(subElements.size() && !subElementProp)
        throw Base::RuntimeError("No SubElements Property configured");

    std::string _subname;
    if(subname && subname[0]) {
        _subname = subname;
        if(_subname[_subname.size()-1]!='.') {
            _subname += '.';
            subname = _subname.c_str();
        }
    }

    int depth = 0;
    while(obj && obj->isDerivedFrom(LinkElement::getClassTypeId())) {
        LinkElement *element = static_cast<LinkElement*>(obj);
        if(!element->myOwner)
            throw Base::RuntimeError("Orphan link element");
        obj = static_cast<DocumentObject*>(element->myOwner->getExtendedContainer());
        if(!subname || !subname[0]) {
            obj = obj->getLinkedObject(false,0,false,++depth);
            continue;
        }
        std::ostringstream str;
        str << element->myIndex << '.';
        if(subname)
            str << subname;
        _subname = str.str();
        subname = _subname.c_str();
        break;
    }

    if(obj) {
        if(!obj->getNameInDocument())
            throw Base::ValueError("Invalid document object");
        if(!xlink) {
            auto parent = getContainer();
            if(parent && obj->getDocument()!=parent->getDocument())
                throw Base::ValueError("Cannot link to external object without PropertyXLink");
        }
    }

    if(subname && subname[0]) {
        auto subObj = obj->getSubObject(subname);
        if(!subObj) 
            throw Base::RuntimeError("Cannot find linked sub-object");
        if(subObj->isDerivedFrom(LinkElement::getClassTypeId())) {
            LinkElement *element = static_cast<LinkElement*>(subObj);
            if(!element->myOwner)
                throw Base::RuntimeError("Orphan link element");
            subObj = static_cast<DocumentObject*>(element->myOwner->getExtendedContainer());
            std::ostringstream str;
            const char *dot = strchr(subname,'.');
            assert(dot);
            str << dot+1 << element->myIndex << '.';
            _subname = str.str();
            subname = _subname.c_str();
        }

        if(!xlink) 
            throw Base::RuntimeError("No Sub property configured");
    }

    if(subElements.size()) {
        subElementProp->setStatus(Property::User3, true);
        subElementProp->setValue(subElements);
        subElementProp->setStatus(Property::User3, false);
    }
    if(xlink)
        xlink->setValue(obj,subname,true);
    else
        linkProp->setValue(obj);
}

///////////////////////////////////////////////////////////////////////////////////////////

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::LinkBaseExtensionPython, App::LinkBaseExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<LinkBaseExtension>;

}

//////////////////////////////////////////////////////////////////////////////

EXTENSION_PROPERTY_SOURCE(App::LinkExtension, App::LinkBaseExtension)

LinkExtension::LinkExtension(void)
{
    initExtensionType(LinkExtension::getExtensionClassTypeId());

    LINK_PROPS_ADD_EXTENSION(LINK_PARAMS_EXT);
}

LinkExtension::~LinkExtension()
{
}

///////////////////////////////////////////////////////////////////////////////////////////

namespace App {
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::LinkExtensionPython, App::LinkExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<App::LinkExtension>;

}

///////////////////////////////////////////////////////////////////////////////////////////

PROPERTY_SOURCE_WITH_EXTENSIONS(App::Link, App::DocumentObject)

Link::Link() {
    LINK_PROPS_ADD(LINK_PARAMS_LINK);
    LinkExtension::initExtension(this);

    static const PropertyIntegerConstraint::Constraints s_constraints = {0,INT_MAX,1};
    ElementCount.setConstraints(&s_constraints);
}

namespace App {
PROPERTY_SOURCE_TEMPLATE(App::LinkPython, App::DocumentObject)
template<> const char* App::LinkPython::getViewProviderName(void) const {
    return "Gui::ViewProviderLinkPython";
}
template class AppExport FeaturePythonT<App::Link>;
}

//--------------------------------------------------------------------------------
PROPERTY_SOURCE(App::LinkElement, App::DocumentObject)

LinkElement::LinkElement():myOwner(0),myIndex(-1)
{
    ADD_PROPERTY_TYPE(Placement,(Base::Placement())," Link",(PropertyType)(Prop_Transient),0);
    ADD_PROPERTY_TYPE(Scale,(Base::Vector3d(1,1,1))," Link",(PropertyType)(Prop_Transient),0);
    ADD_PROPERTY_TYPE(_Recomputed, (false), " Link", (PropertyType)(Prop_Transient|Prop_Hidden),0);
}

void LinkElement::onChanged(const Property *prop) {
    if(!isRestoring()) {
        if(prop == &Placement) {
            if(myOwner && !Placement.testStatus(Property::User3)) {
                auto placementList = myOwner->getPlacementListProperty();
                if(placementList) {
                    placementList->setStatus(Property::User3,true);
                    if(placementList->getSize()<=myIndex)
                        placementList->setSize(myIndex+1);
                    placementList->set1Value(myIndex,Placement.getValue(),true);
                    placementList->setStatus(Property::User3,false);
                }
            }
        }else if(prop == &Scale) {
            if(myOwner && !Scale.testStatus(Property::User3)) {
                auto scaleList = myOwner->getScaleListProperty();
                if(scaleList) {
                    scaleList->setStatus(Property::User3,true);
                    if(scaleList->getSize()<=myIndex)
                        scaleList->setSize(myIndex+1,Base::Vector3d(1,1,1));
                    scaleList->set1Value(myIndex,Scale.getValue(),true);
                    scaleList->setStatus(Property::User3,false);
                }
            }
        }
    }
    inherited::onChanged(prop);
}

int LinkElement::setElementVisible(const char *element, bool visible) {
    auto linked = getLinkedObject(true,0,false,0);
    if(linked != this)
        return linked->setElementVisible(element,visible);
    return -1;
}

int LinkElement::isElementVisible(const char *element) const {
    auto linked = getLinkedObject(true,0,false,0);
    if(linked != this)
        return linked->isElementVisible(element);
    return -1;
}

bool LinkElement::hasChildElement() const {
    auto linked = getLinkedObject(true,0,false,0);
    if(linked != this)
        return linked->hasChildElement();
    return false;
}

std::vector<std::string> LinkElement::getSubObjects() const {
    auto linked = getLinkedObject(true,0,false,0);
    if(linked != this)
        return linked->getSubObjects();
    return std::vector<std::string>();
}

DocumentObject *LinkElement::getSubObject(const char *subname,
        PyObject **pyObj, Base::Matrix4D *mat, bool transform, int depth) const 
{
    auto linked = getLinkedObject(true,mat,transform,depth);
    if(linked != this)
        return linked->getSubObject(subname,pyObj,mat,false,depth);
    return 0;
}

DocumentObject *LinkElement::getLinkedObject(bool recurse, Base::Matrix4D *mat, bool transform, int depth) const
{
    DocumentObject *ret = 0;
    if(mat) {
        if(transform)
            *mat *= Placement.getValue().toMatrix();
        Base::Matrix4D s;
        s.scale(Scale.getValue());
        *mat *= s;
    }
    if(myOwner)
        ret = myOwner->getTrueLinkedObject(recurse,mat,depth);
    if(ret)
        return ret;
    return const_cast<LinkElement*>(this);
}

short LinkElement::mustExecute() const {
    auto ret = inherited::mustExecute();
    if(ret) return ret;
    auto linked = getLinkedObject(true,0,false,0);
    if(linked!=this)
        return linked->mustExecute();
    return 0;
}

App::DocumentObjectExecReturn *LinkElement::execute() {
    _Recomputed.touch();
    return inherited::execute();
}
