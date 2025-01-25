// C accelerator for the types modules

#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"
#include "pycore_critical_section.h"    // _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED
#include "pycore_descrobject.h"         // _PyMethodWrapper_Type
#include "pycore_genobject.h"           // _PyGen_GetCode
#include "pycore_object.h"              // _PyNone_Type, _PyNotImplemented_Type
#include "pycore_unionobject.h"         // _PyUnion_Type

/*[clinic input]
module _types
class _types.DynamicClassAttribute "DynamicClassAttribute *" ""
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=4dc7a5af8cc69b62]*/

typedef struct {
    PyObject *DynamicClassAttribute;
} types_state;

static types_state *
get_module_state(PyObject *module)
{
    assert(PyModule_Check(module));
    types_state *state = PyModule_GetState(module);
    assert(state != NULL);
    return state;
}

typedef struct _DynamicClassAttribute DynamicClassAttribute;

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

/*[clinic input]
_types.get_original_bases

    cls: object(subclass_of="&PyType_Type")

Return the class's "original" bases prior to modification by `__mro_entries__`.

Examples::

    from typing import TypeVar, Generic, NamedTuple, TypedDict

    T = TypeVar("T")
    class Foo(Generic[T]): ...
    class Bar(Foo[int], float): ...
    class Baz(list[str]): ...
    Eggs = NamedTuple("Eggs", [("a", int), ("b", str)])
    Spam = TypedDict("Spam", {"a": int, "b": str})

    assert get_original_bases(Bar) == (Foo[int], float)
    assert get_original_bases(Baz) == (list[str],)
    assert get_original_bases(Eggs) == (NamedTuple,)
    assert get_original_bases(Spam) == (TypedDict,)
    assert get_original_bases(int) == (object,)
[clinic start generated code]*/

static PyObject *
_types_get_original_bases_impl(PyObject *module, PyObject *cls)
/*[clinic end generated code: output=f42e23aec2fb73a9 input=30e786bccf62cac8]*/
{
    PyObject *orig_bases;
    if (PyObject_GetOptionalAttrString(cls, "__orig_bases__", &orig_bases) < 0)
    {
        return NULL;
    }

    if (orig_bases != NULL)
    {
        return orig_bases;
    }

    PyObject *bases;
    if (PyObject_GetOptionalAttr(cls, &_Py_ID(__bases__), &bases) < 0)
    {
        return NULL;
    }

    return bases;
}

struct _DynamicClassAttribute {
    PyObject_HEAD;
    PyObject *fget;
    PyObject *fset;
    PyObject *fdel;
    PyObject *doc;
    int overwrite_doc;
    int is_abstract_method;
};

static int
dynamicclassattribute_build(DynamicClassAttribute *dca, PyObject *fget,
                            PyObject *fset, PyObject *fdel, PyObject *doc)
{
    dca->fget = Py_XNewRef(fget);
    dca->fset = Py_XNewRef(fset);
    dca->fdel = Py_XNewRef(fdel);

    PyObject *the_doc = doc == NULL
                        ? PyObject_GetAttr(Py_None, &_Py_ID(__doc__))
                        : Py_NewRef(doc);
    if (the_doc == NULL)
    {
        return -1;
    }
    dca->doc = the_doc;
    dca->overwrite_doc = doc == NULL;
    int is_abstract_method = PyObject_HasAttrStringWithError(fget, "__abstractmethod__");
    if (is_abstract_method < 0)
    {
        return -1;
    }
    dca->is_abstract_method = is_abstract_method;
    return 0;
}

static int
dynamicclassattribute_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"fget", "fset", "fdel", "doc", NULL};
    PyObject *fget = NULL, *fset = NULL, *fdel = NULL, *doc = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOOO", kwlist, &fget,
                                     &fset, &fdel, &doc)) {
        return -1;
    }
    DynamicClassAttribute *dca = (DynamicClassAttribute *)self;
    return dynamicclassattribute_build(dca, fget, fset, fdel, doc);
}

static int
dynamicclassattribute_clear(PyObject *self)
{
    DynamicClassAttribute *dca = (DynamicClassAttribute *)self;
    Py_CLEAR(dca->fget);
    Py_CLEAR(dca->fdel);
    Py_CLEAR(dca->fset);
    Py_CLEAR(dca->doc);
    return 0;
}

static int
dynamicclassattribute_traverse(PyObject *self, visitproc visit, void *arg)
{
    DynamicClassAttribute *dca = (DynamicClassAttribute *)self;
    Py_VISIT(dca->fget);
    Py_VISIT(dca->fdel);
    Py_VISIT(dca->fset);
    Py_VISIT(dca->doc);
    return 0;
}

static void
dynamicclassattribute_dealloc(PyObject *self)
{
    if (PyObject_CallFinalizerFromDealloc(self) < 0) {
        return;
    }
    PyTypeObject *type = Py_TYPE(self);
    PyObject_GC_UnTrack(self);
    (void)dynamicclassattribute_clear(self);
    type->tp_free(self);
    Py_DECREF(type);
}

static PyObject *
dynamicclassattribute_descr_get_lock_held(PyObject *self, PyObject *obj, PyObject *type)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(self);
    DynamicClassAttribute *dca = (DynamicClassAttribute *)self;
    if (Py_IsNone(obj)) {
        if (dca->is_abstract_method) {
            return Py_NewRef(self);
        }

        PyErr_SetObject(PyExc_AttributeError, Py_None);
        return NULL;
    }

    if (dca->fget == NULL) {
        PyErr_SetString(PyExc_AttributeError, "unreadable attribute");
        return NULL;
    }

    return PyObject_CallOneArg(dca->fget, obj);
}

static int
dynamicclassattribute_descr_del_lock_held(DynamicClassAttribute *dca, PyObject *obj)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(dca);
    if (dca->fdel == NULL) {
        PyErr_SetString(PyExc_AttributeError, "can't delete attribute");
        return -1;
    }
    PyObject *res = PyObject_CallOneArg(dca->fdel, obj);
    if (res == NULL) {
        return -1;
    }
    Py_DECREF(res);
    return 0;
}

static int
dynamicclassattribute_descr_set_lock_held(PyObject *self, PyObject *obj, PyObject *value)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(self);
    DynamicClassAttribute *dca = (DynamicClassAttribute *)self;
    if (value == NULL) {
        return dynamicclassattribute_descr_del_lock_held(dca, obj);
    }

    if (dca->fset == NULL) {
        PyErr_SetString(PyExc_AttributeError, "can't set attribute");
        return -1;
    }

    PyObject *res = PyObject_Vectorcall(dca->fset, (PyObject *[]) { obj, value }, 2, NULL);
    if (res == NULL) {
        return -1;
    }
    Py_DECREF(res);
    return 0;
}

static PyObject *
dynamicclassattribute_descr_get(PyObject *self, PyObject *obj, PyObject *type)
{
    PyObject *res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = dynamicclassattribute_descr_get_lock_held(self, obj, type);
    Py_END_CRITICAL_SECTION();
    return res;
}

static int
dynamicclassattribute_descr_set(PyObject *self, PyObject *obj, PyObject *value)
{
    int res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = dynamicclassattribute_descr_set_lock_held(self, obj, value);
    Py_END_CRITICAL_SECTION();
    return res;
}

static PyObject *
dynamicclassattribute_fast_init(DynamicClassAttribute *instance, PyObject *fget,
                                PyObject *fset, PyObject *fdel, PyObject *doc)
{
    assert(instance != NULL);
    PyTypeObject *type = Py_TYPE(instance);
    DynamicClassAttribute *dca = (DynamicClassAttribute *)type->tp_new(type, NULL, NULL);
    if (dynamicclassattribute_build(dca, fget, fset, fdel, doc) < 0) {
        Py_DECREF(dca);
        return NULL;
    }
    dca->overwrite_doc = instance->overwrite_doc;

    return (PyObject *)dca;
}

/*[clinic input]
@critical_section
_types.DynamicClassAttribute.getter

    fget: object
[clinic start generated code]*/

static PyObject *
_types_DynamicClassAttribute_getter_impl(DynamicClassAttribute *self,
                                         PyObject *fget)
/*[clinic end generated code: output=dbb3b3e914430af4 input=2e59779855a1e02f]*/
{
    PyObject *doc = NULL;
    if (self->overwrite_doc)
    {
        if (PyObject_GetOptionalAttr(fget, &_Py_ID(__doc__), &doc) < 0) {
            return NULL;
        }
    }

    PyObject *obj = dynamicclassattribute_fast_init(self, fget, self->fset, self->fdel,
                                      doc == NULL ? self->doc : doc);
    Py_XDECREF(doc);
    return obj;
}

/*[clinic input]
@critical_section
_types.DynamicClassAttribute.setter

    fset: object
[clinic start generated code]*/

static PyObject *
_types_DynamicClassAttribute_setter_impl(DynamicClassAttribute *self,
                                         PyObject *fset)
/*[clinic end generated code: output=25e077e16413129a input=c16acff4ed6f8aa5]*/
{
    return dynamicclassattribute_fast_init(self, self->fget,
                                           fset, self->fdel, self->doc);
}

/*[clinic input]
@critical_section
_types.DynamicClassAttribute.deleter

    fdel: object
[clinic start generated code]*/

static PyObject *
_types_DynamicClassAttribute_deleter_impl(DynamicClassAttribute *self,
                                          PyObject *fdel)
/*[clinic end generated code: output=34cd8b0e47e00899 input=046702a7439d2c7a]*/
{
    return dynamicclassattribute_fast_init(self, self->fget,
                                           self->fset, fdel, self->doc);
}

/*[clinic input]
@critical_section
@getter
_types.DynamicClassAttribute.overwrite_doc
[clinic start generated code]*/

static PyObject *
_types_DynamicClassAttribute_overwrite_doc_get_impl(DynamicClassAttribute *self)
/*[clinic end generated code: output=cd21b78a0015e444 input=134aa8abbb0d936b]*/
{
    return PyBool_FromLong(self->doc == NULL);
}

static PyMethodDef dynamicclassattribute_methods[] = {
    _TYPES_DYNAMICCLASSATTRIBUTE_GETTER_METHODDEF
    _TYPES_DYNAMICCLASSATTRIBUTE_SETTER_METHODDEF
    _TYPES_DYNAMICCLASSATTRIBUTE_DELETER_METHODDEF
    {NULL, NULL, 0, NULL}
};

static PyGetSetDef dynamicclassattribute_getset[] = {
    _TYPES_DYNAMICCLASSATTRIBUTE_OVERWRITE_DOC_GETSETDEF
    {NULL}
};

static PyMemberDef dynamicclassattribute_members[] = {
    {"fget", _Py_T_OBJECT, offsetof(DynamicClassAttribute, fget), 0, NULL},
    {"fset", _Py_T_OBJECT, offsetof(DynamicClassAttribute, fset), 0, NULL},
    {"fdel", _Py_T_OBJECT, offsetof(DynamicClassAttribute, fdel), 0, NULL},
    {"__doc__", _Py_T_OBJECT, offsetof(DynamicClassAttribute, doc), 0, NULL},
    {"__isabstractmethod__", Py_T_BOOL | Py_READONLY,
        offsetof(DynamicClassAttribute, is_abstract_method), 0, NULL},
    {NULL}
};

static PyType_Slot DynamicClassAttribute_Slots[] = {
    {Py_tp_init, dynamicclassattribute_init},
    {Py_tp_clear, dynamicclassattribute_clear},
    {Py_tp_traverse, dynamicclassattribute_traverse},
    {Py_tp_dealloc, dynamicclassattribute_dealloc},
    {Py_tp_descr_get, dynamicclassattribute_descr_get},
    {Py_tp_descr_set, dynamicclassattribute_descr_set},
    {Py_tp_methods, dynamicclassattribute_methods},
    {Py_tp_members, dynamicclassattribute_members},
    {Py_tp_getset, dynamicclassattribute_getset},
    {0, NULL}
};

PyType_Spec DynamicClassAttribute_Spec = {
    .name = "types.DynamicClassAttribute",
    .basicsize = sizeof(DynamicClassAttribute),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = DynamicClassAttribute_Slots,
};

static int
types_exec(PyObject *mod)
{
    types_state *state = get_module_state(mod);
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
#define ADD_TYPE(name)                                                     \
    PyObject *name## Object = PyType_FromModuleAndSpec(mod, &name ##_Spec, \
                                                       NULL);              \
    if (name## Object == NULL) {                                           \
        return -1;                                                         \
    }                                                                      \
    if (PyModule_AddType(mod, (PyTypeObject *)name## Object) < 0) {        \
        Py_DECREF(name## Object);                                          \
        return -1;                                                         \
    }                                                                      \
    state->DynamicClassAttribute = name## Object                           \

    ADD_TYPE(DynamicClassAttribute);
#undef ADD_TYPE

    return 0;
}

static struct PyMethodDef types_methods[] = {
    _TYPES_COROUTINE_METHODDEF
    _TYPES_RESOLVE_BASES_METHODDEF
    _TYPES_GET_ORIGINAL_BASES_METHODDEF
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
    .m_size = sizeof(types_state)
};

PyMODINIT_FUNC
PyInit__types(void)
{
    return PyModuleDef_Init(&types_module);
}
