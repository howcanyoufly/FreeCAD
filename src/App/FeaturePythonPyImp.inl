/***************************************************************************
 *   Copyright (c) 2013 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


namespace App
{

/// Type structure of FeaturePythonPyT
template<class FeaturePyT>
PyTypeObject FeaturePythonPyT<FeaturePyT>::Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                                                /*ob_size*/
    "FeaturePython",                                  /*tp_name*/
    sizeof(FeaturePythonPyT),                         /*tp_basicsize*/
    0,                                                /*tp_itemsize*/
    /* methods */
    FeaturePyT::PyDestructor,                         /*tp_dealloc*/
    0,                                                /*tp_print*/
    FeaturePyT::__getattr,                            /*tp_getattr*/
    __setattr,                                        /*tp_setattr*/
    0,                                                /*tp_compare*/
    0,                                                /*tp_repr*/
    0,                                                /*tp_as_number*/
    0,                                                /*tp_as_sequence*/
    0,                                                /*tp_as_mapping*/
    0,                                                /*tp_hash*/
    0,                                                /*tp_call */
    0,                                                /*tp_str  */
    0,                                                /*tp_getattro*/
    0,                                                /*tp_setattro*/
    /* --- Functions to access object as input/output buffer ---------*/
    0,                                                /* tp_as_buffer */
    /* --- Flags to define presence of optional/expanded features */
    Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_CLASS,        /*tp_flags */
    "This is the father of all Feature classes",      /*tp_doc */
    0,                                                /*tp_traverse */
    0,                                                /*tp_clear */
    0,                                                /*tp_richcompare */
    0,                                                /*tp_weaklistoffset */
    0,                                                /*tp_iter */
    0,                                                /*tp_iternext */
    0,                                                /*tp_methods */
    0,                                                /*tp_members */
    0,                                                /*tp_getset */
    &FeaturePyT::Type,                                /*tp_base */
    0,                                                /*tp_dict */
    0,                                                /*tp_descr_get */
    0,                                                /*tp_descr_set */
    0,                                                /*tp_dictoffset */
    FeaturePyT::__PyInit,                             /*tp_init */
    0,                                                /*tp_alloc */
    0,                                                /*tp_new */
    0,                                                /*tp_free   Low-level free-memory routine */
    0,                                                /*tp_is_gc  For PyObject_IS_GC */
    0,                                                /*tp_bases */
    0,                                                /*tp_mro    method resolution order */
    0,                                                /*tp_cache */
    0,                                                /*tp_subclasses */
    0,                                                /*tp_weaklist */
    0,                                                /*tp_del */
    0                                                 /*tp_version_tag */
};

template<class FeaturePyT>
FeaturePythonPyT<FeaturePyT>::FeaturePythonPyT(Base::BaseClass *pcObject, PyTypeObject *T)
    : FeaturePyT(reinterpret_cast<typename FeaturePyT::PointerType>(pcObject), T)
{
}

template<class FeaturePyT>
FeaturePythonPyT<FeaturePyT>::~FeaturePythonPyT()
{
}

template<class FeaturePyT>
int FeaturePythonPyT<FeaturePyT>::__setattr(PyObject *obj, char *attr, PyObject *value)
{
    // This overwrites PyObjectBase::__setattr because this actively disallows to delete an attribute
    //
    if (!static_cast<Base::PyObjectBase*>(obj)->isValid()){
        PyErr_Format(PyExc_ReferenceError, "Cannot access attribute '%s' of deleted object", attr);
        return -1;
    }

    int ret = static_cast<Base::PyObjectBase*>(obj)->_setattr(attr, value);
    if (ret == 0) {
        static_cast<Base::PyObjectBase*>(obj)->startNotify();
    }
    return ret;
}

template<class FeaturePyT>
int FeaturePythonPyT<FeaturePyT>::_setattr(char *attr, PyObject *value)
{
    int returnValue = FeaturePyT::_setattr(attr, value);
    if (returnValue == -1) {
        if (value) {
            if (PyFunction_Check(value)) {
                std::map<std::string, PyObject*>::iterator it = dyn_methods.find(attr);
                if (it != dyn_methods.end()) {
                    Py_XDECREF(it->second);
                }
                dyn_methods[attr] = PyMethod_New(value, this, 0);
                returnValue = 0;
                PyErr_Clear();
            }
        }
        else {
            // delete
            std::map<std::string, PyObject*>::iterator it = dyn_methods.find(attr);
            if (it != dyn_methods.end()) {
                Py_XDECREF(it->second);
                dyn_methods.erase(it);
                returnValue = 0;
                PyErr_Clear();
            }
        }
    }
    return returnValue;
}

template<class FeaturePyT>
PyObject *FeaturePythonPyT<FeaturePyT>::_getattr(char *attr)
{
    // See CallTipsList::extractTips
    if (Base::streq(attr, "__fc_template__")) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (Base::streq(attr, "__dict__")) {
        // Return the default dict
        PyTypeObject *tp = this->ob_type;
        // register type if needed
        if (tp->tp_dict == NULL) {
            if (PyType_Ready(tp) < 0)
                return 0;
        }

        PyObject* dict = FeaturePyT::_getattr(attr);
        if (dict && PyDict_CheckExact(dict)) {
            PyObject* dict_old = dict;
            dict = PyDict_Copy(dict_old);
            Py_DECREF(dict_old); // delete old dict

            for (std::map<std::string, PyObject*>::const_iterator it = dyn_methods.begin(); it != dyn_methods.end(); ++it)
                PyDict_SetItem(dict, PyString_FromString(it->first.c_str()), PyString_FromString(""));
            if (PyErr_Occurred()) {
                Py_DECREF(dict);
                dict = 0;
            }
        }
        return dict;
    }

    PyObject *rvalue = NULL;
    std::map<std::string, PyObject*>::iterator it = dyn_methods.find(attr);
    if (it != dyn_methods.end()) {
        Py_INCREF(it->second);
        rvalue = it->second;
        PyErr_Clear();
        return rvalue;
    }

    PyErr_Clear();
    return FeaturePyT::_getattr(attr);
}

} //namespace App
