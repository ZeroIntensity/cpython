/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
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
/*[clinic end generated code: output=7a0cf9b143f09f86 input=a9049054013a1b77]*/
