import unittest

from test import support
from test.support import import_helper
import sys
import itertools
import os
import textwrap
import weakref

_testcapi = import_helper.import_module("_testcapi")


class TestCAPI(unittest.TestCase):
    def test_immortal_builtins(self):
        _testcapi.test_immortal_builtins()

    def test_immortal_small_ints(self):
        _testcapi.test_immortal_small_ints()


_IMMORTAL_REFCNT = sys.getrefcount(None)


class SomeType:
    pass


class TestUserImmortalObjects(unittest.TestCase):
    def immortalize(self, obj, *, already=False, loose=None):
        refcnt = sys.getrefcount(obj)

        if loose is None:
            if already is True:
                self.assertEqual(refcnt, _IMMORTAL_REFCNT)
                self.assertTrue(_testcapi.is_immortal(obj))
            else:
                self.assertNotEqual(refcnt, _IMMORTAL_REFCNT)
                self.assertFalse(_testcapi.is_immortal(obj))

        _testcapi.immortalize_object(obj)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(_testcapi.is_immortal(obj))

        # Test that double-immortalization is a no-op
        _testcapi.immortalize_object(obj)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        self.assertTrue(_testcapi.is_immortal(obj))

        return obj

    def assert_mortal(self, obj):
        self.assertNotEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        return obj

    def test_strings(self):
        # Mortal interned string
        self.immortalize(sys.intern("interned string"))

        self.immortalize("a", already=True)  # 1-byte string
        self.immortalize("nobody expects the spanish inquisition")
        self.immortalize("not interned bytes".encode("utf-8"))

        sys.intern(self.immortalize("i'm immortal"))

        # Now, test using those strings again.
        self.immortalize(b"i'm immortal".decode("utf-8") + "abc")

    def test_bytes(self):
        self.immortalize(b"byte string")
        self.immortalize(b"a", already=True, loose=True)  # 1-byte string
        self.immortalize(bytes.fromhex("FFD9"))

        # Make sure that using it doesn't accidentially make it immortal
        self.assert_mortal(b"byte string" + b"abc")

    def test_bytearray(self):
        self.immortalize(bytearray([1, 2, 3, 4]))
        self.immortalize(bytearray(self.assert_mortal("my silly string"), "utf-8"))
        self.immortalize(bytearray(self.assert_mortal(999)))

    def test_memoryview(self):
        self.immortalize(memoryview(self.assert_mortal(bytearray("XYZ", "utf-8"))))
        # Note: immortalizing the bytearray causes a false positive
        # SystemError (it shows up as "Error in sys.excepthook" because the
        # interpreter is almost dead) during finalization, because the
        # memoryview is also immortal, and it hasn't given the buffer back.
        # However, this is perfectly safe because of deferred memory deletion,
        # so it's really just a cause for confusion. I'm disabling it for now.

        # ba = self.immortalize(bytearray("XYZ", 'utf-8'))
        ba = bytearray("XYZ", "utf-8")
        memoryview(ba)
        self.immortalize(memoryview(ba))

        # Mortal view, immortal buffer
        self.assert_mortal(memoryview(self.immortalize(b"a duck"))).release()

        # Immortal view, mortal buffer
        self.immortalize(memoryview(self.assert_mortal(b"i'm mortal"))).release()

    def test_numbers(self):
        for i in range(1, 256):
            with self.subTest(i=i):
                self.immortalize(i, already=True)

        self.immortalize(1000)
        self.immortalize(10**1000)  # Really big number
        self.immortalize(100j)
        self.immortalize(0.0)
        self.immortalize(42.42)

    def sequence(self, constructor):
        with self.subTest(constructor=constructor):
            self.immortalize(constructor((1, 2, 3, False)))
            self.immortalize(constructor(("hello", sys.intern("world"))))
            self.immortalize(constructor(("hello", sys.intern("hello"), None, True)))
            self.immortalize(constructor(("hello", SomeType(), 1, 2, 3, b"a", "")))

            # Some random types
            self.immortalize(constructor((SomeType, range)))

    def circular(self, constructor):
        with self.subTest(constructor=constructor):
            test = constructor()
            self.immortalize(constructor((test, self.assert_mortal(999))))

    def test_lists(self):
        self.immortalize([])
        self.sequence(list)
        self.circular(list)

    def test_tuples(self):
        self.immortalize((), already=True)  # Interpreter constant
        self.sequence(tuple)
        self.circular(tuple)

    def test_sets(self):
        self.immortalize(set())
        self.immortalize(frozenset())
        self.sequence(set)
        self.sequence(frozenset)
        self.circular(frozenset)

    def test_dicts(self):
        self.immortalize({})
        self.sequence(lambda seq: {a: b for a, b in itertools.pairwise(seq)})
        x = {}
        x["y"] = x
        _testcapi.immortalize_object(x)

        y = {}
        y["x"] = y
        y["y"] = x
        x["y"] = y
        _testcapi.immortalize_object(y)

    def test_types(self):
        for static_type in (type, range, str, list, int, dict, super):
            self.immortalize(static_type, already=True)

        class A:
            pass

        # Type with mortal parent class
        class B(A):
            pass

        import io

        for heap_type in (SomeType, unittest.TestCase, io.StringIO, B):
            self.immortalize(heap_type)

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

    def test_files(self):
        for mode in ("w", "a", "r", "r+", "w+"):
            with self.subTest(mode=mode):
                with open(os.devnull, mode) as file:
                    self.immortalize(file)

    def test_finalizers(self):
        class Bar:
            pass

        # Immortalize while finalizing
        something = SomeType()
        weakref.finalize(something, lambda: self.immortalize(Bar()))
        self.immortalize(something)

        def finalize():
            self.assertIsInstance(something, SomeType)

        class Whatever:
            pass

        some_immortal = self.immortalize(Whatever())
        weakref.finalize(some_immortal, finalize)

    def test_zip(self):
        zip_iter = zip([2000, 3000, 4000], [5000, 6000, 7000])
        self.immortalize(zip_iter)
        for a, b in zip_iter:
            with self.subTest(a=a, b=b):
                self.assert_mortal(a)
                self.assert_mortal(b)

    def test_iter(self):
        # Immortal iterator, mortal contents
        for i in self.immortalize(
            iter(["nobody immortalizes", "the spanish inquisition"])
        ):
            with self.subTest(i=i):
                self.assert_mortal(i)

        # Mortal iterator, immortal contents
        mortal_iterator = self.assert_mortal(
            iter([self.immortalize(9999), self.immortalize(-9999)])
        )
        for _ in mortal_iterator:
            self.assert_mortal(mortal_iterator)

        self.assert_mortal(mortal_iterator)

        # Immortal iterator, immortal contents
        for _ in self.immortalize(iter([9999, -9999])):
            pass

    def test_freelist(self):
        # I remember _asyncio.FutureIter was doing
        # some exotic things with freelists (GH-122695)
        import asyncio

        loop = asyncio.new_event_loop()
        try:
            self.immortalize(iter(loop.create_task(asyncio.sleep(0))))
        finally:
            loop.close()

    def test_modules(self):
        import io
        import _io
        import traceback

        self.immortalize(_io)  # Multi-phase init
        self.immortalize(_testcapi)  # Single-phase init
        self.immortalize(io)  # Python module
        self.immortalize(traceback)  # Used by the interpreter

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

    def test_frames(self):
        import inspect
        import weakref
        import contextlib

        # Wrap it in a function for another frame to get pushed
        def inner():
            class Foo:
                pass

            hello = self.assert_mortal("silly")
            some_other_var = self.assert_mortal(6000)

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

    def test_sys_immortalize(self):
        # sys.immortalize() does pretty much the same thing
        # as Py_Immortalize(), but we might as well test it.
        mortal = self.assert_mortal([1, 2, 3])
        sys.immortalize(mortal)
        self.assertEqual(sys.getrefcount(mortal), _IMMORTAL_REFCNT)

    @support.requires_resource("cpu")
    def test_the_party_pack(self):
        import _interpreters

        source = textwrap.dedent("""
        import sys
        import contextlib

        # These have weird side effects
        blacklisted = {"antigravity", "this"}

        def immortalize_everything(mod):
            sys.immortalize(mod)

            for i in dir(mod):
                attr = getattr(mod, i)
                sys.immortalize(attr)

                #for x in dir(attr):
                #    with contextlib.suppress(Exception):
                #        sys.immortalize(getattr(attr, x))


        for i in sys.stdlib_module_names:
            if i in blacklisted:
                continue

            with contextlib.suppress(ImportError):
                immortalize_everything(__import__(i))
        """)
        interp = _interpreters.create()
        res = _interpreters.run_string(interp, source)
        self.assertEqual(res, None)


if __name__ == "__main__":
    unittest.main()
