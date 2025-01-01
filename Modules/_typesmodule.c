// C accelerator for the types modules

#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"
#include "pycore_genobject.h"

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

static struct PyMethodDef types_methods[] = {
    _TYPES_COROUTINE_METHODDEF
    {NULL, NULL}
};

static PyModuleDef_Slot types_slots[] = {
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
