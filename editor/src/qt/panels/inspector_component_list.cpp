#include "NovelMind/editor/qt/panels/inspector_component_list.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/inspector_binding.hpp"
#include "NovelMind/core/property_system.hpp"

#include <QLineEdit>

namespace NovelMind::editor::qt {

// Forward declare NMSceneObject for the implementation
class NMSceneObject;

InspectorComponentList::InspectorComponentList(NMInspectorPanel* panel) : m_panel(panel) {}

void InspectorComponentList::prepareBindingTargets(const QList<NMSceneObject*>& objects,
                                                    std::vector<std::string>& objectIds,
                                                    std::vector<void*>& objectPtrs) {
  for (auto* obj : objects) {
    if (obj) {
      objectIds.push_back(obj->id().toStdString());
      objectPtrs.push_back(static_cast<void*>(obj));
    }
  }
}

void InspectorComponentList::createMultiObjectPropertyGroups(const QList<NMSceneObject*>& objects,
                                                              bool editable) {
  // Use InspectorBindingManager to handle multi-object editing
  auto& inspector = InspectorBindingManager::instance();

  // Prepare targets for binding manager
  std::vector<std::string> objectIds;
  std::vector<void*> objectPtrs;
  prepareBindingTargets(objects, objectIds, objectPtrs);

  inspector.inspectSceneObjects(objectIds, objectPtrs);

  // Get property groups from binding manager
  auto groups = inspector.getPropertyGroups();

  for (const auto& group : groups) {
    auto* uiGroup = m_panel->addGroup(QString::fromStdString(group.name));

    for (const auto* prop : group.properties) {
      if (!prop)
        continue;

      const auto& meta = prop->getMeta();

      // Skip hidden or ID properties
      if (hasFlag(meta.flags, PropertyFlags::Hidden) || meta.name == "id") {
        continue;
      }

      // Get property value (will be MultipleValues if values differ)
      auto value = inspector.getPropertyValue(meta.name);
      QString valueStr = QString::fromStdString(PropertyUtils::toString(value));

      // Determine property type
      NMPropertyType propType = NMPropertyType::String;
      switch (meta.type) {
      case PropertyType::Bool:
        propType = NMPropertyType::Boolean;
        break;
      case PropertyType::Int:
      case PropertyType::Int64:
        propType = NMPropertyType::Integer;
        break;
      case PropertyType::Float:
      case PropertyType::Double:
        propType = NMPropertyType::Float;
        break;
      case PropertyType::Vector2:
        propType = NMPropertyType::Vector2;
        break;
      case PropertyType::Vector3:
        propType = NMPropertyType::Vector3;
        break;
      case PropertyType::Color:
        propType = NMPropertyType::Color;
        break;
      case PropertyType::Enum:
        propType = NMPropertyType::Enum;
        break;
      case PropertyType::AssetRef:
        propType = NMPropertyType::Asset;
        break;
      case PropertyType::CurveRef:
        propType = NMPropertyType::Curve;
        break;
      default:
        propType = NMPropertyType::String;
        break;
      }

      if (editable && !hasFlag(meta.flags, PropertyFlags::ReadOnly)) {
        // Add editable property
        QStringList enumOptions;
        if (meta.type == PropertyType::Enum) {
          for (const auto& opt : meta.enumOptions) {
            enumOptions.append(QString::fromStdString(opt.second));
          }
        }

        if (auto* widget = uiGroup->addEditableProperty(QString::fromStdString(meta.name),
                                                        QString::fromStdString(meta.displayName),
                                                        propType, valueStr, enumOptions)) {
          // Track the widget for updates
          // Note: trackPropertyWidget is a method on NMInspectorPanel
          // We'll need to add an accessor method or make it public

          // Special styling for "multiple values" placeholder
          if (valueStr == "<multiple values>") {
            if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
              lineEdit->setPlaceholderText("<multiple values>");
              lineEdit->clear();
            }
          }
        }
      } else {
        // Read-only property
        uiGroup->addProperty(QString::fromStdString(meta.displayName), valueStr);
      }
    }
  }
}

} // namespace NovelMind::editor::qt
