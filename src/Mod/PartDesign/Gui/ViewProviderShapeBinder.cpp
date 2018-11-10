/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
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
# include <QApplication>
# include <QMessageBox>
# include <QMenu>
# include <Inventor/nodes/SoSeparator.h>
# include <TopExp.hxx>
# include <TopTools_IndexedMapOfShape.hxx>
#endif

#include <Base/Console.h>
#include <Gui/Application.h>
#include <Gui/Control.h>
#include <Gui/Document.h>

#include <Mod/PartDesign/App/ShapeBinder.h>

#include "ViewProviderShapeBinder.h"
#include "TaskShapeBinder.h"

FC_LOG_LEVEL_INIT("ShapeBinder",true,true);

using namespace PartDesignGui;

PROPERTY_SOURCE(PartDesignGui::ViewProviderShapeBinder,PartGui::ViewProviderPart)

ViewProviderShapeBinder::ViewProviderShapeBinder()
{
    sPixmap = "PartDesign_ShapeBinder.svg";

    //make the viewprovider more datum like
    AngularDeflection.setStatus(App::Property::Hidden, true);
    Deviation.setStatus(App::Property::Hidden, true);
    DrawStyle.setStatus(App::Property::Hidden, true);
    Lighting.setStatus(App::Property::Hidden, true);
    LineColor.setStatus(App::Property::Hidden, true);
    LineWidth.setStatus(App::Property::Hidden, true);
    PointColor.setStatus(App::Property::Hidden, true);
    PointSize.setStatus(App::Property::Hidden, true);
    DisplayMode.setStatus(App::Property::Hidden, true);

    //get the datum coloring sheme
    // set default color for datums (golden yellow with 60% transparency)
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath (
            "User parameter:BaseApp/Preferences/Mod/PartDesign");
    unsigned long shcol = hGrp->GetUnsigned ( "DefaultDatumColor", 0xFFD70099 );
    App::Color col ( (uint32_t) shcol );
    
    MapFaceColor.setValue(false);
    MapLineColor.setValue(false);
    MapPointColor.setValue(false);
    MapTransparency.setValue(false);
    ShapeColor.setValue(col);
    LineColor.setValue(col);
    PointColor.setValue(col);
    Transparency.setValue(60);
    LineWidth.setValue(1);
}

ViewProviderShapeBinder::~ViewProviderShapeBinder()
{

}

bool ViewProviderShapeBinder::setEdit(int ModNum) {
    // TODO Share code with other view providers (2015-09-11, Fat-Zer)
    
    if (ModNum == ViewProvider::Default || ModNum == 1) {
        // When double-clicking on the item for this pad the
        // object unsets and sets its edit mode without closing
        // the task panel
        Gui::TaskView::TaskDialog *dlg = Gui::Control().activeDialog();
        TaskDlgShapeBinder *sbDlg = qobject_cast<TaskDlgShapeBinder*>(dlg);
        if (dlg && !sbDlg) {
            QMessageBox msgBox;
            msgBox.setText(QObject::tr("A dialog is already open in the task panel"));
            msgBox.setInformativeText(QObject::tr("Do you want to close this dialog?"));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Yes)
                Gui::Control().reject();
            else
                return false;
        }

        // clear the selection (convenience)
        Gui::Selection().clearSelection();

        // start the edit dialog
        // another pad left open its task panel
        if (sbDlg)
            Gui::Control().showDialog(sbDlg);
        else
            Gui::Control().showDialog(new TaskDlgShapeBinder(this,ModNum == 1));

        return true;
    }
    else {
        return ViewProviderPart::setEdit(ModNum);
    }
}

void ViewProviderShapeBinder::unsetEdit(int ModNum) {
    
    PartGui::ViewProviderPart::unsetEdit(ModNum);
}

void ViewProviderShapeBinder::highlightReferences(const bool on, bool /*auxiliary*/)
{
    Part::Feature* obj;
    std::vector<std::string> subs;

    if(getObject()->isDerivedFrom(PartDesign::ShapeBinder::getClassTypeId()))
        PartDesign::ShapeBinder::getFilteredReferences(&static_cast<PartDesign::ShapeBinder*>(getObject())->Support, obj, subs);
    else
        return;

    PartGui::ViewProviderPart* svp = dynamic_cast<PartGui::ViewProviderPart*>(
                Gui::Application::Instance->getViewProvider(obj));
    if (svp == NULL) return;

    if (on) {
         if (!subs.empty() && originalLineColors.empty()) {
            TopTools_IndexedMapOfShape eMap;
            TopExp::MapShapes(obj->Shape.getValue(), TopAbs_EDGE, eMap);
            originalLineColors = svp->LineColorArray.getValues();
            std::vector<App::Color> lcolors = originalLineColors;
            lcolors.resize(eMap.Extent(), svp->LineColor.getValue());

            TopExp::MapShapes(obj->Shape.getValue(), TopAbs_FACE, eMap);
            originalFaceColors = svp->DiffuseColor.getValues();
            std::vector<App::Color> fcolors = originalFaceColors;
            fcolors.resize(eMap.Extent(), svp->ShapeColor.getValue());

            for (std::string e : subs) {
                // Note: stoi may throw, but it strictly shouldn't happen
                if(e.substr(4) == "Edge") {
                    int idx = std::stoi(e.substr(4)) - 1;
                    assert ( idx>=0 );
                    if ( idx < (ssize_t) lcolors.size() )
                        lcolors[idx] = App::Color(1.0,0.0,1.0); // magenta
                }
                else if(e.substr(4) == "Face")  {
                    int idx = std::stoi(e.substr(4)) - 1;
                    assert ( idx>=0 );
                    if ( idx < (ssize_t) fcolors.size() )
                        fcolors[idx] = App::Color(1.0,0.0,1.0); // magenta
                }
            }
            svp->LineColorArray.setValues(lcolors);
            svp->DiffuseColor.setValues(fcolors);
        }
    } else {
        if (!subs.empty() && !originalLineColors.empty()) {
            svp->LineColorArray.setValues(originalLineColors);
            originalLineColors.clear();
            
            svp->DiffuseColor.setValues(originalFaceColors);
            originalFaceColors.clear();
        }
    }
}

void ViewProviderShapeBinder::setupContextMenu(QMenu* menu, QObject* receiver, const char* member) 
{
    QAction* act;
    act = menu->addAction(QObject::tr("Edit shape binder"), receiver, member);
    act->setData(QVariant((int)ViewProvider::Default));
}

//=====================================================================================

PROPERTY_SOURCE(PartDesignGui::ViewProviderSubShapeBinder,PartGui::ViewProviderPart)

ViewProviderSubShapeBinder::ViewProviderSubShapeBinder() {
    sPixmap = "PartDesign_SubShapeBinder.svg";

    //get the datum coloring sheme
    // set default color for datums (golden yellow with 60% transparency)
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath (
            "User parameter:BaseApp/Preferences/Mod/PartDesign");
    unsigned long shcol = hGrp->GetUnsigned ( "DefaultDatumColor", 0xFFD70099 );
    App::Color col ( (uint32_t) shcol );
    
    MapFaceColor.setValue(false);
    MapLineColor.setValue(false);
    MapPointColor.setValue(false);
    MapTransparency.setValue(false);
    ShapeColor.setValue(col);
    LineColor.setValue(col);
    PointColor.setValue(col);
    Transparency.setValue(60);
    LineWidth.setValue(1);
}

bool ViewProviderSubShapeBinder::canDropObjectEx(App::DocumentObject *obj, 
        App::DocumentObject *owner, const char *, const std::vector<std::string> &) const
{
    auto self = dynamic_cast<PartDesign::SubShapeBinder*>(getObject());
    if(!self) return false;
    auto doc = getDocument()->getDocument();
    if(self->Relative.getValue()) {
        if(!owner)
            owner = obj;
        return owner->getDocument()==doc;
    }else
        return obj->getDocument()==doc;
}

std::string ViewProviderSubShapeBinder::dropObjectEx(App::DocumentObject *obj, App::DocumentObject *owner,
        const char *subname, const std::vector<std::string> &elements)
{
    auto self = dynamic_cast<PartDesign::SubShapeBinder*>(getObject());
    if(!self) return std::string();
    if(!subname) subname = "";
    std::vector<std::string> subs;
    if(elements.size()) {
        subs.reserve(elements.size());
        std::string sub(subname);
        for(auto &element : elements)
            subs.push_back(sub+element);
    }

    self->setLinks(owner?owner:obj,subs,QApplication::keyboardModifiers()==Qt::ControlModifier);
    if(self->Relative.getValue())
        updatePlacement(false);
    return std::string();
}


bool ViewProviderSubShapeBinder::doubleClicked() {
    updatePlacement(true);
    return true;
}

void ViewProviderSubShapeBinder::updatePlacement(bool transaction) {
    auto self = dynamic_cast<PartDesign::SubShapeBinder*>(getObject());
    if(!self || !self->Support.getValue())
        return;

    Base::Matrix4D mat;
    bool relative = self->Relative.getValue();
    if(relative) {
        const auto &sel = Gui::Selection().getSelection("",0);
        if(sel.empty() || !sel[0].pObject) {
            FC_LOG("invalid selection");
            return;
        }
        auto link = self->Support.getValue();
        std::string subname(sel[0].SubName?sel[0].SubName:"");
        std::string linkSub;
        auto obj = sel[0].pObject->resolveRelativeLink(subname,link,linkSub);
        if(!obj) {
            if(!link) {
                FC_ERR("cannot resolve relative link");
                return;
            }
        }else{
            auto sobj = obj->getSubObject(subname.c_str(),0,&mat);
            if(sobj!=self) {
                FC_ERR("invalid selection " << subname);
                return;
            }
        }
    }
    if(!transaction) {
        if(relative)
            self->updatePlacement(mat);
        self->update();
        return;
    }

    App::GetApplication().setActiveTransaction("Refresh SubShapeBinder");
    try{
        if(relative)
            self->updatePlacement(mat);
        self->update();
        App::GetApplication().closeActiveTransaction();
    }catch(Base::Exception &e) {
        e.ReportException();
    }catch(Standard_Failure &e) {
        std::ostringstream str;
        Standard_CString msg = e.GetMessageString();
        str << typeid(e).name() << " ";
        if (msg) {str << msg;}
        else     {str << "No OCCT Exception Message";}
        FC_ERR(str.str());
    }
    App::GetApplication().closeActiveTransaction(true);
}

std::vector<App::DocumentObject*> ViewProviderSubShapeBinder::claimChildren(void) const {
    std::vector<App::DocumentObject *> ret;
    auto self = dynamic_cast<PartDesign::SubShapeBinder*>(getObject());
    if(self && self->ClaimChildren.getValue() && self->Support.getValue())
        ret.push_back(self->Support.getValue());
    return ret;
}

