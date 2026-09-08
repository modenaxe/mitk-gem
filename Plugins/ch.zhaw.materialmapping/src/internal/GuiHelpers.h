#pragma once

#include <QWidget>
#include <tinyxml2.h>

#include "ui_MaterialMappingViewControls.h"

#include "BoneDensityFunctor.h"
#include "CalibrationDataModel.h"
#include "MaterialMappingFilter.h"

/**
 * Utility functions to work with the gui components that are not self managed.
 *
 *      --- CalibrationDataModel  ---
 *     |                             |
 *     |        self managed         |
 *     |                             |
 *      ---  Bone density group   ---
 *     |                             |
 *     |   *managed via GuiHelper*   |
 *     |                             |
 *      --- PowerLawWidgetManager ---
 *     |                             |
 *     |        self managed         |
 *     |                             |
 *      ---     Options group     ---
 *     |                             |
 *     |   *managed via GuiHelper*   |
 *     |                             |
 *      -----------------------------
 *     | save params |  load params  |
 *      -----------------------------
 */
namespace gui {
    /**
     * Gathers all field data needed for the rho calculation and creates a functor with them
     */
    BoneDensityFunctor createDensityFunctor(Ui::MaterialMappingViewControls &_controls, CalibrationDataModel &_dataModel);

    // get selected mapping method
    MaterialMappingFilter::Method getSelectedMappingMethod(Ui::MaterialMappingViewControls &_controls);

    // XML (de-)serialization
    tinyxml2::XMLElement* serializeDensityGroupStateToXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLDocument&);
    void loadDensityGroupStateFromXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLElement *);
    tinyxml2::XMLElement* serializeOptionsGroupStateToXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLDocument&);
    void loadOptionsGroupStateFromXml(Ui::MaterialMappingViewControls &_controls, tinyxml2::XMLElement *);

    /**
     * Convenience functions to highlight QWidgets with predefined background-colors
     */
    void setMandatoryQSSField(QWidget *widget, bool bEnabled);
    void setWarningQSSField(QWidget *widget, bool bEnabled);
    void setErrorQSSField(QWidget *widget, bool bEnabled);
    void setNamedQSSField(QWidget *widget, const char *fieldName, bool bEnabled);
}
