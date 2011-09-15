// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <Python.h>

#include <util/python/pycppwrapper_util.h>

#include <dns/rrset.h>
#include <dns/name.h>
#include <dns/messagerenderer.h>

#include "name_python.h"
#include "pydnspp_common.h"
#include "rrset_python.h"
#include "rrclass_python.h"
#include "rrtype_python.h"
#include "rrttl_python.h"
#include "rdata_python.h"
#include "messagerenderer_python.h"

using namespace isc::dns;
using namespace isc::dns::python;
using namespace isc::util;
using namespace isc::util::python;

namespace {

// The s_* Class simply coverst one instantiation of the object

// Using a shared_ptr here should not really be necessary (PyObject
// is already reference-counted), however internally on the cpp side,
// not doing so might result in problems, since we can't copy construct
// rdata field, adding them to rrsets results in a problem when the
// rrset is destroyed later
class s_RRset : public PyObject {
public:
    isc::dns::RRsetPtr cppobj;
};


// Shortcut type which would be convenient for adding class variables safely.
typedef CPPPyObjectContainer<s_RRset, RRset> RRsetContainer;

int RRset_init(s_RRset* self, PyObject* args);
void RRset_destroy(s_RRset* self);

PyObject* RRset_getRdataCount(s_RRset* self);
PyObject* RRset_getName(s_RRset* self);
PyObject* RRset_getClass(s_RRset* self);
PyObject* RRset_getType(s_RRset* self);
PyObject* RRset_getTTL(s_RRset* self);
PyObject* RRset_setName(s_RRset* self, PyObject* args);
PyObject* RRset_setTTL(s_RRset* self, PyObject* args);
PyObject* RRset_toText(s_RRset* self);
PyObject* RRset_str(PyObject* self);
PyObject* RRset_toWire(s_RRset* self, PyObject* args);
PyObject* RRset_addRdata(s_RRset* self, PyObject* args);
PyObject* RRset_getRdata(s_RRset* self);
PyObject* RRset_removeRRsig(s_RRset* self);

// TODO: iterator?

PyMethodDef RRset_methods[] = {
    { "get_rdata_count", reinterpret_cast<PyCFunction>(RRset_getRdataCount), METH_NOARGS,
      "Returns the number of rdata fields." },
    { "get_name", reinterpret_cast<PyCFunction>(RRset_getName), METH_NOARGS,
      "Returns the name of the RRset, as a Name object." },
    { "get_class", reinterpret_cast<PyCFunction>(RRset_getClass), METH_NOARGS,
      "Returns the class of the RRset as an RRClass object." },
    { "get_type", reinterpret_cast<PyCFunction>(RRset_getType), METH_NOARGS,
      "Returns the type of the RRset as an RRType object." },
    { "get_ttl", reinterpret_cast<PyCFunction>(RRset_getTTL), METH_NOARGS,
      "Returns the TTL of the RRset as an RRTTL object." },
    { "set_name", reinterpret_cast<PyCFunction>(RRset_setName), METH_VARARGS,
      "Sets the name of the RRset.\nTakes a Name object as an argument." },
    { "set_ttl", reinterpret_cast<PyCFunction>(RRset_setTTL), METH_VARARGS,
      "Sets the TTL of the RRset.\nTakes an RRTTL object as an argument." },
    { "to_text", reinterpret_cast<PyCFunction>(RRset_toText), METH_NOARGS,
      "Returns the text representation of the RRset as a string" },
    { "to_wire", reinterpret_cast<PyCFunction>(RRset_toWire), METH_VARARGS,
      "Converts the RRset object to wire format.\n"
      "The argument can be either a MessageRenderer or an object that "
      "implements the sequence interface. If the object is mutable "
      "(for instance a bytearray()), the wire data is added in-place.\n"
      "If it is not (for instance a bytes() object), a new object is "
      "returned" },
    { "add_rdata", reinterpret_cast<PyCFunction>(RRset_addRdata), METH_VARARGS,
      "Adds the rdata for one RR to the RRset.\nTakes an Rdata object as an argument" },
    { "get_rdata", reinterpret_cast<PyCFunction>(RRset_getRdata), METH_NOARGS,
      "Returns a List containing all Rdata elements" },
    { "remove_rrsig", reinterpret_cast<PyCFunction>(RRset_removeRRsig), METH_NOARGS,
      "Clears the list of RRsigs for this RRset" },
    { NULL, NULL, 0, NULL }
};

int
RRset_init(s_RRset* self, PyObject* args) {
    PyObject* name;
    PyObject* rrclass;
    PyObject* rrtype;
    PyObject* rrttl;

    if (PyArg_ParseTuple(args, "O!O!O!O!", &name_type, &name,
                                           &rrclass_type, &rrclass,
                                           &rrtype_type, &rrtype,
                                           &rrttl_type, &rrttl
       )) {
        self->cppobj = RRsetPtr(new RRset(PyName_ToName(name),
                                          PyRRClass_ToRRClass(rrclass),
                                          PyRRType_ToRRType(rrtype),
                                          PyRRTTL_ToRRTTL(rrttl)));
        return (0);
    }

    self->cppobj = RRsetPtr();
    return (-1);
}

void
RRset_destroy(s_RRset* self) {
    // Clear the shared_ptr so that its reference count is zero
    // before we call tp_free() (there is no direct release())
    self->cppobj.reset();
    Py_TYPE(self)->tp_free(self);
}

PyObject*
RRset_getRdataCount(s_RRset* self) {
    return (Py_BuildValue("I", self->cppobj->getRdataCount()));
}

PyObject*
RRset_getName(s_RRset* self) {
    return (createNameObject(self->cppobj->getName()));
}

PyObject*
RRset_getClass(s_RRset* self) {
    return (createRRClassObject(self->cppobj->getClass()));
}

PyObject*
RRset_getType(s_RRset* self) {
    return (createRRTypeObject(self->cppobj->getType()));
}

PyObject*
RRset_getTTL(s_RRset* self) {
    return (createRRTTLObject(self->cppobj->getTTL()));
}

PyObject*
RRset_setName(s_RRset* self, PyObject* args) {
    PyObject* name;
    if (!PyArg_ParseTuple(args, "O!", &name_type, &name)) {
        return (NULL);
    }
    self->cppobj->setName(PyName_ToName(name));
    Py_RETURN_NONE;
}

PyObject*
RRset_setTTL(s_RRset* self, PyObject* args) {
    PyObject* rrttl;
    if (!PyArg_ParseTuple(args, "O!", &rrttl_type, &rrttl)) {
        return (NULL);
    }
    self->cppobj->setTTL(PyRRTTL_ToRRTTL(rrttl));
    Py_RETURN_NONE;
}

PyObject*
RRset_toText(s_RRset* self) {
    try {
        return (Py_BuildValue("s", self->cppobj->toText().c_str()));
    } catch (const EmptyRRset& ers) {
        PyErr_SetString(po_EmptyRRset, ers.what());
        return (NULL);
    }
}

PyObject*
RRset_str(PyObject* self) {
    // Simply call the to_text method we already defined
    return (PyObject_CallMethod(self,
                               const_cast<char*>("to_text"),
                                const_cast<char*>("")));
}

PyObject*
RRset_toWire(s_RRset* self, PyObject* args) {
    PyObject* bytes;
    PyObject* mr;

    try {
        if (PyArg_ParseTuple(args, "O", &bytes) && PySequence_Check(bytes)) {
            PyObject* bytes_o = bytes;

            OutputBuffer buffer(4096);
            self->cppobj->toWire(buffer);
            PyObject* n = PyBytes_FromStringAndSize(static_cast<const char*>(buffer.getData()), buffer.getLength());
            PyObject* result = PySequence_InPlaceConcat(bytes_o, n);
            // We need to release the object we temporarily created here
            // to prevent memory leak
            Py_DECREF(n);
            return (result);
        } else if (PyArg_ParseTuple(args, "O!", &messagerenderer_type, &mr)) {
            self->cppobj->toWire(PyMessageRenderer_ToMessageRenderer(mr));
            // If we return NULL it is seen as an error, so use this for
            // None returns
            Py_RETURN_NONE;
        }
    } catch (const EmptyRRset& ers) {
        PyErr_Clear();
        PyErr_SetString(po_EmptyRRset, ers.what());
        return (NULL);
    }
    PyErr_Clear();
    PyErr_SetString(PyExc_TypeError,
                    "toWire argument must be a sequence object or a MessageRenderer");
    return (NULL);
}

PyObject*
RRset_addRdata(s_RRset* self, PyObject* args) {
    PyObject* rdata;
    if (!PyArg_ParseTuple(args, "O!", &rdata_type, &rdata)) {
        return (NULL);
    }
    try {
        self->cppobj->addRdata(PyRdata_ToRdata(rdata));
        Py_RETURN_NONE;
    } catch (const std::bad_cast&) {
        PyErr_Clear();
        PyErr_SetString(PyExc_TypeError,
                        "Rdata type to add must match type of RRset");
        return (NULL);
    }
}

PyObject*
RRset_getRdata(s_RRset* self) {
    PyObject* list = PyList_New(0);

    RdataIteratorPtr it = self->cppobj->getRdataIterator();

    for (; !it->isLast(); it->next()) {
        const rdata::Rdata *rd = &it->getCurrent();
        PyList_Append(list,
                      createRdataObject(createRdata(self->cppobj->getType(),
                                        self->cppobj->getClass(), *rd)));
    }

    return (list);
}

PyObject*
RRset_removeRRsig(s_RRset* self) {
    self->cppobj->removeRRsig();
    Py_RETURN_NONE;
}

} // end of unnamed namespace

namespace isc {
namespace dns {
namespace python {

//
// Declaration of the custom exceptions
// Initialization and addition of these go in the module init at the
// end
//
PyObject* po_EmptyRRset;

PyTypeObject rrset_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pydnspp.RRset",
    sizeof(s_RRset),                    // tp_basicsize
    0,                                  // tp_itemsize
    (destructor)RRset_destroy,          // tp_dealloc
    NULL,                               // tp_print
    NULL,                               // tp_getattr
    NULL,                               // tp_setattr
    NULL,                               // tp_reserved
    NULL,                               // tp_repr
    NULL,                               // tp_as_number
    NULL,                               // tp_as_sequence
    NULL,                               // tp_as_mapping
    NULL,                               // tp_hash
    NULL,                               // tp_call
    RRset_str,                          // tp_str
    NULL,                               // tp_getattro
    NULL,                               // tp_setattro
    NULL,                               // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    "The AbstractRRset class is an abstract base class that "
    "models a DNS RRset.\n\n"
    "An object of (a specific derived class of) AbstractRRset "
    "models an RRset as described in the DNS standard:\n"
    "A set of DNS resource records (RRs) of the same type and class. "
    "The standard requires the TTL of all RRs in an RRset be the same; "
    "this class follows that requirement.\n\n"
    "Note about duplicate RDATA: RFC2181 states that it's meaningless that an "
    "RRset contains two identical RRs and that name servers should suppress "
    "such duplicates.\n"
    "This class is not responsible for ensuring this requirement: For example, "
    "addRdata() method doesn't check if there's already RDATA identical "
    "to the one being added.\n"
    "This is because such checks can be expensive, and it's often easy to "
    "ensure the uniqueness requirement at the %data preparation phase "
    "(e.g. when loading a zone).",
    NULL,                               // tp_traverse
    NULL,                               // tp_clear
    NULL,                               // tp_richcompare
    0,                                  // tp_weaklistoffset
    NULL,                               // tp_iter
    NULL,                               // tp_iternext
    RRset_methods,                      // tp_methods
    NULL,                               // tp_members
    NULL,                               // tp_getset
    NULL,                               // tp_base
    NULL,                               // tp_dict
    NULL,                               // tp_descr_get
    NULL,                               // tp_descr_set
    0,                                  // tp_dictoffset
    (initproc)RRset_init,               // tp_init
    NULL,                               // tp_alloc
    PyType_GenericNew,                  // tp_new
    NULL,                               // tp_free
    NULL,                               // tp_is_gc
    NULL,                               // tp_bases
    NULL,                               // tp_mro
    NULL,                               // tp_cache
    NULL,                               // tp_subclasses
    NULL,                               // tp_weaklist
    NULL,                               // tp_del
    0                                   // tp_version_tag
};



// Module Initialization, all statics are initialized here
namespace internal {
bool
initModulePart_RRset(PyObject* mod) {
    // Add the exceptions to the module
    po_EmptyRRset = PyErr_NewException("pydnspp.EmptyRRset", NULL, NULL);
    PyModule_AddObject(mod, "EmptyRRset", po_EmptyRRset);

    // Add the enums to the module

    // Add the constants to the module

    // Add the classes to the module
    // We initialize the static description object with PyType_Ready(),
    // then add it to the module

    // NameComparisonResult
    if (PyType_Ready(&rrset_type) < 0) {
        return (false);
    }
    Py_INCREF(&rrset_type);
    PyModule_AddObject(mod, "RRset",
                       reinterpret_cast<PyObject*>(&rrset_type));

    return (true);
}
} // end namespace internal

PyObject*
createRRsetObject(const RRset& source) {
    s_RRset* py_rrset =
        static_cast<s_RRset*>(rrset_type.tp_alloc(&rrset_type, 0));
    if (py_rrset == NULL) {
        isc_throw(PyCPPWrapperException, "Unexpected NULL C++ object, "
                  "probably due to short memory");
    }

    // RRsets are noncopyable, so as a workaround we recreate a new one
    // and copy over all content
    try {
        py_rrset->cppobj = isc::dns::RRsetPtr(
            new isc::dns::RRset(source.getName(), source.getClass(),
                                source.getType(), source.getTTL()));

        isc::dns::RdataIteratorPtr rdata_it(source.getRdataIterator());
        for (rdata_it->first(); !rdata_it->isLast(); rdata_it->next()) {
            py_rrset->cppobj->addRdata(rdata_it->getCurrent());
        }

        isc::dns::RRsetPtr sigs = source.getRRsig();
        if (sigs) {
            py_rrset->cppobj->addRRsig(sigs);
        }
        return (py_rrset);
    } catch (const std::bad_alloc&) {
        isc_throw(PyCPPWrapperException, "Unexpected NULL C++ object, "
                  "probably due to short memory");
    }
}

bool
PyRRset_Check(PyObject* obj) {
    return (PyObject_TypeCheck(obj, &rrset_type));
}

RRset&
PyRRset_ToRRset(PyObject* rrset_obj) {
    s_RRset* rrset = static_cast<s_RRset*>(rrset_obj);
    return (*rrset->cppobj);
}

RRsetPtr
PyRRset_ToRRsetPtr(PyObject* rrset_obj) {
    s_RRset* rrset = static_cast<s_RRset*>(rrset_obj);
    return (rrset->cppobj);
}


} // end python namespace
} // end dns namespace
} // end isc namespace
