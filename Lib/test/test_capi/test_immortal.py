import unittest

from test import support
from test.support import import_helper
import sys
import functools
import inspect
import contextlib

_testcapi = import_helper.import_module("_testcapi")


class TestCAPI(unittest.TestCase):
    def test_immortal_builtins(self):
        _testcapi.test_immortal_builtins()

    def test_immortal_small_ints(self):
        _testcapi.test_immortal_small_ints()


class ImmortalUtilities:
    def immortalize(self, obj, *, already=False, loose=None):
        _IMMORTAL_REFCNT = sys.getrefcount(None)
        refcnt = sys.getrefcount(obj)

        if loose is None:
            if already is True:
                self.assertEqual(refcnt, _IMMORTAL_REFCNT, msg=f"{obj!r} should not be mortal")
                self.assertTrue(sys.is_immortal(obj))
            else:
                self.assertNotEqual(refcnt, _IMMORTAL_REFCNT, msg=f"{obj!r} should not be immortal")
                self.assertFalse(sys.is_immortal(obj))

        sys.immortalize(obj)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(sys.is_immortal(obj))

        # Test that double-immortalization is a no-op
        self.assertEqual(sys.immortalize(obj), 0)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(sys.is_immortal(obj))

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
        self.assertNotEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT, f"{obj!r} is immortal somehow")
        return obj

    def sequence(self, constructor):
        with self.subTest(constructor=constructor):
            self.immortalize(constructor((1, 2, 3, False)), loose=True)
            self.immortalize(constructor((1, 2, 3, self.mortal_int())))
            self.immortalize(constructor((self.mortal_str(), sys.intern("world"))))
            self.immortalize(constructor((self.mortal_str(), sys.intern("hello"), None, True)))
            self.immortalize(constructor((self.mortal_str(), self.mortal(), 1, self.mortal_int(), 3, b"a", "")))

            # Some random types
            self.immortalize(constructor((self.mortable_type(int), range)))

    def circular(self, constructor):
        with self.subTest(constructor=constructor):
            test = constructor()
            self.immortalize(constructor((test, self.mortal_int())))


def _gen_isolated_source(test_method):
    utilities = inspect.getsource(ImmortalUtilities)
    meth_source = inspect.getsource(test_method)

    source = f"""
import traceback
import sys
import unittest

def isolate(test_method):
    return test_method

{utilities}

class IsolatedTest(unittest.TestCase, ImmortalUtilities):
{meth_source}

IsolatedTest.Py_GIL_DISABLED = {support.Py_GIL_DISABLED}

try:
    IsolatedTest().{test_method.__name__}()
except BaseException:
    traceback.print_exc()
    raise
    """
    return source


def _isolate_subprocess(test_method, subprocess):
    source = _gen_isolated_source(test_method)
    output = subprocess.run([sys.executable, "-c", source], capture_output=True)
    if output.stderr != b"":
        raise RuntimeError(f"Error in subprocess: {output.stderr.decode('utf-8')}")


def _isolate_subinterpreter(test_method, _interpreters):
    source = _gen_isolated_source(test_method)
    interp = _interpreters.create()
    err = _interpreters.run_string(interp, source)
    if err is not None:
        raise RuntimeError(f"Error in interpreter: {err.formatted}")


def isolate(test_method):
    # For debugging and performance, don't isolate
    # the tests when executing this module directly
    if __name__ == "__main__":
        return test_method

    try:
        import _interpreters

        @functools.wraps(test_method)
        def isolated_test(self):
            _isolate_subinterpreter(test_method, _interpreters)

        raise ImportError
        return isolated_test
    except ImportError:
        # _interpreters not available
        try:
            import subprocess

            @functools.wraps(test_method)
            def isolated_test(self):
                _isolate_subprocess(test_method, subprocess)

            return isolated_test
        except ImportError:
            raise unittest.SkipTest("_interpreters and subprocess are not available")


class TestUserImmortalObjects(unittest.TestCase, ImmortalUtilities):
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
        self.immortalize(memoryview(self.assert_mortal(bytearray(self.mortal_str(), "utf-8"))))
        ba = self.immortalize(bytearray(self.mortal_str(), 'utf-8'))
        ba = bytearray(self.mortal_str(), "utf-8")

        # Trigger some weird things with finalization
        # In my prior implementations of AOI, this caused an
        # unraisable SystemError during finalization
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
        self.immortalize(10**1000, loose=self.Py_GIL_DISABLED)  # Really big number
        self.immortalize(self.mortable_type(complex)(100j))
        self.immortalize(self.mortable_type(float)(0.0))

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

    @isolate
    def test_finalizers(self):
        class Bar:
            pass

        class SomeType:
            pass

        # Immortalize while finalizing
        something = SomeType()
        import weakref
        weakref.finalize(something, lambda: self.immortalize(Bar()))
        self.immortalize(something)

        def finalize():
            self.assertIsInstance(something, SomeType)

        class Whatever:
            pass

        some_immortal = self.immortalize(Whatever())
        weakref.finalize(some_immortal, finalize)

    @isolate
    def test_zip(self):
        # Immortal iterator, mortal contents
        zip_iter = zip(
            [self.mortal_int(), self.mortal_int(), self.mortal_int()],
            [self.mortal(), self.mortal_bytes(), self.mortal_str()]
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
            zip(
                [self.immortal(), self.immortal()],
                [self.immortal(), self.immortal()]
            )
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
            iter([self.mortal_str(), self.mortal_bytes(), self.mortal_int(), self.mortal()])
        ):
            with self.subTest(i=i):
                self.assert_mortal(i)

        # Mortal iterator, immortal contents
        mortal_iterator = self.assert_mortal(
            iter([self.immortal(), self.immortal()])
        )
        for _ in mortal_iterator:
            self.assert_mortal(mortal_iterator)

        self.assert_mortal(mortal_iterator)

        # Immortal iterator, immortal contents
        for _ in self.immortalize(iter([self.immortal(), self.immortal()])):
            pass

    @isolate
    def test_freelist(self):
        # I remember _asyncio.FutureIter was doing
        # some exotic things with freelists (GH-122695)
        import asyncio

        loop = asyncio.new_event_loop()
        try:
            self.immortalize(iter(loop.create_task(asyncio.sleep(0))))
        finally:
            loop.close()

    # This can't be isolated because subinterpreters can't load
    # single-phase init modules
    def test_modules(self):
        import io
        import _io
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

        with self.subTest(exc=exc):
            # Both immortal
            self.immortalize(exc)
            self.immortalize(exc.__traceback__)

        exc = get_exc()
        with self.subTest(exc=exc):
            # Mortal exception, immortal traceback
            self.immortalize(exc.__traceback__)

    @isolate
    def test_frames(self):
        import inspect
        import weakref
        import os
        import contextlib

        # Wrap it in a function for another frame to get pushed
        def inner():
            class Foo:
                pass

            hello = self.mortal_str()
            some_other_var = self.mortal_int()

            def exotic_finalizer():
                with contextlib.suppress(NameError):
                    hello.something()
                    # We shouldn't get here
                    os.abort()

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

        inner()

    @support.requires_resource("cpu")
    @unittest.skipIf(support.Py_GIL_DISABLED, "slow for free-threading")
    @isolate
    def test_the_party_pack(self):
        # These have weird side effects
        blacklisted = {"antigravity", "this"}

        def immortalize_everything(mod):
            sys.immortalize(mod)

            for i in dir(mod):
                attr = getattr(mod, i)
                sys.immortalize(attr)

                for x in dir(attr):
                    with contextlib.suppress(Exception):
                        sys.immortalize(getattr(attr, x))


        for i in sys.stdlib_module_names:
            if i in blacklisted:
                continue

            with contextlib.suppress(ImportError):
                immortalize_everything(__import__(i))


if __name__ == "__main__":
    unittest.main()
