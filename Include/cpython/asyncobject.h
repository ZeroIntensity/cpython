/* C API for composing asynchronous operations. */

#ifndef Py_LIMITED_API
#ifndef Py_ASYNCOBJECT_H
#define Py_ASYNCOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PyAsync PyAsync;

PyAPI_DATA(PyTypeObject) PyAsync_Type;

#define PyAsync_CheckExact(op) Py_IS_TYPE((op), &PyAsync_Type)

typedef int (*PyAsync_Callback)(PyObject *, PyObject *);
typedef int (*PyAsync_Error)(PyObject *, PyObject *);
typedef int (*PyAsync_Defer)(PyObject *);

PyAPI_FUNC(PyObject *) PyAsync_New(void);
PyAPI_FUNC(int) PyAsync_SetResult(PyObject *, PyObject *);
PyAPI_FUNC(int) PyAsync_AddAwait(
    PyObject *, PyObject *, PyAsync_Callback, PyAsync_Error);
PyAPI_FUNC(int) PyAsync_AddExpr(
    PyObject *, PyObject *, PyAsync_Callback, PyAsync_Error);
PyAPI_FUNC(int) PyAsync_DeferAwait(PyObject *, PyAsync_Defer);
PyAPI_FUNC(void) PyAsync_Cancel(PyObject *);
PyAPI_FUNC(int) PyAsync_AsyncWith(
    PyObject *, PyObject *, PyAsync_Callback, PyAsync_Error);

/* The object slot holds a strong reference; GetValue returns a borrowed one.
   Either slot can be cleared by setting it to NULL. */
PyAPI_FUNC(int) PyAsync_SetValue(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyAsync_GetValue(PyObject *);
PyAPI_FUNC(int) PyAsync_SetArbValue(PyObject *, void *);
PyAPI_FUNC(void *) PyAsync_GetArbValue(PyObject *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_ASYNCOBJECT_H */
#endif /* !Py_LIMITED_API */
