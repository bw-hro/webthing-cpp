// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <bw/webthing/errors.hpp>
#include <bw/webthing/json_validator.hpp>
#include <bw/webthing/utils.hpp>
#include <bw/webthing/value.hpp>

namespace bw::webthing {

template<class T> class Property;

template<class T>
json property_status_message(const Property<T>& property)
{
    return json({
        {"messageType", "propertyStatus"}, 
        {"data", property_value_object(property)}
    });
}

template<class T>
json property_value_object(const Property<T>& property)
{
    auto value_as_json = property.get_value() ? json(*property.get_value()) : json();
    return json({{property.get_name(), value_as_json}});
}

class PropertyBase
{
public:

    PropertyBase(std::string name, json metadata, bool wraps_double)
        : name(name)
        , metadata(metadata)
        , wraps_double(wraps_double)
    {
        if(this->metadata.type() != json::value_t::object)
            throw PropertyError("Only json::object is allowed as meta data.");

        href = "/properties/" + this->name;
    }

    virtual ~PropertyBase() = default;
    virtual json get_property_value_object() const = 0;

    json as_property_description() const
    {
        json description = metadata;

        if(!description["links"].is_array())
            description["links"] = json::array();

        description["links"] += {{"rel", "property"}, {"href", href_prefix + href}};

        return description;
    }

    void set_href_prefix(std::string prefix)
    {
        href_prefix = prefix;
    }

    std::string get_href() const
    {
        return href_prefix +  href;
    }

    std::string get_name() const
    {
        return name;
    }

    json get_metadata() const
    {
        return metadata;
    }

    template<class T> std::optional<T> get_value() const
    {
        return dynamic_cast<const Property<T>&>(*this).get_value();
    }

    template<class T> void set_value(T value)
    {
        try{
           if(wraps_double && !std::is_same_v<T, double>)
               return set_value(try_static_cast<double>(value));

            auto property = dynamic_cast<Property<T>&>(*this);
            property.set_value(value);
        }
        catch(std::bad_cast&)
        {
            throw PropertyError("Property value type not matching");
        }
    }

protected:
    std::string name;
    std::string href_prefix;
    std::string href;
    json metadata;
    const bool wraps_double;
};

typedef std::function<void (json)> PropertyChangedCallback;

template<class T>
class Property : public PropertyBase
{
public:
    Property(PropertyChangedCallback changed_callback, std::string name, std::shared_ptr<Value<T>> value, json metadata = json::object())
        : PropertyBase(name, metadata, std::is_same_v<T, double>)
        , property_change_callback(changed_callback)
        , value(value)
    {
        // Add value change observer to notify the Thing about a property change.
        if(property_change_callback)
            this->value->add_observer([&](auto v){property_change_callback(property_status_message(*this));});
    }

    // Validate new proptery value before setting it.
    void validate_value(const T& value) const
    {
        if(metadata.contains("readOnly"))
        {
            auto json_ro = metadata["readOnly"];
            bool read_only = json_ro.is_boolean() && json_ro.template get<bool>();
            if(read_only)
                throw PropertyError("Read-only property");
        }

        try
        {
            validate_value_by_scheme(value, metadata);
        }
        catch(std::exception& ex)
        {
            throw PropertyError("Invalid property value - " + std::string(ex.what()));
        }
    }

    json get_property_value_object() const
    {
        return property_value_object(*this);
    }

    // Get the current property value.
    std::optional<T> get_value() const
    {
        return value->get();
    }

    // Set the current value of the property.
    // throws PropertyError If value could not be set.
    void set_value(T value)
    {
        this->validate_value(value);
        this->value->set(value);
    }

private:
    std::shared_ptr<Value<T>> value;
    PropertyChangedCallback property_change_callback;
};

} // bw::webthing