import unittest

from test import support
from test.support import import_helper
from test.support import threading_helper
import sys
import functools
import inspect
import io
import os

_testcapi = import_helper.import_module("_testcapi")
_testinternalcapi = import_helper.import_module("_testinternalcapi")


class TestUnstableCAPI(unittest.TestCase):
    def test_immortal(self):
        # Not extensive
        known_immortals = (True, False, None, 0, ())
        for immortal in known_immortals:
            with self.subTest(immortal=immortal):
                self.assertTrue(_testcapi.is_immortal(immortal))

        # Some arbitrary mutable objects
        non_immortals = (object(), self, [object()])
        for non_immortal in non_immortals:
            with self.subTest(non_immortal=non_immortal):
                self.assertFalse(_testcapi.is_immortal(non_immortal))

        # CRASHES _testcapi.is_immortal(NULL)


class TestInternalCAPI(unittest.TestCase):

    def test_immortal_builtins(self):
        for obj in range(-5, 1025):
            self.assertTrue(_testinternalcapi.is_static_immortal(obj))
        self.assertTrue(_testinternalcapi.is_static_immortal(None))
        self.assertTrue(_testinternalcapi.is_static_immortal(False))
        self.assertTrue(_testinternalcapi.is_static_immortal(True))
        self.assertTrue(_testinternalcapi.is_static_immortal(...))
        self.assertTrue(_testinternalcapi.is_static_immortal(()))
        for obj in range(1025, 1125):
            self.assertFalse(_testinternalcapi.is_static_immortal(obj))
        for obj in ([], {}, set()):
            self.assertFalse(_testinternalcapi.is_static_immortal(obj))


class ImmortalUtilities(unittest.TestCase):
    TARGET_FILE = sys.stdout.fileno()

    def write_for_test(self, msg):
        # Always use the TARGET_FILE from ImmortalUtilities
        msg = str(msg)
        file = ImmortalUtilities.TARGET_FILE
        if isinstance(file, int):
            os.write(file, msg.encode() + b"\n")
        else:
            file.write(msg + "\n")

    def immortalize(self, obj, *, already=False, loose=None):
        _IMMORTAL_REFCNT = sys.getrefcount(None)
        refcnt = sys.getrefcount(obj)

        if loose is None:
            if already is True:
                self.assertEqual(
                    refcnt, _IMMORTAL_REFCNT, msg=f"{obj!r} should not be mortal"
                )
                self.assertTrue(sys._is_immortal(obj))
            else:
                self.assertNotEqual(
                    refcnt, _IMMORTAL_REFCNT, msg=f"{obj!r} should not be immortal"
                )
                self.assertFalse(sys._is_immortal(obj))

        sys._immortalize(obj)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(sys._is_immortal(obj))

        # Test that double-immortalization is a no-op
        self.assertEqual(sys._immortalize(obj), 0)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(sys._is_immortal(obj))

        return obj

    def mortal(self):
        class MortalType:
            pass

        return self.assert_mortal(MortalType())

    def mortable_type(self, tp):
        class my_tp(tp):
            pass

        return my_tp

    def mortal_int(self):
        my_int = self.mortable_type(int)
        return self.assert_mortal(my_int(1000000))

    def mortal_str(self):
        my_str = self.mortable_type(str)
        return self.assert_mortal(my_str("nobody expects the spanish inquisition"))

    def mortal_bytes(self):
        my_bytes = self.mortable_type(bytes)
        return self.assert_mortal(my_bytes(b"peter was here"))

    def immortal(self):
        return self.immortalize(self.mortal())

    def assert_mortal(self, obj):
        _IMMORTAL_REFCNT = sys.getrefcount(None)
        self.assertNotEqual(
            sys.getrefcount(obj), _IMMORTAL_REFCNT, f"{obj!r} is immortal somehow"
        )
        return obj

    def sequence(self, constructor):
        with self.subTest(constructor=constructor):
            self.immortalize(constructor((1, 2, 3, False)), loose=True)
            self.immortalize(constructor((1, 2, 3, self.mortal_int())))
            self.immortalize(constructor((self.mortal_str(), sys.intern("world"))))
            self.immortalize(
                constructor((self.mortal_str(), sys.intern("hello"), None, True))
            )
            self.immortalize(
                constructor(
                    (
                        self.mortal_str(),
                        self.mortal(),
                        1,
                        self.mortal_int(),
                        3,
                        b"a",
                        "",
                    )
                )
            )

            # Some random types
            self.immortalize(constructor((self.mortable_type(int), range)))

    def circular(self, constructor):
        with self.subTest(constructor=constructor):
            test = constructor()
            self.immortalize(constructor((test, self.mortal_int())))


def _gen_isolated_source(test_method, pipefd):
    utilities = inspect.getsource(ImmortalUtilities)
    meth_source = inspect.getsource(test_method)

    source = f"""
import traceback
import sys
import unittest
import types
import os
from test import support
from test.support import threading_helper

def _passthrough_decorator(test_method=None, *args, **kwargs):
    if test_method is None:
        return lambda method: method
    else:
        return test_method

always_isolate = isolate = _passthrough_decorator

{utilities}

ImmortalUtilities.TARGET_FILE = {pipefd}

class IsolatedTest(ImmortalUtilities):
{meth_source}

IsolatedTest.Py_GIL_DISABLED = {support.Py_GIL_DISABLED}

try:
    IsolatedTest().{test_method.__name__}()
except BaseException:
    traceback.print_exc()
    raise
"""
    return source


def _compare_stdout_str(needed, found):
    if isinstance(found, bytes):
        found = found.decode("utf-8")

    if found == "":
        raise RuntimeError(
            f"stdout was empty, but expected a required string {needed!r}"
        )

    if needed not in found:
        raise RuntimeError(
            f"stdout {found!r} did not contain required string {needed!r}"
        )


def _compare_stdout(needed, found):
    if isinstance(needed, str):
        _compare_stdout_str(needed, found)
    elif isinstance(needed, list):
        for required in needed:
            _compare_stdout_str(required, found)
    else:
        raise TypeError(f"unexpected type for needed: {needed!r}")


def _capture_in_process(test_method, required_stdout=None):
    @functools.wraps(test_method)
    def decorator(*args, **kwargs):
        buffer = io.StringIO()
        ImmortalUtilities.TARGET_FILE = buffer
        test_method(*args, **kwargs)

        if required_stdout is not None:
            _compare_stdout(required_stdout, buffer.getvalue())

    return decorator


def _isolate_subprocess(test_method, subprocess, required_stdout=None):
    source = _gen_isolated_source(test_method, sys.stdout.fileno())
    output = subprocess.run([sys.executable, "-c", source], capture_output=True)
    if output.stderr != b"":
        raise RuntimeError(f"Error in subprocess: {output.stderr.decode('utf-8')}")
    if required_stdout is not None:
        _compare_stdout(required_stdout, output.stdout)


def _isolate_subinterpreter(test_method, _interpreters, required_stdout=None):
    read, write = os.pipe()
    source = _gen_isolated_source(test_method, write)
    interp = _interpreters.create()
    err = _interpreters.run_string(interp, source)
    _interpreters.destroy(interp)

    if err is not None:
        raise RuntimeError(f"Error in interpreter: {err.formatted}")
    if required_stdout is not None:
        # Hack: if stdout was empty, we're about to get an error
        # anyway. os.read() will hang if we call it with an empty pipe, so
        # just stick something in there to prevent that.
        os.write(write, b".")
        captured = os.read(read, 256)
        captured = captured[:-1]  # Remove that dummy character
        _compare_stdout(required_stdout, captured)

    os.close(read)
    os.close(write)


def _get_isolator_and_module():
    # Assume _interpreters by default, fall back to subprocess otherwise
    try:
        import _interpreters as mod

        func = _isolate_subinterpreter
    except ImportError:
        # Skip entire test if neither are available
        mod = import_helper.import_module("subprocess")
        func = _isolate_subprocess

    return func, mod


def always_isolate(raw_test_method=None, *, via_subprocess=None, required_stdout=None):
    if via_subprocess is True:
        func = _isolate_subprocess
        mod = import_helper.import_module("subprocess")
    elif via_subprocess is False:
        func = _isolate_subinterpreter
        mod = import_helper.import_module("_interpreters")
    else:
        func, mod = _get_isolator_and_module()

    def decorator(test_method):
        @functools.wraps(test_method)
        def isolated_test(*args, **kwargs):
            return func(test_method, mod, required_stdout=required_stdout)

        return isolated_test

    if raw_test_method is None:
        return decorator
    else:
        return decorator(raw_test_method)


def isolate(raw_test_method=None, *, via_subprocess=None, required_stdout=None):
    def decorator(test_method):
        # For debugging and performance, don't isolate
        # the tests when executing this module directly
        if __name__ == "__main__":
            return _capture_in_process(test_method, required_stdout=required_stdout)

        return always_isolate(
            test_method, via_subprocess=via_subprocess, required_stdout=required_stdout
        )

    if raw_test_method is None:
        return decorator

    return decorator(raw_test_method)


class TestUserImmortalObjects(ImmortalUtilities):
    Py_GIL_DISABLED = support.Py_GIL_DISABLED

    @isolate
    def test_strings(self):
        # Mortal interned string
        self.immortalize(sys.intern("interned string"), loose=True)

        self.immortalize("a", already=True)  # 1-byte string
        self.immortalize(self.mortal_str())
        self.immortalize("not interned bytes".encode("utf-8"))

        sys.intern(self.immortalize("i'm immortal", loose=self.Py_GIL_DISABLED))

        # Now, test using those strings again.
        self.immortalize(b"i'm immortal".decode("utf-8") + "abc")

    @isolate
    def test_bytes(self):
        self.immortalize(b"bytes literal", loose=self.Py_GIL_DISABLED)
        self.immortalize(self.mortal_bytes())
        self.immortalize(self.mortal_str().encode("utf-8"))
        self.immortalize(b"a", already=True, loose=True)  # 1-byte string
        self.immortalize(bytes.fromhex("FFD9"))

        # All bytes are immortal on the free-threaded build
        if not self.Py_GIL_DISABLED:
            # Make sure that using it doesn't accidentially make it immortal
            self.assert_mortal(b"byte string" + b"abc")

    @isolate
    def test_bytearray(self):
        mortal_int = self.mortable_type(int)
        self.immortalize(bytearray([mortal_int(1), mortal_int(2)]))
        self.immortalize(bytearray(self.mortal_str(), "utf-8"))
        # TODO: Add more

    @isolate
    def test_memoryview(self):
        self.immortalize(
            memoryview(self.assert_mortal(bytearray(self.mortal_str(), "utf-8")))
        )
        ba = self.immortalize(bytearray(self.mortal_str(), "utf-8"))
        ba = bytearray(self.mortal_str(), "utf-8")

        # In prior implementationss of AOI, this caused an
        # unraisable SystemError during finalization, because
        # the bytearray was counted as an export.
        memoryview(ba)

        self.immortalize(memoryview(ba))

        # Mortal view, immortal buffer
        self.assert_mortal(memoryview(self.immortalize(self.mortal_bytes()))).release()

        # Immortal view, mortal buffer
        self.immortalize(memoryview(self.mortal_bytes())).release()

    @isolate
    def test_numbers(self):
        for i in range(1, 256):
            with self.subTest(i=i):
                self.immortalize(i, already=True)

        self.immortalize(self.mortal_int())
        self.immortalize(10 ** 1000, loose=self.Py_GIL_DISABLED)  # Really big number
        self.immortalize(self.mortable_type(complex)(100j))
        self.immortalize(self.mortable_type(float)(0.0))
        self.immortalize(self.mortable_type(float)('nan'))
        self.immortalize(self.mortable_type(float)('inf'))

    @isolate
    def test_lists(self):
        self.immortalize([])
        self.sequence(list)
        self.circular(list)

    @isolate
    def test_tuples(self):
        self.immortalize((), already=True)  # Interpreter constant
        self.sequence(tuple)
        self.circular(tuple)

    @isolate
    def test_sets(self):
        self.immortalize(set())
        self.immortalize(frozenset())
        self.sequence(set)
        self.sequence(frozenset)
        self.circular(frozenset)

    @isolate
    def test_dicts(self):
        import itertools

        self.immortalize({})
        self.sequence(lambda seq: {a: b for a, b in itertools.pairwise(seq)})
        x = {}
        x["y"] = x
        self.immortalize(x)

        y = {}
        y["x"] = y
        y["y"] = x
        x["y"] = y
        self.immortalize(y)

    @isolate
    def test_types(self):
        for static_type in (type, range, str, list, int, dict, super):
            self.immortalize(static_type, already=True)

        class A:
            pass

        # Type with mortal parent class
        class B(A):
            pass

        import io
        import unittest

        for heap_type in (self.mortable_type(int), unittest.TestCase, io.StringIO, B):
            self.immortalize(heap_type)

    @isolate
    def test_functions(self):
        def something():
            return 42

        self.immortalize(something.__qualname__, loose=True)
        self.immortalize(self)
        self.immortalize(something)
        self.assertEqual(something(), 42)

        class A:
            def dummy(self):
                pass

            @staticmethod
            def dummy_static():
                pass

            @classmethod
            def dummy_class(cls):
                pass

        self.immortalize(A().dummy)
        self.immortalize(A().dummy_static)
        self.immortalize(A().dummy_class)

    @isolate
    def test_files(self):
        import os

        for mode in ("w", "a", "r", "r+", "w+"):
            with self.subTest(mode=mode):
                with open(os.devnull, mode) as file:
                    self.immortalize(file)

    @always_isolate(required_stdout="called")
    def test_immortalize_while_finalizing(self):
        import weakref

        container = self.assert_mortal([])

        def immortalize_while_finalizing(obj):
            self.immortalize(obj)
            self.assert_mortal(container).append(obj)
            self.write_for_test("called")

        mortal = self.mortal()
        weakref.finalize(mortal, immortalize_while_finalizing, mortal)
        del mortal

    @always_isolate(required_stdout=["42", "24"])
    def test_finalizers_with_circular_immortals(self):
        import weakref

        circle_a = self.mortal()
        circle_b = self.mortal()
        circle_a.circle_b = self.immortalize(circle_b)
        circle_b.circle_a = self.immortalize(circle_a)

        circle_a.value = 42
        circle_b.value = 24

        def finalize_circle_a(obj_a):
            self.assertEqual(obj_a.circle_b.value, 24)
            self.write_for_test(obj_a.value)

        def finalize_circle_b(obj_b):
            self.assertEqual(obj_b.circle_a.value, 42)
            self.write_for_test(obj_b.value)

        weakref.finalize(circle_a, finalize_circle_a, circle_a)
        weakref.finalize(circle_b, finalize_circle_b, circle_b)
        # Now we can't use them in the finalizer
        del circle_a, circle_b

    @isolate
    def test_zip(self):
        # Immortal iterator, mortal contents
        zip_iter = zip(
            [self.mortal_int(), self.mortal_int(), self.mortal_int()],
            [self.mortal(), self.mortal_bytes(), self.mortal_str()],
        )
        self.immortalize(zip_iter)
        for a, b in zip_iter:
            with self.subTest(a=a, b=b):
                self.assert_mortal(a)
                self.assert_mortal(b)

        # Mortal iterator, immortal contents
        for a, b in zip(
            [self.immortal(), self.immortal()],
            [self.immortal(), self.immortal()],
        ):
            self.immortalize(a, already=True)
            self.immortalize(b, already=True)

        # Mortal iterator, immortal contents with circular reference
        mortal_zip = self.assert_mortal(
            zip([self.immortal(), self.immortal()], [self.immortal(), self.immortal()])
        )
        for a, b in mortal_zip:
            self.immortalize(a, already=True)
            self.immortalize(b, already=True)
            a.circular = mortal_zip
            b.a = a

    @isolate
    def test_iter(self):
        # Immortal iterator, mortal contents
        for i in self.immortalize(
            iter(
                [
                    self.mortal_str(),
                    self.mortal_bytes(),
                    self.mortal_int(),
                    self.mortal(),
                ]
            )
        ):
            with self.subTest(i=i):
                self.assert_mortal(i)

        # Mortal iterator, immortal contents
        mortal_iterator = self.assert_mortal(iter([self.immortal(), self.immortal()]))
        for _ in mortal_iterator:
            self.assert_mortal(mortal_iterator)

        self.assert_mortal(mortal_iterator)

        # Immortal iterator, immortal contents
        for _ in self.immortalize(iter([self.immortal(), self.immortal()])):
            pass

    @isolate
    def test_freelists(self):
        # I remember _asyncio.FutureIter was doing
        # some exotic things with freelists (GH-122695)
        import asyncio

        loop = asyncio.new_event_loop()
        try:
            self.immortalize(iter(loop.create_task(asyncio.sleep(0))))
        finally:
            loop.close()

    # This can't be isolated with a subinterpreter because subinterpreters can't load
    # single-phase init modules
    @isolate(via_subprocess=True)
    def test_modules(self):
        import _io
        import _testcapi
        import io
        import traceback

        self.immortalize(_io)  # Multi-phase init
        self.immortalize(_testcapi)  # Single-phase init
        self.immortalize(io)  # Python module
        self.immortalize(traceback)  # Used by the interpreter

    @isolate
    def test_exceptions_and_tracebacks(self):
        # Immortal exception, mortal traceback
        self.immortalize(Exception())

        def get_exc():
            try:
                _ = 0 / 0
            except ZeroDivisionError as e:
                return self.assert_mortal(e)

        exc = get_exc()
        self.assertIsNotNone(exc)

        with self.subTest(exc=exc):
            # Both immortal
            self.immortalize(exc)
            self.immortalize(exc.__traceback__)

        exc = get_exc()
        with self.subTest(exc=exc):
            # Mortal exception, immortal traceback
            self.immortalize(exc.__traceback__)

    # Frame finalizers might not be ran until interpreter shutdown, so
    # we always need to isolate it for the required stdout string.
    @always_isolate(required_stdout="finalized")
    def test_frames(self):
        import inspect
        import weakref

        # Wrap it in a function for another frame to get pushed
        def frame_finalizers():
            class Foo:
                pass

            hello = self.mortal_str()
            some_other_var = self.mortal_int()

            def exotic_finalizer():
                self.write_for_test("finalized")

            weakref.finalize(self.immortalize(Foo()), exotic_finalizer)
            frame = inspect.currentframe()
            self.assertIsNotNone(frame)
            self.immortalize(frame)
            self.immortalize(frame.f_locals)
            self.assert_mortal(hello)
            self.assert_mortal(some_other_var)

            # hello will get destroyed early, and the other one
            # will live until finalization.
            del hello

        def frame_locals():
            class Foo2:
                pass

            frame = inspect.currentframe()
            self.assertIsNotNone(frame)
            self.assert_mortal(frame)
            instance = self.immortalize(Foo2())
            del Foo2
            self.assert_mortal(type(instance))
            self.assertTrue(sys._is_immortal(frame.f_locals["instance"]))

            del frame

        frame_finalizers()
        frame_locals()

    @isolate
    def test_circular_frame_references(self):
        import inspect

        def push():
            # This will create a circular reference, because the frame holds
            # the locals. Frames aren't always garbage collected, so the
            # normal garbage collection mechanism doesn't work here.
            # sys._immortalize() has to manually update the stack references
            # held in the locals.
            frame = inspect.currentframe()
            self.immortalize(frame)

        push()

        def deeply_nested_frame():
            frame = inspect.currentframe()
            def new_frame():
                def another_new_frame():
                    x = frame
                    self.immortalize(frame)
                    frame.f_locals['x'] = frame

                another_new_frame()

            new_frame()

        deeply_nested_frame()

    @isolate
    def test_function_code(self):
        def func(value):
            return value * 2

        # Some arbitrary code objects
        code_objects = [func.__code__, self.test_function_code.__code__]
        for code in code_objects:
            with self.subTest(code=code):
                self.immortalize(code)

        # Make sure we can still use the function
        self.assertEqual(func(21), 42)

    @isolate
    def test_compiled_code(self):
        code = self.immortalize(compile("a = 1; b = 0; a / b", "<test>", "exec"))
        # Ensure that an immortal code object can be exec'd
        with self.assertRaises(ZeroDivisionError):
            exec(code)

        # Ensure that an immortal code object can be eval'd
        other_code = self.immortalize(compile("a * b", "<test>", "eval"))
        self.assertEqual(eval(other_code, locals={"a": 21, "b": 2}), 42)

    @isolate
    def test_descriptors(self):
        class Test:
            @staticmethod
            def static():
                pass

            @classmethod
            def clas(cls):
                pass

            def instance(self):
                pass

        for parent in (Test(), Test):
            with self.subTest(parent=parent):
                self.assert_mortal(parent)
                self.immortalize(parent.clas)
                self.immortalize(parent.instance)
                self.immortalize(parent.static, loose=True)

    @always_isolate(required_stdout="called")
    def test_builtins_shenanigans(self):
        # builtins is cleared very late during finalization
        import builtins
        import weakref

        def foo():
            # Use another builtin
            exec("10 + 10")
            self.assertEqual(circular.builtins.eval("41 + 1"), 42)
            self.write_for_test("called")

        class Circular:
            def __init__(self):
                self.builtins = builtins

        builtins.foo = self.immortalize(foo)
        builtins.circular = self.immortalize(Circular())
        weakref.finalize(compile, foo)

    @always_isolate(
        required_stdout=["on_exit_immortal called", "on_exit_circular called"],
        via_subprocess=True,
    )
    def test_atexit(self):
        import atexit

        @atexit.register
        def on_exit():
            self.assert_mortal(on_exit)
            self.immortalize(atexit)

        mortal_int = self.mortable_type(int)(42)
        mortal = [mortal_int]

        @atexit.register
        def on_exit_immortal():
            self.immortalize(on_exit_immortal, already=True)
            self.assert_mortal(mortal)
            self.assertEqual(mortal[0], 42)
            self.write_for_test("on_exit_immortal called")

        self.immortalize(on_exit_immortal)

        @atexit.register
        def on_exit_circular():
            self.immortalize(on_exit_circular)
            self.write_for_test("on_exit_circular called")

        on_exit_circular.circle = on_exit_circular

    @unittest.skipUnless(support.Py_GIL_DISABLED, "only meaningful under free-threading")
    @threading_helper.requires_working_threading()
    @isolate(via_subprocess=True)
    def test_immortalize_concurrently(self):
        import threading
        mortal = self.mortal()
        num_threads = 4
        barrier = threading.Barrier(num_threads)
        results = []

        def race():
            barrier.wait()
            results.append(sys._immortalize(mortal))

        with threading_helper.start_threads((threading.Thread(target=race) for _ in range(num_threads))):
            pass

        self.assertEqual(len(results), num_threads)
        self.assertEqual(results.count(1), 1)
        self.assertEqual(results.count(0), num_threads - 1)

    @threading_helper.requires_working_threading()
    @isolate(via_subprocess=True)
    def test_lingering_immortal_in_daemon_thread(self):
        import threading

        immortal = self.immortal()
        event = threading.Event()

        def linger():
            other = self.immortal()
            other.cycle = immortal
            immortal.cycle = other
            event.set()

        thread = threading.Thread(target=linger, daemon=True)
        thread.start()
        event.wait()
        self.assertTrue(hasattr(immortal, "cycle"))

    @support.requires_resource("cpu")
    @always_isolate
    def test_the_party_pack(self):
        import contextlib
        import warnings

        warnings.simplefilter("ignore", DeprecationWarning)

        # These have weird side effects
        blacklisted = {"antigravity", "this"}

        def immortalize_everything(mod):
            self.immortalize(mod, loose=True)

            for i in dir(mod):
                attr = getattr(mod, i)
                self.immortalize(attr, loose=True)

                for x in dir(attr):
                    with contextlib.suppress(Exception):
                        self.immortalize(getattr(attr, x), loose=True)

        for i in sys.stdlib_module_names:
            if i in blacklisted:
                continue

            with contextlib.suppress(ImportError):
                immortalize_everything(__import__(i))


if __name__ == "__main__":
    unittest.main()
