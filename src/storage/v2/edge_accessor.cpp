#include "storage/v2/edge_accessor.hpp"

#include <memory>

#include "storage/v2/mvcc.hpp"
#include "storage/v2/vertex_accessor.hpp"

namespace storage {

VertexAccessor EdgeAccessor::FromVertex() const {
  return VertexAccessor{from_vertex_, transaction_, indices_};
}

VertexAccessor EdgeAccessor::ToVertex() const {
  return VertexAccessor{to_vertex_, transaction_, indices_};
}

Result<bool> EdgeAccessor::SetProperty(PropertyId property,
                                       const PropertyValue &value) {
  std::lock_guard<utils::SpinLock> guard(edge_->lock);

  if (!PrepareForWrite(transaction_, edge_))
    return Error::SERIALIZATION_ERROR;

  if (edge_->deleted) return Error::DELETED_OBJECT;

  auto it = edge_->properties.find(property);
  bool existed = it != edge_->properties.end();
  // We could skip setting the value if the previous one is the same to the new
  // one. This would save some memory as a delta would not be created as well as
  // avoid copying the value. The reason we are not doing that is because the
  // current code always follows the logical pattern of "create a delta" and
  // "modify in-place". Additionally, the created delta will make other
  // transactions get a SERIALIZATION_ERROR.
  if (it != edge_->properties.end()) {
    CreateAndLinkDelta(transaction_, edge_, Delta::SetPropertyTag(), property,
                       it->second);
    if (value.IsNull()) {
      // remove the property
      edge_->properties.erase(it);
    } else {
      // set the value
      it->second = value;
    }
  } else {
    CreateAndLinkDelta(transaction_, edge_, Delta::SetPropertyTag(), property,
                       PropertyValue());
    if (!value.IsNull()) {
      edge_->properties.emplace(property, value);
    }
  }

  return !existed;
}

Result<PropertyValue> EdgeAccessor::GetProperty(PropertyId property,
                                                View view) const {
  bool deleted = false;
  PropertyValue value;
  Delta *delta = nullptr;
  {
    std::lock_guard<utils::SpinLock> guard(edge_->lock);
    deleted = edge_->deleted;
    auto it = edge_->properties.find(property);
    if (it != edge_->properties.end()) {
      value = it->second;
    }
    delta = edge_->delta;
  }
  ApplyDeltasForRead(transaction_, delta, view,
                     [&deleted, &value, property](const Delta &delta) {
                       switch (delta.action) {
                         case Delta::Action::SET_PROPERTY: {
                           if (delta.property.key == property) {
                             value = delta.property.value;
                           }
                           break;
                         }
                         case Delta::Action::DELETE_OBJECT: {
                           LOG(FATAL) << "Invalid accessor!";
                           break;
                         }
                         case Delta::Action::RECREATE_OBJECT: {
                           deleted = false;
                           break;
                         }
                         case Delta::Action::ADD_LABEL:
                         case Delta::Action::REMOVE_LABEL:
                         case Delta::Action::ADD_IN_EDGE:
                         case Delta::Action::ADD_OUT_EDGE:
                         case Delta::Action::REMOVE_IN_EDGE:
                         case Delta::Action::REMOVE_OUT_EDGE:
                           break;
                       }
                     });
  if (deleted) return Error::DELETED_OBJECT;
  return std::move(value);
}

Result<std::map<PropertyId, PropertyValue>> EdgeAccessor::Properties(
    View view) const {
  std::map<PropertyId, PropertyValue> properties;
  bool deleted = false;
  Delta *delta = nullptr;
  {
    std::lock_guard<utils::SpinLock> guard(edge_->lock);
    deleted = edge_->deleted;
    properties = edge_->properties;
    delta = edge_->delta;
  }
  ApplyDeltasForRead(
      transaction_, delta, view, [&deleted, &properties](const Delta &delta) {
        switch (delta.action) {
          case Delta::Action::SET_PROPERTY: {
            auto it = properties.find(delta.property.key);
            if (it != properties.end()) {
              if (delta.property.value.IsNull()) {
                // remove the property
                properties.erase(it);
              } else {
                // set the value
                it->second = delta.property.value;
              }
            } else if (!delta.property.value.IsNull()) {
              properties.emplace(delta.property.key, delta.property.value);
            }
            break;
          }
          case Delta::Action::DELETE_OBJECT: {
            LOG(FATAL) << "Invalid accessor!";
            break;
          }
          case Delta::Action::RECREATE_OBJECT: {
            deleted = false;
            break;
          }
          case Delta::Action::ADD_LABEL:
          case Delta::Action::REMOVE_LABEL:
          case Delta::Action::ADD_IN_EDGE:
          case Delta::Action::ADD_OUT_EDGE:
          case Delta::Action::REMOVE_IN_EDGE:
          case Delta::Action::REMOVE_OUT_EDGE:
            break;
        }
      });
  if (deleted) {
    return Error::DELETED_OBJECT;
  }
  return std::move(properties);
}

}  // namespace storage
