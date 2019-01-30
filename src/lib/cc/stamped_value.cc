// Copyright (C) 2018-2019 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cc/stamped_value.h>
#include <exceptions/exceptions.h>
#include <boost/lexical_cast.hpp>

namespace isc {
namespace data {

StampedValue::StampedValue(const std::string& name)
    : StampedElement(), name_(name), value_() {
}

StampedValue::StampedValue(const std::string& name, const ElementPtr& value)
    : StampedElement(), name_(name), value_(value) {
    validateConstruct();
}

StampedValue::StampedValue(const std::string& name, const std::string& value)
    : StampedElement(), name_(name), value_(Element::create(value)) {
    validateConstruct();
}

StampedValuePtr
StampedValue::create(const std::string& name) {
    return (StampedValuePtr(new StampedValue(name)));
}

StampedValuePtr
StampedValue::create(const std::string& name, const ElementPtr& value) {
    return (StampedValuePtr(new StampedValue(name, value)));
}

StampedValuePtr
StampedValue::create(const std::string& name, const std::string& value) {
    return (StampedValuePtr(new StampedValue(name, value)));
}

int
StampedValue::getType() const {
    if (!value_) {
        isc_throw(InvalidOperation, "StampedValue: attempt to retrieve the "
                  "type of the null value for the '" << name_
                  << "' parameter");
    }

    return (value_->getType());
}

std::string
StampedValue::getValue() const {
    validateAccess(Element::string);

    try {
        switch (static_cast<Element::types>(value_->getType())) {
        case Element::string:
            return (value_->stringValue());
        case Element::integer:
            return (boost::lexical_cast<std::string>(value_->intValue()));
        case Element::boolean:
            return (value_->boolValue() ? "1" : "0");
        case Element::real:
            return (boost::lexical_cast<std::string>(value_->doubleValue()));
        default:
            // Impossible condition.
            isc_throw(TypeError, "StampedValue: invalid type of the '"
                      << name_ << "' parameter");
        }

    } catch (const boost::bad_lexical_cast& ex) {
        isc_throw(BadValue, "StampedValue: unable to convert the value of "
                  "the parameter '" << name_ << "' to string");
    }
    return (value_->stringValue());
}

int64_t
StampedValue::getSignedIntegerValue() const {
    validateAccess(Element::integer);
    return (value_->intValue());
}

bool
StampedValue::getBoolValue() const {
    validateAccess(Element::boolean);
    return (value_->boolValue());
}

double
StampedValue::getDoubleValue() const {
    validateAccess(Element::real);
    return (value_->doubleValue());
}

void
StampedValue::validateConstruct() const {
    if (!value_) {
        isc_throw(BadValue, "StampedValue: provided value of the '"
                  << name_ << "' parameter is NULL");
    }

    if ((value_->getType() != Element::string) &&
        (value_->getType() != Element::integer) &&
        (value_->getType() != Element::boolean) &&
        (value_->getType() != Element::real)) {
        isc_throw(TypeError, "StampedValue: provided value of the '"
                  << name_ << "' parameter has invalid type: "
                  << Element::typeToName(static_cast<Element::types>(value_->getType())));
    }
}

void
StampedValue::validateAccess(Element::types type) const {
    if (!value_) {
        isc_throw(InvalidOperation, "StampedValue: attempt to get null value "
                  "of the '" << name_ << "' parameter");
    }

    if ((type != Element::string) && (type != value_->getType())) {
        isc_throw(TypeError, "StampedValue: attempt to access a '"
                  << name_ << "' parameter as " << Element::typeToName(type)
                  << ", but this parameter has "
                  << Element::typeToName(static_cast<Element::types>(value_->getType()))
                  << " type");
    }
}

ElementPtr
StampedValue::toElement(Element::types elem_type) {
    ElementPtr element;
    switch(elem_type) {
    case Element::string: {
        element.reset(new StringElement(value_));
        break;
    }
    case Element::integer: {
        try {
            int64_t int_value = boost::lexical_cast<int64_t>(value_);
            element.reset(new IntElement(int_value));
        } catch (const std::exception& ex) {
            isc_throw(BadValue, "StampedValue::toElement:  integer value expected for: "
                                 << name_ << ", value is: " << value_ );
        }
        break;
    }
    case Element::boolean: {
        bool bool_value;
        if (value_ == std::string("true")) {
            bool_value = true;
        } else if (value_ == std::string("false")) {
            bool_value = false;
        } else {
            isc_throw(BadValue, "StampedValue::toElement: boolean value specified as "
                      << name_ << ", value is: " << value_
                      << ", expected true or false");
        }

        element.reset(new BoolElement(bool_value));
        break;
    }
    case Element::real: {
        try {
            double dbl_value = boost::lexical_cast<double>(value_);
            element.reset(new DoubleElement(dbl_value));
        }
        catch (const std::exception& ex) {
            isc_throw(BadValue, "StampedValue::toElement: real number value expected for: "
                      << name_ << ", value is: " << value_ );
        }

    break;
    }
    default:
        isc_throw (BadValue, "StampedValue::toElement: unsupported element type "
                   << elem_type << " for: " << name_);
        break;
    }

    return (element);
}

} // end of namespace isc::data
} // end of namespace isc
