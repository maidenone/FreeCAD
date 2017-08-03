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


#ifndef GUI_VIEWPROVIDER_LINK_H
#define GUI_VIEWPROVIDER_LINK_H

#include <boost/intrusive_ptr.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <App/PropertyGeo.h>
#include <App/Link.h>
#include "SoFCUnifiedSelection.h"
#include "ViewProviderPythonFeature.h"
#include "ViewProviderDocumentObject.h"
#include "ViewProviderExtension.h"

class SoBase;

namespace Gui {

// Convenience smart pointer to wrap coin node. It is basically
// boost::intrusive plus implicit pointer conversion to save the trouble of
// typing get() all the time.
template<class T>
class CoinPtr: public boost::intrusive_ptr<T> {
public:
    // Too bad, VC2013 does not support constructor inheritance
    //using boost::intrusive_ptr<T>::intrusive_ptr;
    typedef boost::intrusive_ptr<T> inherited;
    CoinPtr() {}
    CoinPtr(T *p, bool add_ref=true):inherited(p,add_ref){}
    template<class Y> CoinPtr(CoinPtr<Y> const &r):inherited(r){}

    operator T *() const {
        return this->get();
    }
};

class LinkInfo;
typedef boost::intrusive_ptr<LinkInfo> LinkInfoPtr;

class GuiExport ViewProviderLinkObserver: public ViewProviderExtension {
    EXTENSION_TYPESYSTEM_HEADER_WITH_OVERRIDE();
public:
    ViewProviderLinkObserver();
    void extensionBeforeDelete() override;
    void extensionOnChanged(const App::Property *) override;
    void extensionUpdateData(const App::Property*) override;
    void extensionHide(void) override;
    void extensionShow(void) override;
    void extensionFinishRestoring() override;
    bool extensionCanDragObject(App::DocumentObject*) const { return false; }
    bool extensionCanDropObject(App::DocumentObject*) const { return false; }

    LinkInfoPtr linkInfo;
};

class GuiExport LinkHandle : public Base::BaseClass {
    TYPESYSTEM_HEADER();
public:

    LinkHandle();
    ~LinkHandle();

    void unlink(LinkInfoPtr link);
    bool isLinked() const;

    SoFCSelectionRoot *getLinkRoot() const {return pcLinkRoot;}

    QIcon getLinkedIcon(QPixmap overlay) const;

    virtual void onLinkUpdate();
    virtual void onLinkedIconChange(LinkInfoPtr link);

    void setLink(App::DocumentObject *obj,
        const std::vector<std::string> &subs = std::vector<std::string>()); 

    void setMaterial(int index, const App::Material *material);
    void setTransform(int index, const Base::Matrix4D &mat);
    void setSize(int size);

    int getSize() const { return nodeArray.size(); }

    static void setTransform(SoTransform *pcTransform, const Base::Matrix4D &mat);

    enum SnapshotType {
        //three type of snapshot to override linked root node:
        
        //override transform and visibility
        SnapshotTransform = 0,
        //override visibility
        SnapshotVisible = 1,
        //override none (for child objects of a container)
        SnapshotChild = 2,

        SnapshotMax,

        //special type for subelement linking
        SnapshotContainer = -1,
        // subelement linking with transform override
        SnapshotContainerTransform = -2,
    };
    void setNodeType(SnapshotType type, bool sublink=true);

    void setVisibility(bool visible);
    bool getVisibility() const {return visible;}

    ViewProviderDocumentObject *linkGetLinkedView(bool recursive, int depth=0) const;
    bool linkGetDetailPath(const char *, SoFullPath *, SoDetail *&) const;
    bool linkGetElementPicked(const SoPickedPoint *, std::string &) const;

    int setElementVisible(int index, bool visible);
    bool isElementVisible(int index) const;

    ViewProviderDocumentObject *getOwner() const {return owner;}
    void setOwner(ViewProviderDocumentObject *vpd) {owner=vpd;}

    bool hasSubs() const;

    friend class LinkInfo;

protected:
    void replaceLinkedRoot(SoSeparator *);

protected:
    ViewProviderDocumentObject *owner;
    CoinPtr<SoFCSelectionRoot> pcLinkRoot;
    CoinPtr<SoSeparator> pcLinkedRoot;
    CoinPtr<SoMaterial> pcMaterial;
    SnapshotType nodeType;
    bool autoSubLink; //auto delegate to linked sub object if there is only one sub object
    LinkInfoPtr linkInfo;
    struct SubInfo {
        LinkHandle &handle;
        CoinPtr<SoSeparator> pcNode;
        CoinPtr<SoTransform> pcTransform;
        LinkInfoPtr link;
        std::set<std::string> subElements;

        SubInfo(LinkHandle &handle);
        ~SubInfo();
        void unlink(bool reset=false);
        bool isLinked() const;
    };
    std::map<std::string, SubInfo> subInfo;

    struct Element {
        CoinPtr<SoSwitch> pcSwitch;
        CoinPtr<SoFCSelectionRoot> pcRoot;
        CoinPtr<SoMaterial> pcMaterial;
        CoinPtr<SoTransform> pcTransform;
    };
    std::vector<Element> nodeArray;
    std::map<SoNode*,int> nodeMap;

    bool visible;
};

class ViewProviderLinkElement;
    
class GuiExport ViewProviderLink : public ViewProviderDocumentObject
{
    PROPERTY_HEADER(Gui::ViewProviderLink);
    typedef ViewProviderDocumentObject inherited;

public:
    App::PropertyBool LinkUseMaterial;
    App::PropertyMaterial LinkShapeMaterial;
    App::PropertyMaterialList MaterialList;
    App::PropertyBoolList UseMaterialList;
    App::PropertyBool Selectable;

    ViewProviderLink();
    virtual ~ViewProviderLink();

    void attach(App::DocumentObject *pcObj) override;

    bool isSelectable(void) const override {return Selectable.getValue();}

    bool useNewSelectionModel(void) const override {return true;}

    void updateData(const App::Property*) override;
    void onChanged(const App::Property* prop) override;
    std::vector<App::DocumentObject*> claimChildren(void) const override;
    bool getElementPicked(const SoPickedPoint *, std::string &) const override;
    SoDetail* getDetailPath(const char *, SoFullPath *, bool) const override;

    void finishRestoring() override;

    ViewProviderDocumentObject *getLinkedView(bool recursive, int depth=0) const override;

    QIcon getIcon(void) const override;

    bool canDragObjects() const override;
    bool canDragObject(App::DocumentObject*) const override;
    void dragObject(App::DocumentObject*) override;
    bool canDropObjects() const override;
    bool canDragAndDropObject(App::DocumentObject*) const override;
    bool canDropObjectEx(App::DocumentObject *obj, App::DocumentObject *owner, const char *subname) const override;
    void dropObjectEx(App::DocumentObject*, App::DocumentObject*, const char *) override;

    int setElementVisible(const char * /*element*/, bool /*visible*/) override;
    int isElementVisible(const char * /*element*/) const  override;

    bool hasChildElement() const override;

    std::vector<std::string> getDisplayModes(void) const override;

    virtual QPixmap getOverlayPixmap() const;

protected:
    enum LinkType {
        LinkTypeNone,
        LinkTypeNormal,
        LinkTypeX,
        LinkTypeSubs,
        LinkTypeXSubs,
    };

    bool hasElements(const App::LinkBaseExtension *ext = 0) const;
    const App::LinkBaseExtension *getLinkExtension() const;
    App::LinkBaseExtension *getLinkExtension();

    void updateDataPrivate(App::LinkBaseExtension *ext, const App::Property*);
    void updateElementChildren(App::LinkBaseExtension *ext);

    bool setLinkType(App::LinkBaseExtension *ext);

    QIcon getLinkedIcon() const;
    void onChangeIcon() const;
    std::vector<App::DocumentObject*> claimChildrenPrivate(void) const;

protected:
    ViewProviderDocumentObject *prevLinked;
    LinkHandle handle;
    std::vector<App::DocumentObject*> childrenCache;
    LinkType linkType;
    bool linkTransform;

    friend class ViewProviderLinkElement;
};

typedef ViewProviderPythonFeatureT<ViewProviderLink> ViewProviderLinkPython;

class ViewProviderLinkElement: public ViewProviderDocumentObject {
    PROPERTY_HEADER(Gui::ViewProviderLinkElement);
    typedef ViewProviderDocumentObject inherited;
public:
    App::PropertyMaterial ShapeMaterial;
    App::PropertyBool UseMaterial;

    ViewProviderLinkElement();
    void onChanged(const App::Property* prop) override;
    bool onDelete(const std::vector<std::string> &) override {return false;}
    ViewProviderDocumentObject *getLinkedView(bool recursive, int depth=0) const override;
    QIcon getIcon(void) const override;
    std::vector<App::DocumentObject*> claimChildren(void) const override;

private:
    ViewProviderLink *owner;
    int index;
    friend class ViewProviderLink;
};

} //namespace Gui

#ifdef _MSC_VER
// forward decleration to please VC 2013
void intrusive_ptr_add_ref(Gui::LinkInfo *px);
void intrusive_ptr_release(Gui::LinkInfo *px);
#endif

#endif // GUI_VIEWPROVIDER_LINK_H
