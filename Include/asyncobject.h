/* C API for defining asynchronous functions (PEP 852). */

#ifndef Py_ASYNCOBJECT_H
#define Py_ASYNCOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03100000

/* An opaque object implementing the coroutine protocol. */
typedef struct _PyAsync PyAsync;

PyAPI_DATA(PyTypeObject) PyAsync_Type;

PyAPI_FUNC(int) PyAsync_CheckExact(PyObject *op);

typedef int (*PyAsync_Callback)(PyObject *asyncobj, PyObject *result);
typedef int (*PyAsync_Error)(PyObject *asyncobj, PyObject *exception);
typedef int (*PyAsync_Defer)(PyObject *asyncobj);

PyAPI_FUNC(PyObject *) PyAsync_New(void);
PyAPI_FUNC(int) PyAsync_SetResult(PyObject *asyncobj, PyObject *result);
PyAPI_FUNC(int) PyAsync_AddAwait(
    PyObject *asyncobj, PyObject *coroutine,
    PyAsync_Callback result_callback, PyAsync_Error error_callback);
PyAPI_FUNC(int) PyAsync_AddExpr(
    PyObject *asyncobj, PyObject *expr,
    PyAsync_Callback result_callback, PyAsync_Error error_callback);
PyAPI_FUNC(int) PyAsync_DeferAwait(
    PyObject *asyncobj, PyAsync_Defer callback);
PyAPI_FUNC(void) PyAsync_Cancel(PyObject *asyncobj);
PyAPI_FUNC(int) PyAsync_AsyncWith(
    PyObject *asyncobj, PyObject *context_manager,
    PyAsync_Callback result_callback, PyAsync_Error error_callback);

/* The object slot holds a strong reference; GetValue returns a new one.
   Either slot can be cleared by setting it to NULL. */
PyAPI_FUNC(int) PyAsync_SetValue(PyObject *asyncobj, PyObject *value);
PyAPI_FUNC(PyObject *) PyAsync_GetValue(PyObject *asyncobj);
PyAPI_FUNC(int) PyAsync_SetArbValue(PyObject *asyncobj, void *value);
PyAPI_FUNC(void *) PyAsync_GetArbValue(PyObject *asyncobj);

#endif /* !Py_LIMITED_API || Py_LIMITED_API >= 3.16 */

#ifdef __cplusplus
}
#endif
#endif /* !Py_ASYNCOBJECT_H */
