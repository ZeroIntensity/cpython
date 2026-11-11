/* PyAsync: a coroutine object driven by C callbacks (PEP 852). */

#include "Python.h"
#include "pycore_genobject.h"     // _PyCoro_GetAwaitableIter()
#include "pycore_pyerrors.h"      // _PyErr_FormatFromCause()

#include <stdbool.h>

/* One queued operation: either an awaitable with its callbacks, or a
   deferred (synchronous) callback. */
typedef struct {
    PyObject *coro;
    PyAsync_Callback callback;
    PyAsync_Error err_callback;
    PyAsync_Defer defer;
} async_callback;

struct _PyAsync {
    PyObject_HEAD
    async_callback **aw_callbacks;
    Py_ssize_t aw_size;
    Py_ssize_t aw_capacity;
    Py_ssize_t aw_state;
    bool aw_done;
    bool aw_awaited;
    bool aw_running;
    bool aw_recently_cancelled;
    PyObject *aw_result;
    PyObject *aw_gen;
    PyObject *aw_object_value;
    void *aw_arbitrary_value;

    /* Set on the internal object created by PyAsync_AsyncWith(). Result and
       value storage are delegated to the root of the parent chain, so a
       callback running inside the block sees the enclosing object's state. */
    PyAsync *aw_parent;
    PyObject *aw_with_exit;         // bound __aexit__, or NULL
    PyObject *aw_with_error;        // exception being passed to __aexit__
    PyAsync_Callback aw_with_callback;
    bool aw_with_entered;           // __aenter__ completed; __aexit__ is owed
};

typedef struct {
    PyObject_HEAD
    PyAsync *gw_async;
    PyObject *gw_current_await;
} asyncgenwrapper;

extern PyTypeObject _PyAsyncGenWrapper_Type;

int
PyAsync_CheckExact(PyObject *op)
{
    return Py_IS_TYPE(op, &PyAsync_Type);
}

static PyAsync *
async_cast(PyObject *op)
{
    assert(PyAsync_CheckExact(op));
    return (PyAsync *)op;
}

/* The object whose result and value slots this object uses. */
static PyAsync *
async_root(PyAsync *aw)
{
    while (aw->aw_parent != NULL) {
        aw = aw->aw_parent;
    }
    return aw;
}

static void
async_clear_callbacks(PyAsync *aw)
{
    async_callback **callbacks = aw->aw_callbacks;
    Py_ssize_t size = aw->aw_size;
    aw->aw_callbacks = NULL;
    aw->aw_size = 0;
    aw->aw_capacity = 0;
    aw->aw_state = 0;
    for (Py_ssize_t i = 0; i < size; i++) {
        Py_XDECREF(callbacks[i]->coro);
        PyMem_Free(callbacks[i]);
    }
    PyMem_Free(callbacks);
}

static int
async_append_callback(PyAsync *aw, PyObject *coro, PyAsync_Callback callback,
                      PyAsync_Error err_callback, PyAsync_Defer defer)
{
    if (aw->aw_done) {
        PyErr_SetString(PyExc_RuntimeError, "cannot reuse a completed PyAsync");
        return -1;
    }
    if (aw->aw_size == aw->aw_capacity) {
        if ((size_t)aw->aw_capacity >
            PY_SSIZE_T_MAX / 2 / sizeof(async_callback *)) {
            PyErr_NoMemory();
            return -1;
        }
        Py_ssize_t capacity = aw->aw_capacity ? aw->aw_capacity * 2 : 16;
        async_callback **callbacks = PyMem_Realloc(
            aw->aw_callbacks, capacity * sizeof(async_callback *));
        if (callbacks == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        aw->aw_callbacks = callbacks;
        aw->aw_capacity = capacity;
    }
    async_callback *cb = PyMem_Malloc(sizeof(*cb));
    if (cb == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    cb->coro = Py_XNewRef(coro);
    cb->callback = callback;
    cb->err_callback = err_callback;
    cb->defer = defer;
    aw->aw_callbacks[aw->aw_size++] = cb;
    return 0;
}

static int
async_traverse(PyObject *self, visitproc visit, void *arg)
{
    PyAsync *aw = async_cast(self);
    for (Py_ssize_t i = 0; i < aw->aw_size; i++) {
        Py_VISIT(aw->aw_callbacks[i]->coro);
    }
    Py_VISIT(aw->aw_object_value);
    Py_VISIT(aw->aw_gen);
    Py_VISIT(aw->aw_result);
    Py_VISIT(aw->aw_parent);
    Py_VISIT(aw->aw_with_exit);
    Py_VISIT(aw->aw_with_error);
    return 0;
}

static int
async_clear(PyObject *self)
{
    PyAsync *aw = async_cast(self);
    aw->aw_done = true;
    async_clear_callbacks(aw);
    Py_CLEAR(aw->aw_object_value);
    Py_CLEAR(aw->aw_gen);
    Py_CLEAR(aw->aw_result);
    Py_CLEAR(aw->aw_parent);
    Py_CLEAR(aw->aw_with_exit);
    Py_CLEAR(aw->aw_with_error);
    aw->aw_arbitrary_value = NULL;
    aw->aw_with_callback = NULL;
    return 0;
}

static void
async_dealloc(PyObject *self)
{
    PyAsync *aw = async_cast(self);
    PyObject_GC_UnTrack(self);
    PyObject *error = PyErr_GetRaisedException();
    if (!aw->aw_awaited) {
        /* Match native coroutines, which emit a RuntimeWarning. */
        if (PyErr_WarnEx(PyExc_RuntimeWarning,
                         "PyAsync object was never awaited", 1) < 0) {
            PyErr_WriteUnraisable(self);
        }
    }
    (void)async_clear(self);
    PyObject_GC_Del(self);
    PyErr_SetRaisedException(error);
}

PyObject *
PyAsync_New(void)
{
    PyAsync *aw = PyObject_GC_New(PyAsync, &PyAsync_Type);
    if (aw == NULL) {
        return NULL;
    }
    aw->aw_callbacks = NULL;
    aw->aw_size = 0;
    aw->aw_capacity = 0;
    aw->aw_state = 0;
    aw->aw_done = false;
    aw->aw_awaited = false;
    aw->aw_running = false;
    aw->aw_recently_cancelled = false;
    aw->aw_result = NULL;
    aw->aw_gen = NULL;
    aw->aw_object_value = NULL;
    aw->aw_arbitrary_value = NULL;
    aw->aw_parent = NULL;
    aw->aw_with_exit = NULL;
    aw->aw_with_error = NULL;
    aw->aw_with_callback = NULL;
    aw->aw_with_entered = false;
    PyObject_GC_Track(aw);
    return (PyObject *)aw;
}

int
PyAsync_SetResult(PyObject *self, PyObject *result)
{
    assert(result != NULL);
    PyAsync *aw = async_root(async_cast(self));
    Py_BEGIN_CRITICAL_SECTION(aw);
    Py_XSETREF(aw->aw_result, Py_NewRef(result));
    Py_END_CRITICAL_SECTION();
    return 0;
}

int
PyAsync_SetValue(PyObject *self, PyObject *value)
{
    PyAsync *aw = async_root(async_cast(self));
    Py_BEGIN_CRITICAL_SECTION(aw);
    Py_XSETREF(aw->aw_object_value, Py_XNewRef(value));
    Py_END_CRITICAL_SECTION();
    return 0;
}

PyObject *
PyAsync_GetValue(PyObject *self)
{
    PyAsync *aw = async_root(async_cast(self));
    PyObject *value;
    Py_BEGIN_CRITICAL_SECTION(aw);
    value = Py_XNewRef(aw->aw_object_value);
    Py_END_CRITICAL_SECTION();
    return value;
}

int
PyAsync_SetArbValue(PyObject *self, void *value)
{
    PyAsync *aw = async_root(async_cast(self));
    Py_BEGIN_CRITICAL_SECTION(aw);
    aw->aw_arbitrary_value = value;
    Py_END_CRITICAL_SECTION();
    return 0;
}

void *
PyAsync_GetArbValue(PyObject *self)
{
    PyAsync *aw = async_root(async_cast(self));
    void *value;
    Py_BEGIN_CRITICAL_SECTION(aw);
    value = aw->aw_arbitrary_value;
    Py_END_CRITICAL_SECTION();
    return value;
}

static int
async_add_await_locked(PyAsync *aw, PyObject *coro, PyAsync_Callback callback,
                       PyAsync_Error err_callback)
{
    if (coro == NULL) {
        PyErr_SetString(PyExc_ValueError, "NULL passed to PyAsync_AddAwait()");
        return -1;
    }
    if (coro == (PyObject *)aw) {
        PyErr_SetString(PyExc_ValueError, "a PyAsync cannot await itself");
        return -1;
    }
    int iterable_coroutine = 0;
    if (PyGen_CheckExact(coro)) {
        PyCodeObject *code = PyGen_GetCode((PyGenObject *)coro);
        iterable_coroutine = (code->co_flags & CO_ITERABLE_COROUTINE) != 0;
        Py_DECREF(code);
    }
    if (!PyCoro_CheckExact(coro) && !iterable_coroutine &&
        (Py_TYPE(coro)->tp_as_async == NULL ||
         Py_TYPE(coro)->tp_as_async->am_await == NULL)) {
        PyErr_Format(PyExc_TypeError, "%.100s is not an awaitable object",
                     Py_TYPE(coro)->tp_name);
        return -1;
    }
    return async_append_callback(aw, coro, callback, err_callback, NULL);
}

int
PyAsync_AddAwait(PyObject *self, PyObject *coro, PyAsync_Callback callback,
                 PyAsync_Error err_callback)
{
    PyAsync *aw = async_cast(self);
    int res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = async_add_await_locked(aw, coro, callback, err_callback);
    Py_END_CRITICAL_SECTION();
    return res;
}

int
PyAsync_AddExpr(PyObject *self, PyObject *expr, PyAsync_Callback callback,
                PyAsync_Error err_callback)
{
    if (expr == NULL) {
        return -1;
    }
    int res = PyAsync_AddAwait(self, expr, callback, err_callback);
    Py_DECREF(expr);
    return res;
}

int
PyAsync_DeferAwait(PyObject *self, PyAsync_Defer callback)
{
    PyAsync *aw = async_cast(self);
    assert(callback != NULL);
    int res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = async_append_callback(aw, NULL, NULL, NULL, callback);
    Py_END_CRITICAL_SECTION();
    return res;
}

static void
async_cancel_locked(PyAsync *aw)
{
    aw->aw_recently_cancelled = true;
    aw->aw_awaited = true;
    if (aw->aw_gen != NULL) {
        asyncgenwrapper *g = (asyncgenwrapper *)aw->aw_gen;
        Py_CLEAR(g->gw_current_await);
    }
    async_clear_callbacks(aw);
}

void
PyAsync_Cancel(PyObject *self)
{
    PyAsync *aw = async_cast(self);
    Py_BEGIN_CRITICAL_SECTION(self);
    async_cancel_locked(aw);
    Py_END_CRITICAL_SECTION();
}

static int
genwrapper_traverse(PyObject *self, visitproc visit, void *arg)
{
    asyncgenwrapper *g = (asyncgenwrapper *)self;
    Py_VISIT(g->gw_current_await);
    Py_VISIT(g->gw_async);
    return 0;
}

static int
genwrapper_clear(PyObject *self)
{
    asyncgenwrapper *g = (asyncgenwrapper *)self;
    Py_CLEAR(g->gw_current_await);
    Py_CLEAR(g->gw_async);
    return 0;
}

static void
genwrapper_dealloc(PyObject *self)
{
    PyObject_GC_UnTrack(self);
    (void)genwrapper_clear(self);
    PyObject_GC_Del(self);
}

static PyObject *
async_await_locked(PyAsync *aw)
{
    if (aw->aw_done) {
        PyErr_SetString(PyExc_RuntimeError, "cannot reuse a completed PyAsync");
        return NULL;
    }
    if (aw->aw_gen != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "PyAsync is already being awaited");
        return NULL;
    }
    asyncgenwrapper *g = PyObject_GC_New(
        asyncgenwrapper, &_PyAsyncGenWrapper_Type);
    if (g == NULL) {
        return NULL;
    }
    g->gw_async = (PyAsync *)Py_NewRef(aw);
    g->gw_current_await = NULL;
    aw->aw_gen = Py_NewRef(g);
    aw->aw_awaited = true;
    PyObject_GC_Track(g);
    return (PyObject *)g;
}

static PyObject *
async_await(PyObject *self)
{
    PyObject *res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = async_await_locked(async_cast(self));
    Py_END_CRITICAL_SECTION();
    return res;
}

/* Validate the return value of a callback against the exception state.
   Callbacks must set an exception if and only if they report failure.
   Return 1 for success, 0 for "failed, run the error callback", and -1 for
   "failed, propagate", always with an exception set for the latter two. */
static int
async_check_callback_result(int res, const char *what)
{
    if (res > 0) {
        if (PyErr_Occurred()) {
            _PyErr_FormatFromCause(PyExc_SystemError,
                "PyAsync %s returned success with an exception set", what);
            return -1;
        }
        return 1;
    }
    if (!PyErr_Occurred()) {
        PyErr_Format(PyExc_SystemError,
            "PyAsync %s returned %d without setting an exception", what, res);
        return -1;
    }
    return res == 0 ? 0 : -1;
}

/* Run the error callback for the current exception. Return 0 if the error
   was handled and execution should continue, or -1 with an exception set. */
static int
async_fire_error(PyAsync *aw, PyAsync_Error callback)
{
    assert(PyErr_Occurred());
    if (callback == NULL) {
        return -1;
    }
    PyObject *error = PyErr_GetRaisedException();
    int res = callback((PyObject *)aw, error);
    if (res > 0) {
        Py_DECREF(error);
        if (PyErr_Occurred()) {
            _PyErr_FormatFromCause(PyExc_SystemError,
                "PyAsync error callback returned success "
                "with an exception set");
            return -1;
        }
        return 0;
    }
    if (res == 0) {
        if (PyErr_Occurred()) {
            Py_DECREF(error);
            _PyErr_FormatFromCause(PyExc_SystemError,
                "PyAsync error callback returned 0 with an exception set");
            return -1;
        }
        PyErr_SetRaisedException(error);
        return -1;
    }
    Py_DECREF(error);
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_SystemError,
            "PyAsync error callback returned -1 without setting an exception");
    }
    return -1;
}

static void
genwrapper_callback_done(asyncgenwrapper *g, async_callback *cb)
{
    /* Detach both references before running any destructors, which may cancel
       the PyAsync and free cb. */
    PyObject *coro = cb->coro;
    cb->coro = NULL;
    PyObject *current = g->gw_current_await;
    g->gw_current_await = NULL;
    Py_XDECREF(coro);
    Py_XDECREF(current);
}

static PySendResult
genwrapper_throw_current(PyObject *current, PyObject *error, PyObject **result)
{
    PyObject *throw;
    int found = PyObject_GetOptionalAttrString(current, "throw", &throw);
    if (found < 0) {
        return PYGEN_ERROR;
    }
    if (!found) {
        PyErr_SetRaisedException(Py_NewRef(error));
        return PYGEN_ERROR;
    }
    *result = PyObject_CallOneArg(throw, error);
    Py_DECREF(throw);
    if (*result != NULL) {
        return PYGEN_NEXT;
    }
    if (_PyGen_FetchStopIterationValue(result) == 0) {
        return PYGEN_RETURN;
    }
    return PYGEN_ERROR;
}

/* Result callback for the awaited __aexit__(). */
static int
async_with_exit(PyObject *self, PyObject *result)
{
    PyAsync *aw = async_cast(self);
    if (aw->aw_with_error == NULL) {
        return 1;
    }
    int suppress = PyObject_IsTrue(result);
    if (suppress < 0) {
        return -1;
    }
    PyObject *error = aw->aw_with_error;
    aw->aw_with_error = NULL;
    if (suppress) {
        Py_DECREF(error);
        return 1;
    }
    PyErr_SetRaisedException(error);
    return -1;
}

/* Queue the __aexit__() call. *error* is a borrowed reference to the
   exception raised by the block, or NULL for a normal exit. Any operations
   remaining in the queue are discarded. */
static int
async_with_schedule_exit(PyAsync *aw, PyObject *error)
{
    assert(aw->aw_with_entered && aw->aw_with_exit != NULL);
    PyObject *exit = aw->aw_with_exit;
    aw->aw_with_exit = NULL;
    aw->aw_with_entered = false;
    async_clear_callbacks(aw);
    Py_XSETREF(aw->aw_with_error, Py_XNewRef(error));

    PyObject *args[3] = {Py_None, Py_None, Py_None};
    PyObject *tb = NULL;
    if (error != NULL) {
        tb = PyException_GetTraceback(error);
        args[0] = (PyObject *)Py_TYPE(error);
        args[1] = error;
        args[2] = tb != NULL ? tb : Py_None;
    }
    PyObject *coro = PyObject_Vectorcall(exit, args, 3, NULL);
    Py_XDECREF(tb);
    Py_DECREF(exit);
    if (coro == NULL) {
        return -1;
    }
    int res = async_add_await_locked(aw, coro, async_with_exit, NULL);
    Py_DECREF(coro);
    return res;
}

/* Result callback for the awaited __aenter__(). From here on, __aexit__()
   is owed, whatever happens in the block. */
static int
async_with_enter_done(PyObject *self, PyObject *result)
{
    PyAsync *aw = async_cast(self);
    aw->aw_with_entered = true;
    if (aw->aw_with_callback == NULL) {
        return 1;
    }
    return aw->aw_with_callback(self, result);
}

/* An exception is set and would end execution of *aw*. Return 0 if it was
   routed to a pending __aexit__() and execution should continue, or -1 if
   it should propagate (still set). */
static int
async_on_failure(PyAsync *aw)
{
    assert(PyErr_Occurred());
    if (aw->aw_with_entered && aw->aw_with_exit != NULL) {
        PyObject *error = PyErr_GetRaisedException();
        int res = async_with_schedule_exit(aw, error);
        Py_DECREF(error);
        if (res == 0) {
            return 0;
        }
        /* Calling __aexit__ itself failed; fall through and chain. */
    }
    if (aw->aw_with_error != NULL) {
        /* Failure during __aexit__(): keep the block's exception as context,
           as the interpreter does for exceptions raised in a handler. */
        PyObject *error = PyErr_GetRaisedException();
        PyObject *context = aw->aw_with_error;
        aw->aw_with_error = NULL;
        if (error != context && PyException_GetContext(error) == NULL) {
            PyException_SetContext(error, context);
        }
        else {
            Py_DECREF(context);
        }
        PyErr_SetRaisedException(error);
    }
    return -1;
}

static PyObject *
genwrapper_resume_locked(asyncgenwrapper *g, PyAsync *aw, PyObject *value,
                         PyObject *error)
{
    if (aw->aw_running) {
        PyErr_SetString(PyExc_ValueError, "PyAsync is already executing");
        return NULL;
    }
    if (aw->aw_done) {
        PyErr_SetString(PyExc_RuntimeError, "cannot reuse a completed PyAsync");
        return NULL;
    }
    aw->aw_running = true;
    PyObject *result = NULL;

    for (;;) {
        if (g->gw_current_await == NULL && error != NULL) {
            /* Thrown in while nothing is being awaited. */
            PyErr_SetRaisedException(Py_NewRef(error));
            error = NULL;
            if (async_on_failure(aw) < 0) {
                goto done;
            }
            continue;
        }
        if (g->gw_current_await == NULL && aw->aw_state == aw->aw_size) {
            if (aw->aw_with_entered && aw->aw_with_exit != NULL) {
                if (async_with_schedule_exit(aw, NULL) < 0 &&
                    async_on_failure(aw) < 0) {
                    goto done;
                }
                continue;
            }
            _PyGen_SetStopIterationValue(
                aw->aw_result != NULL ? aw->aw_result : Py_None);
            goto done;
        }

        async_callback *cb;
        PyAsync_Callback callback;
        PyAsync_Error err_callback;
        aw->aw_recently_cancelled = false;
        if (g->gw_current_await == NULL) {
            cb = aw->aw_callbacks[aw->aw_state++];
            callback = cb->callback;
            err_callback = cb->err_callback;
            if (cb->defer != NULL) {
                int res = async_check_callback_result(
                    cb->defer((PyObject *)aw), "deferred callback");
                if (res <= 0 && async_on_failure(aw) < 0) {
                    goto done;
                }
                continue;
            }
            PyObject *coro = Py_NewRef(cb->coro);
            PyObject *iter = _PyCoro_GetAwaitableIter(coro);
            Py_DECREF(coro);
            if (aw->aw_recently_cancelled) {
                Py_XDECREF(iter);
                if (PyErr_Occurred() && async_on_failure(aw) < 0) {
                    goto done;
                }
                continue;
            }
            if (iter == NULL) {
                genwrapper_callback_done(g, cb);
                if (async_fire_error(aw, err_callback) < 0 &&
                    async_on_failure(aw) < 0) {
                    goto done;
                }
                continue;
            }
            g->gw_current_await = iter;
        }
        else {
            cb = aw->aw_callbacks[aw->aw_state - 1];
            callback = cb->callback;
            err_callback = cb->err_callback;
        }

        PyObject *current = Py_NewRef(g->gw_current_await);
        PySendResult status;
        if (error != NULL) {
            status = genwrapper_throw_current(current, error, &result);
            error = NULL;
        }
        else {
            status = PyIter_Send(current, value, &result);
        }
        Py_DECREF(current);
        value = Py_None;
        if (status == PYGEN_NEXT) {
            goto yield;
        }
        if (!aw->aw_recently_cancelled) {
            genwrapper_callback_done(g, cb);
        }
        else {
            callback = NULL;
            err_callback = NULL;
        }
        if (status == PYGEN_ERROR) {
            if (async_fire_error(aw, err_callback) < 0 &&
                async_on_failure(aw) < 0) {
                goto done;
            }
            continue;
        }
        int res = 1;
        if (callback != NULL) {
            res = async_check_callback_result(
                callback((PyObject *)aw, result), "result callback");
        }
        Py_CLEAR(result);
        if (res <= 0) {
            if ((res < 0 || async_fire_error(aw, err_callback) < 0) &&
                async_on_failure(aw) < 0) {
                goto done;
            }
        }
    }

done:
    aw->aw_done = true;
    Py_CLEAR(g->gw_current_await);
    /* The wrapper keeps its owner alive, like a native coroutine wrapper.
       Drop the reverse reference when execution finishes to break the cycle. */
    Py_CLEAR(aw->aw_gen);
yield:
    aw->aw_running = false;
    return result;
}

static PyObject *
genwrapper_resume(asyncgenwrapper *g, PyObject *value, PyObject *error)
{
    PyAsync *aw = g->gw_async;
    if (aw == NULL) {
        if (error != NULL) {
            PyErr_SetRaisedException(Py_NewRef(error));
        }
        else {
            PyErr_SetString(PyExc_RuntimeError,
                            "cannot reuse a completed PyAsync");
        }
        return NULL;
    }
    Py_INCREF(aw);
    PyObject *res;
    Py_BEGIN_CRITICAL_SECTION(aw);
    res = genwrapper_resume_locked(g, aw, value, error);
    Py_END_CRITICAL_SECTION();
    Py_DECREF(aw);
    return res;
}

static PyObject *
genwrapper_send(PyObject *self, PyObject *value)
{
    return genwrapper_resume((asyncgenwrapper *)self, value, NULL);
}

static PyObject *
genwrapper_next(PyObject *self)
{
    return genwrapper_send(self, Py_None);
}

static PyObject *
async_send_locked(PyAsync *aw, PyObject *value)
{
    PyObject *gen;
    if (aw->aw_gen == NULL) {
        if (value != Py_None) {
            PyErr_SetString(PyExc_TypeError,
                "can't send non-None value to a just-started PyAsync");
            return NULL;
        }
        gen = async_await_locked(aw);
        if (gen == NULL) {
            return NULL;
        }
    }
    else {
        gen = Py_NewRef(aw->aw_gen);
    }
    PyObject *res = genwrapper_resume_locked(
        (asyncgenwrapper *)gen, aw, value, NULL);
    Py_DECREF(gen);
    return res;
}

static PyObject *
async_send(PyObject *self, PyObject *value)
{
    PyObject *res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = async_send_locked(async_cast(self), value);
    Py_END_CRITICAL_SECTION();
    return res;
}

static PySendResult
async_am_send(PyObject *self, PyObject *value, PyObject **result)
{
    *result = async_send(self, value);
    if (*result != NULL) {
        return PYGEN_NEXT;
    }
    if (_PyGen_FetchStopIterationValue(result) == 0) {
        return PYGEN_RETURN;
    }
    return PYGEN_ERROR;
}

/* Throw *error* (a strong reference, consumed) into the object. */
static PyObject *
async_throw_locked(PyAsync *aw, PyObject *error)
{
    PyObject *res = NULL;
    if (aw->aw_gen != NULL) {
        PyObject *gen = Py_NewRef(aw->aw_gen);
        res = genwrapper_resume_locked(
            (asyncgenwrapper *)gen, aw, Py_None, error);
        Py_DECREF(gen);
    }
    else if (aw->aw_done) {
        PyErr_SetString(PyExc_RuntimeError, "cannot reuse a completed PyAsync");
    }
    else {
        /* Like a coroutine that was never started: raise at the first line. */
        aw->aw_done = true;
        aw->aw_awaited = true;
        async_clear_callbacks(aw);
        PyErr_SetRaisedException(Py_NewRef(error));
    }
    Py_DECREF(error);
    return res;
}

static PyObject *
async_throw(PyObject *self, PyObject *args)
{
    PyObject *type;
    PyObject *value = NULL;
    PyObject *traceback = NULL;
    if (!PyArg_ParseTuple(args, "O|OO:throw", &type, &value, &traceback)) {
        return NULL;
    }
    if (_PyGen_SetException(type, value, traceback) < 0) {
        return NULL;
    }
    PyObject *error = PyErr_GetRaisedException();
    if (error == NULL) {
        return NULL;
    }
    PyObject *res;
    Py_BEGIN_CRITICAL_SECTION(self);
    res = async_throw_locked(async_cast(self), error);
    Py_END_CRITICAL_SECTION();
    return res;
}

/* close(): throw GeneratorExit, like a native coroutine. Error callbacks and
   any pending __aexit__() observe it; the object may not suspend again. */
static PyObject *
async_close(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    PyAsync *aw = async_cast(self);
    PyObject *res = NULL;
    bool finished = false;
    Py_BEGIN_CRITICAL_SECTION(self);
    if (aw->aw_done) {
        finished = true;
    }
    else if (aw->aw_gen == NULL) {
        /* Never started: nothing to unwind. */
        aw->aw_done = true;
        async_cancel_locked(aw);
        finished = true;
    }
    else {
        PyErr_SetNone(PyExc_GeneratorExit);
        PyObject *error = PyErr_GetRaisedException();
        res = async_throw_locked(aw, error);
    }
    Py_END_CRITICAL_SECTION();
    if (finished) {
        Py_RETURN_NONE;
    }
    if (res != NULL) {
        Py_DECREF(res);
        PyErr_SetString(PyExc_RuntimeError, "PyAsync ignored GeneratorExit");
        return NULL;
    }
    if (PyErr_ExceptionMatches(PyExc_GeneratorExit) ||
        PyErr_ExceptionMatches(PyExc_StopIteration)) {
        PyErr_Clear();
        Py_RETURN_NONE;
    }
    return NULL;
}

static PyObject *
genwrapper_throw(PyObject *self, PyObject *args)
{
    asyncgenwrapper *g = (asyncgenwrapper *)self;
    if (g->gw_async == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "cannot reuse a completed PyAsync");
        return NULL;
    }
    PyObject *aw = Py_NewRef(g->gw_async);
    PyObject *res = async_throw(aw, args);
    Py_DECREF(aw);
    return res;
}

static PyObject *
genwrapper_close(PyObject *self, PyObject *ignored)
{
    asyncgenwrapper *g = (asyncgenwrapper *)self;
    if (g->gw_async == NULL) {
        Py_RETURN_NONE;
    }
    PyObject *aw = Py_NewRef(g->gw_async);
    PyObject *res = async_close(aw, ignored);
    Py_DECREF(aw);
    return res;
}

int
PyAsync_AsyncWith(PyObject *self, PyObject *ctx, PyAsync_Callback callback,
                  PyAsync_Error err_callback)
{
    PyObject *enter = PyObject_GetAttrString(ctx, "__aenter__");
    if (enter == NULL) {
        return -1;
    }
    PyObject *exit = PyObject_GetAttrString(ctx, "__aexit__");
    if (exit == NULL) {
        Py_DECREF(enter);
        return -1;
    }
    PyAsync *inner = (PyAsync *)PyAsync_New();
    if (inner == NULL) {
        Py_DECREF(enter);
        Py_DECREF(exit);
        return -1;
    }
    /* The block shares result and value storage with the enclosing object,
       and is driven by it, so it never warns on its own. */
    inner->aw_parent = (PyAsync *)Py_NewRef(self);
    inner->aw_awaited = true;
    inner->aw_with_exit = exit;
    inner->aw_with_callback = callback;

    PyObject *coro = PyObject_CallNoArgs(enter);
    Py_DECREF(enter);
    int res = PyAsync_AddExpr((PyObject *)inner, coro,
                              async_with_enter_done, NULL);
    if (res == 0) {
        res = PyAsync_AddAwait(self, (PyObject *)inner, NULL, err_callback);
    }
    Py_DECREF(inner);
    return res;
}

static PyMethodDef async_methods[] = {
    {"send", async_send, METH_O, NULL},
    {"close", async_close, METH_NOARGS, NULL},
    {"throw", async_throw, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyAsyncMethods async_as_async = {
    .am_await = async_await,
    .am_send = async_am_send,
};

PyTypeObject PyAsync_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "async",
    .tp_basicsize = sizeof(PyAsync),
    .tp_dealloc = async_dealloc,
    .tp_as_async = &async_as_async,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = async_traverse,
    .tp_clear = async_clear,
    .tp_methods = async_methods,
};

static PyMethodDef genwrapper_methods[] = {
    {"send", genwrapper_send, METH_O, NULL},
    {"close", genwrapper_close, METH_NOARGS, NULL},
    {"throw", genwrapper_throw, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

PyTypeObject _PyAsyncGenWrapper_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "async_wrapper",
    .tp_basicsize = sizeof(asyncgenwrapper),
    .tp_dealloc = genwrapper_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = genwrapper_traverse,
    .tp_clear = genwrapper_clear,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = genwrapper_next,
    .tp_methods = genwrapper_methods,
};
