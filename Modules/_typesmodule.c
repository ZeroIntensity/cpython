// C accelerator for the types modules

#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"
#include "pycore_descrobject.h" // _PyMethodWrapper_Type
#include "pycore_genobject.h"   // _PyGen_GetCode
#include "pycore_object.h"      // _PyNone_Type, _PyNotImplemented_Type
#include "pycore_unionobject.h" // _PyUnion_Type

/*[clinic input]
module _types
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=530308b1011b659d]*/

#include "clinic/_typesmodule.c.h"

PyDoc_STRVAR(types__doc__,
"A helper for the types module.");

static PyObject *
generator_as_coroutine(PyObject *func, PyCodeObject *code)
{
    /* Equivalent to calling code.replace(co_flags=code.co_flags | CO_ITERABLE_COROUTINE) */
    PyObject *method = PyObject_GetAttrString((PyObject *)code, "replace");
    if (method == NULL)
    {
        return NULL;
    }

    // We could optimize this by manually constructing a code object
    // with all the fields instead of calling replace().
    // However, this is much simpler if we don't.
    int new_flags = code->co_flags;
    new_flags |= CO_ITERABLE_COROUTINE;
    PyObject *kwargs = Py_BuildValue("{s:i}", "co_flags", new_flags);
    if (kwargs == NULL)
    {
        Py_DECREF(method);
        return NULL;
    }

    PyObject *new_code = PyObject_VectorcallDict(method, NULL, 0, kwargs);
    Py_DECREF(method);
    Py_DECREF(kwargs);

    if (new_code == NULL)
    {
        return NULL;
    }

    PyObject *func_copy = PyFunction_New(new_code, PyFunction_GET_GLOBALS(func));
    Py_DECREF(new_code);
    if (func_copy == NULL)
    {
        return NULL;
    }

    return func_copy;
}

static PyObject *
wrapped_decorator(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *coro = PyObject_Call(self, args, kwargs);
    if (coro == NULL)
    {
        return NULL;
    }

    if (PyCoro_CheckExact(coro))
    {
        return coro;
    }

    if (PyGen_CheckExact(coro))
    {
        PyCodeObject *code = _PyGen_GetCode((PyGenObject *)coro);
        if (code->co_flags & CO_ITERABLE_COROUTINE)
        {
            return coro;
        }
    }

    return coro;
}

const PyMethodDef wrapped_decorator_md = {
    .ml_meth = (PyCFunction)wrapped_decorator,
    .ml_flags = METH_VARARGS|METH_KEYWORDS
};

/*[clinic input]
_types.coroutine

    func: object

Convert regular generator function to a coroutine.
[clinic start generated code]*/

static PyObject *
_types_coroutine_impl(PyObject *module, PyObject *func)
/*[clinic end generated code: output=9d829b77b39bfbeb input=b32ba38c90aa4bcf]*/
{
    if (!PyCallable_Check(func))
    {
        PyErr_SetString(PyExc_TypeError, "types.coroutine() expects a callable");
        return NULL;
    }

    if (PyFunction_Check(func))
    {
        PyCodeObject *code = (PyCodeObject *)PyFunction_GET_CODE(func); // Borrowed
        if (code->co_flags & (CO_COROUTINE | CO_ITERABLE_COROUTINE))
        {
            // Already a coroutine function
            return Py_NewRef(func);
        }

        if (code->co_flags & CO_GENERATOR)
        {
            return generator_as_coroutine(func, code);
        }
    }

    return PyCFunction_New((PyMethodDef *)&wrapped_decorator_md, func);
}

/*[clinic input]
_types.resolve_bases

    bases: object

Resolve MRO entries dynamically as specified by PEP 560.
[clinic start generated code]*/

static PyObject *
_types_resolve_bases_impl(PyObject *module, PyObject *bases)
/*[clinic end generated code: output=4f8e2e4bfdaeba2e input=84fbb8401697b712]*/
{
    PyObject *iterator = PyObject_GetIter(bases);
    if (iterator == NULL)
    {
        return NULL;
    }
    Py_ssize_t shift = 0;
    Py_ssize_t index = -1;
    /* Optimization: lazily initialize the list */
    PyObject *new_bases = NULL;

    PyObject *item;
    while (PyIter_NextItem(iterator, &item))
    {
        ++index;
        if (item == NULL)
        {
            Py_DECREF(iterator);
            Py_XDECREF(new_bases);
            return NULL;
        }

        if (PyType_Check(item))
        {
            continue;
        }

        PyObject *mro_entries;
        if (PyObject_GetOptionalAttr(item, &_Py_ID(__mro_entries__), &mro_entries) < 0)
        {
            Py_DECREF(iterator);
            Py_XDECREF(new_bases);
            return NULL;
        }

        if (mro_entries == NULL)
        {
            continue;
        }

        PyObject *new_base = PyObject_CallOneArg(mro_entries, bases);
        Py_DECREF(mro_entries);
        if (new_base == NULL)
        {
            Py_DECREF(iterator);
            Py_XDECREF(new_bases);
            return NULL;
        }

        if (!PyTuple_Check(new_base))
        {
            Py_DECREF(iterator);
            Py_XDECREF(new_bases);
            Py_DECREF(new_base);
            PyErr_SetString(PyExc_TypeError,
                            "__mro_entries__ must return a tuple");
            return NULL;
        }

        if (new_bases == NULL)
        {
            new_bases = PyObject_CallOneArg((PyObject *)&PyList_Type, bases);
            if (new_bases == NULL)
            {
                Py_DECREF(iterator);
                Py_DECREF(new_base);
                return NULL;
            }
        }

        assert(new_bases != NULL);
        if (PyList_SetSlice(new_bases, index + shift, index + shift + 1, new_base) < 0)
        {
            Py_DECREF(iterator);
            Py_DECREF(new_bases);
            Py_DECREF(new_base);
            return NULL;
        }
        Py_DECREF(new_base);
    }
    Py_DECREF(iterator);
    assert(!PyErr_Occurred());

    if (new_bases == NULL)
    {
        return Py_NewRef(bases);
    }

    return new_bases;
}

static int
types_exec(PyObject *mod)
{
    if (PyModule_AddObjectRef(mod, "FunctionType", (PyObject *)&PyFunction_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "CodeType", (PyObject *)&PyCode_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "CodeType", (PyObject *)&PyCode_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "MappingProxyType", (PyObject *)&PyDictProxy_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "CellType", (PyObject *)&PyCell_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "GeneratorType", (PyObject *)&PyGen_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "CoroutineType", (PyObject *)&PyCoro_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "AsyncGeneratorType", (PyObject *)&PyAsyncGen_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "MethodType", (PyObject *)&PyMethod_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "BuiltinFunctionType", (PyObject *)&PyCMethod_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "BuiltinMethodType", (PyObject *)&PyCMethod_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "WrapperDescriptorType", (PyObject *)&PyWrapperDescr_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "MethodWrapperType", (PyObject *)&_PyMethodWrapper_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "MethodDescriptorType", (PyObject *)&PyMethodDescr_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "ClassMethodDescriptorType", (PyObject *)&PyClassMethodDescr_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "TracebackType", (PyObject *)&PyTraceBack_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "FrameType", (PyObject *)&PyFrame_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "GetSetDescriptorType", (PyObject *)&PyGetSetDescr_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "MemberDescriptorType", (PyObject *)&PyMemberDescr_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "ModuleType", (PyObject *)&PyModule_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "GenericAlias", (PyObject *)&Py_GenericAliasType) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "UnionType", (PyObject *)&_PyUnion_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "EllipsisType", (PyObject *)&PyEllipsis_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "NoneType", (PyObject *)&_PyNone_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "NotImplementedType", (PyObject *)&_PyNotImplemented_Type) < 0) {
        return -1;
    }
    if (PyModule_AddObjectRef(mod, "CapsuleType", (PyObject *)&PyCapsule_Type) < 0) {
        return -1;
    }
    return 0;
}

static struct PyMethodDef types_methods[] = {
    _TYPES_COROUTINE_METHODDEF
    _TYPES_RESOLVE_BASES_METHODDEF
    {NULL, NULL}
};

static PyModuleDef_Slot types_slots[] = {
    {Py_mod_exec, types_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static PyModuleDef types_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_types",
    .m_doc = types__doc__,
    .m_methods = types_methods,
    .m_slots = types_slots,
};

PyMODINIT_FUNC
PyInit__types(void)
{
    return PyModuleDef_Init(&types_module);
}
