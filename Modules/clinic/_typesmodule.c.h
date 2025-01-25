/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_critical_section.h"// Py_BEGIN_CRITICAL_SECTION()
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(_types_coroutine__doc__,
"coroutine($module, /, func)\n"
"--\n"
"\n"
"Convert regular generator function to a coroutine.");

#define _TYPES_COROUTINE_METHODDEF    \
    {"coroutine", _PyCFunction_CAST(_types_coroutine), METH_FASTCALL|METH_KEYWORDS, _types_coroutine__doc__},

static PyObject *
_types_coroutine_impl(PyObject *module, PyObject *func);

static PyObject *
_types_coroutine(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(func), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"func", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "coroutine",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *func;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    func = args[0];
    return_value = _types_coroutine_impl(module, func);

exit:
    return return_value;
}

PyDoc_STRVAR(_types_resolve_bases__doc__,
"resolve_bases($module, /, bases)\n"
"--\n"
"\n"
"Resolve MRO entries dynamically as specified by PEP 560.");

#define _TYPES_RESOLVE_BASES_METHODDEF    \
    {"resolve_bases", _PyCFunction_CAST(_types_resolve_bases), METH_FASTCALL|METH_KEYWORDS, _types_resolve_bases__doc__},

static PyObject *
_types_resolve_bases_impl(PyObject *module, PyObject *bases);

static PyObject *
_types_resolve_bases(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(bases), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"bases", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "resolve_bases",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *bases;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    bases = args[0];
    return_value = _types_resolve_bases_impl(module, bases);

exit:
    return return_value;
}

PyDoc_STRVAR(_types_get_original_bases__doc__,
"get_original_bases($module, /, cls)\n"
"--\n"
"\n"
"Return the class\'s \"original\" bases prior to modification by `__mro_entries__`.\n"
"\n"
"Examples::\n"
"\n"
"    from typing import TypeVar, Generic, NamedTuple, TypedDict\n"
"\n"
"    T = TypeVar(\"T\")\n"
"    class Foo(Generic[T]): ...\n"
"    class Bar(Foo[int], float): ...\n"
"    class Baz(list[str]): ...\n"
"    Eggs = NamedTuple(\"Eggs\", [(\"a\", int), (\"b\", str)])\n"
"    Spam = TypedDict(\"Spam\", {\"a\": int, \"b\": str})\n"
"\n"
"    assert get_original_bases(Bar) == (Foo[int], float)\n"
"    assert get_original_bases(Baz) == (list[str],)\n"
"    assert get_original_bases(Eggs) == (NamedTuple,)\n"
"    assert get_original_bases(Spam) == (TypedDict,)\n"
"    assert get_original_bases(int) == (object,)");

#define _TYPES_GET_ORIGINAL_BASES_METHODDEF    \
    {"get_original_bases", _PyCFunction_CAST(_types_get_original_bases), METH_FASTCALL|METH_KEYWORDS, _types_get_original_bases__doc__},

static PyObject *
_types_get_original_bases_impl(PyObject *module, PyObject *cls);

static PyObject *
_types_get_original_bases(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(cls), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"cls", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "get_original_bases",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *cls;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!PyObject_TypeCheck(args[0], &PyType_Type)) {
        _PyArg_BadArgument("get_original_bases", "argument 'cls'", (&PyType_Type)->tp_name, args[0]);
        goto exit;
    }
    cls = args[0];
    return_value = _types_get_original_bases_impl(module, cls);

exit:
    return return_value;
}

PyDoc_STRVAR(_types_DynamicClassAttribute_getter__doc__,
"getter($self, /, fget)\n"
"--\n"
"\n");

#define _TYPES_DYNAMICCLASSATTRIBUTE_GETTER_METHODDEF    \
    {"getter", _PyCFunction_CAST(_types_DynamicClassAttribute_getter), METH_FASTCALL|METH_KEYWORDS, _types_DynamicClassAttribute_getter__doc__},

static PyObject *
_types_DynamicClassAttribute_getter_impl(DynamicClassAttribute *self,
                                         PyObject *fget);

static PyObject *
_types_DynamicClassAttribute_getter(DynamicClassAttribute *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(fget), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"fget", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "getter",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *fget;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fget = args[0];
    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _types_DynamicClassAttribute_getter_impl(self, fget);
    Py_END_CRITICAL_SECTION();

exit:
    return return_value;
}

PyDoc_STRVAR(_types_DynamicClassAttribute_setter__doc__,
"setter($self, /, fset)\n"
"--\n"
"\n");

#define _TYPES_DYNAMICCLASSATTRIBUTE_SETTER_METHODDEF    \
    {"setter", _PyCFunction_CAST(_types_DynamicClassAttribute_setter), METH_FASTCALL|METH_KEYWORDS, _types_DynamicClassAttribute_setter__doc__},

static PyObject *
_types_DynamicClassAttribute_setter_impl(DynamicClassAttribute *self,
                                         PyObject *fset);

static PyObject *
_types_DynamicClassAttribute_setter(DynamicClassAttribute *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(fset), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"fset", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "setter",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *fset;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fset = args[0];
    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _types_DynamicClassAttribute_setter_impl(self, fset);
    Py_END_CRITICAL_SECTION();

exit:
    return return_value;
}

PyDoc_STRVAR(_types_DynamicClassAttribute_deleter__doc__,
"deleter($self, /, fdel)\n"
"--\n"
"\n");

#define _TYPES_DYNAMICCLASSATTRIBUTE_DELETER_METHODDEF    \
    {"deleter", _PyCFunction_CAST(_types_DynamicClassAttribute_deleter), METH_FASTCALL|METH_KEYWORDS, _types_DynamicClassAttribute_deleter__doc__},

static PyObject *
_types_DynamicClassAttribute_deleter_impl(DynamicClassAttribute *self,
                                          PyObject *fdel);

static PyObject *
_types_DynamicClassAttribute_deleter(DynamicClassAttribute *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(fdel), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"fdel", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "deleter",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *fdel;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    fdel = args[0];
    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _types_DynamicClassAttribute_deleter_impl(self, fdel);
    Py_END_CRITICAL_SECTION();

exit:
    return return_value;
}

#if !defined(_types_DynamicClassAttribute_overwrite_doc_DOCSTR)
#  define _types_DynamicClassAttribute_overwrite_doc_DOCSTR NULL
#endif
#if defined(_TYPES_DYNAMICCLASSATTRIBUTE_OVERWRITE_DOC_GETSETDEF)
#  undef _TYPES_DYNAMICCLASSATTRIBUTE_OVERWRITE_DOC_GETSETDEF
#  define _TYPES_DYNAMICCLASSATTRIBUTE_OVERWRITE_DOC_GETSETDEF {"overwrite_doc", (getter)_types_DynamicClassAttribute_overwrite_doc_get, (setter)_types_DynamicClassAttribute_overwrite_doc_set, _types_DynamicClassAttribute_overwrite_doc_DOCSTR},
#else
#  define _TYPES_DYNAMICCLASSATTRIBUTE_OVERWRITE_DOC_GETSETDEF {"overwrite_doc", (getter)_types_DynamicClassAttribute_overwrite_doc_get, NULL, _types_DynamicClassAttribute_overwrite_doc_DOCSTR},
#endif

static PyObject *
_types_DynamicClassAttribute_overwrite_doc_get_impl(DynamicClassAttribute *self);

static PyObject *
_types_DynamicClassAttribute_overwrite_doc_get(DynamicClassAttribute *self, void *Py_UNUSED(context))
{
    PyObject *return_value = NULL;

    Py_BEGIN_CRITICAL_SECTION(self);
    return_value = _types_DynamicClassAttribute_overwrite_doc_get_impl(self);
    Py_END_CRITICAL_SECTION();

    return return_value;
}
/*[clinic end generated code: output=760624a11a9e164e input=a9049054013a1b77]*/
