#include <cassert>
#include "GuiHelpers.h"

BoneDensityFunctor gui::createDensityFunctor(Ui::MaterialMappingViewControls &_controls, CalibrationDataModel &_dataModel) {
    BoneDensityFunctor ret;
    if(_controls.automaticFitCheckBox->isChecked()){
        ret.SetRhoCt(_dataModel.getFittedLine());
    } else {
        auto rhoCt_slope = _controls.linEQSlopeSpinBox->value();
        auto rhoCt_offset = _controls.linEQOffsetSpinBox->value();
        BoneDensityParameters::RhoCt rhoCt(rhoCt_slope, rhoCt_offset);
        ret.SetRhoCt(rhoCt);
    }

    if (_controls.rhoAshCheckBox->isChecked()) {
        auto rhoAsh_offset = _controls.rhoAshOffsetSpinBox->value();
        auto rhoAsh_divisor = _controls.rhoAshDivisorSpinBox->value();
        BoneDensityParameters::RhoAsh rhoAsh(rhoAsh_offset, rhoAsh_divisor);
        ret.SetRhoAsh(rhoAsh);

        if (_controls.rhoAppCheckBox->isChecked()) {
            auto rhoApp_divisor = _controls.rhoAppDivisorSpinBox->value();
            BoneDensityParameters::RhoApp rhoApp(rhoApp_divisor);
            ret.SetRhoApp(rhoApp);
        }
    }
    return ret;
}

MaterialMappingFilter::Method gui::getSelectedMappingMethod(Ui::MaterialMappingViewControls &_controls) {
    if(_controls.oldMethodRadioButton->isChecked()){
        return MaterialMappingFilter::Method::Old;
    }
    assert(_controls.newMethodRadioButton->isChecked());
    return MaterialMappingFilter::Method::New;
}

void gui::setNamedQSSField(QWidget *widget, const char *fieldName, bool bEnabled) {
    widget->setProperty(fieldName, bEnabled);
    widget->style()->unpolish(widget); // need to do this since we changed the stylesheet
    widget->style()->polish(widget);
    widget->update();
}

void gui::setMandatoryQSSField(QWidget *widget, bool bEnabled) {
    setNamedQSSField(widget, "mandatoryField", bEnabled);
}

void gui::setWarningQSSField(QWidget *widget, bool bEnabled) {
    setNamedQSSField(widget, "warningField", bEnabled);
}

void gui::setErrorQSSField(QWidget *widget, bool bEnabled) {
    setNamedQSSField(widget, "errorField", bEnabled);
}

tinyxml2::XMLElement *gui::serializeDensityGroupStateToXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLDocument &document) {
    auto root = document.NewElement("BoneDensityParameters");

    auto rhoCt = document.NewElement("RhoCT");
    rhoCt->SetAttribute("AutomaticFit", _controls.automaticFitCheckBox->isChecked());
    rhoCt->SetAttribute("slope", _controls.linEQSlopeSpinBox->value());
    rhoCt->SetAttribute("offset", _controls.linEQOffsetSpinBox->value());

    auto rhoAsh = document.NewElement("RhoAsh");
    rhoAsh->SetAttribute("enabled", _controls.rhoAshCheckBox->isChecked());
    rhoAsh->SetAttribute("offset", _controls.rhoAshOffsetSpinBox->value());
    rhoAsh->SetAttribute("divisor", _controls.rhoAshDivisorSpinBox->value());

    auto rhoApp = document.NewElement("RhoApp");
    rhoApp->SetAttribute("enabled", _controls.rhoAppCheckBox->isChecked());
    rhoApp->SetAttribute("divisor", _controls.rhoAppDivisorSpinBox->value());

    root->InsertEndChild(rhoCt);
    root->InsertEndChild(rhoAsh);
    root->InsertEndChild(rhoApp);

    return root;
}

void gui::loadDensityGroupStateFromXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLElement *_root) {
    bool b;
    double d;
    int ret;

    auto rhoCt = _root->FirstChildElement("RhoCT");
    ret = rhoCt->QueryBoolAttribute("AutomaticFit", &b);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.automaticFitCheckBox->setChecked(b);

        if(!b){
            ret = rhoCt->QueryDoubleAttribute("slope", &d);
            if (ret == tinyxml2::XML_SUCCESS) {
                _controls.linEQSlopeSpinBox->setValue(d);
            }
            ret = rhoCt->QueryDoubleAttribute("offset", &d);
            if (ret == tinyxml2::XML_SUCCESS) {
                _controls.linEQOffsetSpinBox->setValue(d);
            }
        }
    }

    auto rhoAsh = _root->FirstChildElement("RhoAsh");
    ret = rhoAsh->QueryBoolAttribute("enabled", &b);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.rhoAshCheckBox->setChecked(b);
    }
    ret = rhoAsh->QueryDoubleAttribute("offset", &d);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.rhoAshOffsetSpinBox->setValue(d);
    }
    ret = rhoAsh->QueryDoubleAttribute("divisor", &d);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.rhoAshDivisorSpinBox->setValue(d);
    }

    auto rhoApp = _root->FirstChildElement("RhoApp");
    ret = rhoApp->QueryBoolAttribute("enabled", &b);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.rhoAppCheckBox->setChecked(b);
    }
    ret = rhoApp->QueryDoubleAttribute("divisor", &d);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.rhoAppDivisorSpinBox->setValue(d);
    }
}

tinyxml2::XMLElement *gui::serializeOptionsGroupStateToXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLDocument &document) {
    auto root = document.NewElement("Options");
    root->SetAttribute("doPeel", _controls.uParamCheckBox->isChecked());
    root->SetAttribute("numberOfExtends", _controls.eParamSpinBox->value());
    root->SetAttribute("minValue", _controls.fParamSpinBox->value());

    return root;
}

void gui::loadOptionsGroupStateFromXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLElement *_root) {
    bool b;
    int i;
    double d;

    auto ret = _root->QueryBoolAttribute("doPeel", &b);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.uParamCheckBox->setChecked(b);
    }

    ret = _root->QueryIntAttribute("numberOfExtends", &i);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.eParamSpinBox->setValue(i);
    }

    ret = _root->QueryDoubleAttribute("minValue", &d);
    if (ret == tinyxml2::XML_SUCCESS) {
        _controls.fParamSpinBox->setValue(d);
    }
}
