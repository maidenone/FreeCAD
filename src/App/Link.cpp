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
    int index = getShowElementValue()?getElementIndex(element):getArrayIndex(element);
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
    int index = getShowElementValue()?getElementIndex(element):getArrayIndex(element);
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
    const char *dot = strchr(subname,'.');
    if(!dot) dot= subname+strlen(subname);

    if(isdigit(subname[0])) {
        idx = getArrayIndex(subname,psubname);
        if(idx<0) return -1;
        if(getElementCountProperty()) {
            if(idx>=getElementCountValue())
                return -1;
        }else if(!getElementListProperty() || idx>=getElementListProperty()->getSize())
            return -1;
    }else if(!getShowElementValue() && getElementCountValue()) {
        // If elements are collapsed, we allow refereing the first
        // array element with the actual linked object's name or label
        auto linked = getTrueLinkedObject(true);
        if(!linked || !linked->getNameInDocument()) return -1;
        if(subname[0]=='$') {
           if(std::string(subname+1,dot-subname-1) != linked->Label.getValue())
               return -1;
        }else if(std::string(subname,dot-subname)!=linked->getNameInDocument())
            return -1;
        idx = 0;
    }else if(subname[0]!='$') {
        // Try search by element objects' name
        auto prop = getElementListProperty();
        if(!prop || !prop->find(std::string(subname,dot-subname).c_str(),&idx))
            return -1;
    }else {
        // Try search by label if the reference name start with '$'
        ++subname;
        std::string name(subname,dot-subname);
        const auto &elements = getElementListValue();
        idx = 0;
        for(auto element : elements) {
            if(element->Label.getStrValue() == name)
                break;
            ++idx;
        }
        if(idx >= (int)elements.size())
            return -1;
    }
    if(psubname)
        *psubname = dot[0]?dot+1:dot;
    return idx;
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
        return true;
    }
    DocumentObject *linked = getTrueLinkedObject(true);
    if(linked) {
        if(!getElementCountValue())
            ret = linked->getSubObjects();
        else{
            char index[30];
            for(int i=0,count=getElementCountValue();i<count;++i) {
                snprintf(index,sizeof(index),"%d.",i);
                ret.push_back(index);
            }
        }
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
                linked->getSubObject(0,pyObj,mat,false,depth+1);
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
    }else if(prop == getShowElementProperty()) {
        const auto &objs = getElementListValue();
        if(getShowElementValue()) 
            update(parent,getElementCountProperty());
        else if(objs.size()){
            // preseve element properties in ourself
            std::vector<Base::Placement> placements;
            placements.reserve(objs.size());
            std::vector<Base::Vector3d> scales;
            scales.reserve(objs.size());
            for(size_t i=0;i<objs.size();++i) {
                auto element = dynamic_cast<LinkElement*>(objs[i]);
                if(element) {
                    placements.push_back(element->Placement.getValue());
                    scales.push_back(element->Scale.getValue());
                }else{
                    placements.emplace_back();
                    scales.emplace_back(1,1,1);
                }
            }
            if(getPlacementListProperty()) {
                getPlacementListProperty()->setStatus(Property::User3,getScaleListProperty()!=0);
                getPlacementListProperty()->setValue(placements);
                getPlacementListProperty()->setStatus(Property::User3,false);
            }
            if(getScaleListProperty())
                getScaleListProperty()->setValue(scales);

            // About to remove all elements
            //
            // NOTE: there is an assumption here that signalChangeObject will
            // be trigged before the call here (i.e. through
            // extensionOnChanged()), which is the default behavior on
            // DocumentObject::onChanged(). This ensures the view provider has
            // a chance to save the element view provider's properties.  This
            // assumption may be broken if someone override onChanged().
            getElementListProperty()->setValues(std::vector<App::DocumentObject*>());

            for(auto obj : objs) {
                if(obj && obj->getNameInDocument())
                    obj->getDocument()->remObject(obj->getNameInDocument());
            }
        }
    }else if(prop == getElementCountProperty()) {
        size_t elementCount = getElementCountValue()<0?0:(size_t)getElementCountValue();

        if(getVisibilityListProperty()) {
            if(getVisibilityListValue().size()>elementCount)
                getVisibilityListProperty()->setSize(getElementCountValue());
        }

        if(!getShowElementValue()) {
            if(getScaleListProperty()) {
                auto scales = getScaleListValue();
                scales.resize(elementCount,Base::Vector3d(1,1,1));
                getScaleListProperty()->setStatus(Property::User3,getPlacementListProperty()!=0);
                getScaleListProperty()->setValue(scales);
                getScaleListProperty()->setStatus(Property::User3,false);
            }
            if(getPlacementListProperty()) {
                auto placements = getPlacementListValue();
                if(placements.size()<elementCount) {
                    for(size_t i=placements.size();i<elementCount;++i)
                        placements.emplace_back(Base::Vector3d(i,0,0),Base::Rotation());
                }else
                    placements.resize(elementCount);
                getPlacementListProperty()->setValue(placements);
            }
        }else if(getElementListProperty()) {
            const auto &placementList = getPlacementListValue();
            const auto &scaleList = getScaleListValue();
            auto objs = getElementListValue();
            if(elementCount>objs.size()) {
                std::string name = parent->getNameInDocument();
                name += "_i";
                name = parent->getDocument()->getUniqueObjectName(name.c_str());
                if(name[name.size()-1] != 'i')
                    name += "_i";
                auto offset = name.size();
                for(size_t i=objs.size();i<elementCount;++i) {
                    LinkElement *obj = new LinkElement;
                    Base::Placement pla(Base::Vector3d(i,0,0),Base::Rotation());
                    obj->Placement.setValue(pla);
                    name.resize(offset);
                    name += std::to_string(i);
                    parent->getDocument()->addObject(obj,name.c_str());
                    objs.push_back(obj);
                }
                if(getPlacementListProperty()) 
                    getPlacementListProperty()->setSize(0);
                if(getScaleListProperty())
                    getScaleListProperty()->setSize(0);

                getElementListProperty()->setValue(objs);

            }else if(elementCount<objs.size()){
                std::vector<App::DocumentObject*> tmpObjs;
                while(objs.size()>elementCount) {
                    tmpObjs.push_back(objs.back());
                    objs.pop_back();
                }
                getElementListProperty()->setValue(objs);
                for(auto obj : tmpObjs) {
                    if(obj && obj->getNameInDocument())
                        obj->getDocument()->remObject(obj->getNameInDocument());
                }
            }
        }
    }else if(prop == getElementListProperty()) {
        const auto &elements = getElementListValue();
        // Element list changed, we need to sychrnoize VisibilityList.
        if(getShowElementValue() && getVisibilityListProperty()) {
            boost::dynamic_bitset<> vis;
            vis.resize(elements.size(),true);
            std::set<const App::DocumentObject *> hiddenElements;
            for(size_t i=0;i<elements.size();++i) {
                if(myHiddenElements.find(elements[i])!=myHiddenElements.end()) {
                    hiddenElements.insert(elements[i]);
                    vis[i] = false;
                }
            }
            myHiddenElements.swap(hiddenElements);
            if(vis != getVisibilityListValue())
                getVisibilityListProperty()->setValue(vis);
        }

        // If we have a link property, it means the element list is for the array.
        // sychronize the the element's linked object.
        if(getLinkedObjectProperty()) {
            syncElementList();
            if(getShowElementValue() && getElementCountProperty() && 
               getElementCountValue()!=(int)elements.size())
                getElementCountProperty()->setValue(elements.size());
        }
    }else if(prop == getLinkedObjectProperty()) {
        auto xlink = dynamic_cast<const PropertyXLink*>(prop);
        if(xlink) {
            mySub = xlink->getSubName();
            if(mySub.size() && mySub[mySub.size()-1]!='.')
                mySub += '.';
        }
        syncElementList();

    }else if(prop == getLinkTransformProperty()) {
        auto linkPlacement = getLinkPlacementProperty();
        auto placement = getPlacementProperty();
        if(linkPlacement && placement) {
            bool transform = getLinkTransformValue();
            placement->setStatus(Property::Hidden,transform);
            linkPlacement->setStatus(Property::Hidden,!transform);
        }
        syncElementList();
    }
}

void LinkBaseExtension::syncElementList() {
    if(!getLinkedObjectProperty()) return;
    const auto &elements = getElementListValue();
    for(auto obj : elements) {
        auto element = dynamic_cast<LinkElement*>(obj);
        if(!element) continue;
        if(element->LinkTransform.getValue()!=getLinkTransformValue())
            element->LinkTransform.setValue(getLinkTransformValue());
        element->LinkedObject.setStatus(Property::Hidden,true);
        element->LinkedObject.setStatus(Property::Immutable,true);
        auto link = getLinkedObjectProperty();
        auto xlink = dynamic_cast<const PropertyXLink*>(link);
        if(xlink) {
            if(xlink->getValue()!=element->LinkedObject.getValue() ||
                strcmp(xlink->getSubName(),element->LinkedObject.getSubName())!=0)
            {
                element->LinkedObject.Paste(*xlink);
            }
        }else if(element->LinkedObject.getValue()!=link->getValue())
            element->LinkedObject.setValue(link->getValue());
    }
}

void LinkBaseExtension::extensionOnDocumentRestored() {
    inherited::extensionOnDocumentRestored();
    auto parent = getContainer();
    myHiddenElements.clear();
    if(parent) {
        const auto &elements = getElementListValue();
        const auto &vis = getVisibilityListValue();
        if(elements.size() && vis.size()) {
            for(size_t i=0;i<elements.size();++i) {
                if(vis.size()<=i) break;
                if(!vis[i])
                    myHiddenElements.insert(elements[i]);
            }
        }
        if(getLinkPlacementProperty())
            update(parent,getLinkPlacementProperty());
        else
            update(parent,getPlacementProperty());
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
PROPERTY_SOURCE_WITH_EXTENSIONS(App::LinkElement, App::DocumentObject)

LinkElement::LinkElement() {
    LINK_PROPS_ADD(LINK_PARAMS_ELEMENT);
    LinkBaseExtension::initExtension(this);
}

//--------------------------------------------------------------------------------
PROPERTY_SOURCE_WITH_EXTENSIONS(App::LinkGroup, App::DocumentObject)

LinkGroup::LinkGroup() {
    LINK_PROPS_ADD(LINK_PARAMS_GROUP);
    LinkBaseExtension::initExtension(this);
}

