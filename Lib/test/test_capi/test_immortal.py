import unittest
from test.support import import_helper
import sys
import itertools
import os
import weakref
_testcapi = import_helper.import_module('_testcapi')


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
            else:
                self.assertNotEqual(refcnt, _IMMORTAL_REFCNT)

        _testcapi.immortalize_object(obj)
        self.assertEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)

        # Test that double-immortalization is a no-op
        _testcapi.immortalize_object(obj)

        return obj

    def assert_mortal(self, obj):
        self.assertNotEqual(sys.getrefcount(obj), _IMMORTAL_REFCNT)
        return obj

    def test_strings(self):
        # Mortal interned string
        self.immortalize(sys.intern("interned string"))

        self.immortalize("a", already=True)  # 1-byte string
        self.immortalize("nobody expects the spanish inquisition")
        self.immortalize("not interned bytes".encode('utf-8'))

    def test_bytes(self):
        self.immortalize(b"byte string")
        self.immortalize(b"a", already=True, loose=True)  # 1-byte string
        self.immortalize(bytes.fromhex("FFD9"))

    def test_bytearray(self):
        self.immortalize(bytearray([1, 2, 3, 4]))
        self.immortalize(bytearray(
            self.assert_mortal("my silly string"), 'utf-8')
        )
        self.immortalize(bytearray(self.assert_mortal(999)))

    def test_memoryview(self):
        self.immortalize(
            memoryview(self.assert_mortal(bytearray("XYZ", 'utf-8')))
        )
        # Note: immortalizing the bytearray causes a false positive
        # SystemError (it shows up as "Error in sys.excepthook" because the
        # interpreter is almost dead) during finalization, because the
        # memoryview is also immortal, and it hasn't given the buffer back.
        # However, this is perfectly safe because of deferred memory deletion,
        # so it's really just a cause for confusion. I'm disabling it for now.

        # ba = self.immortalize(bytearray("XYZ", 'utf-8'))
        ba = bytearray("XYZ", 'utf-8')
        memoryview(ba)
        self.immortalize(memoryview(ba))

    def test_numbers(self):
        for i in range(1, 256):
            with self.subTest(i=i):
                self.immortalize(i, already=True)

        self.immortalize(1000)
        self.immortalize(10 ** 1000)  # Really big number
        self.immortalize(100j)
        self.immortalize(0.0)
        self.immortalize(42.42)

    def sequence(self, constructor):
        with self.subTest(constructor=constructor):
            self.immortalize(constructor((1, 2, 3, False)))
            self.immortalize(constructor(("hello", sys.intern("world"))))
            self.immortalize(constructor(
                ("hello", sys.intern("hello"), None, True)
            ))
            self.immortalize(constructor(
                ("hello", SomeType(), 1, 2, 3, b"a", "")
            ))

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
        x['y'] = x
        _testcapi.immortalize_object(x)

        y = {}
        y['x'] = y
        y['y'] = x
        x['y'] = y
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
        zip_iter = zip(
            [2000, 3000, 4000],
            [5000, 6000, 7000]
        )
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

    def test_exceptions(self):
        try:
            _ = 0 / 0
        except ZeroDivisionError as e:
            self.immortalize(e)

        self.immortalize(Exception())

    def test_sys_immortalize(self):
        # sys.immortalize() does pretty much the same thing
        # as Py_Immortalize(), but we might as well test it.
        mortal = self.assert_mortal([1, 2, 3])
        sys.immortalize(mortal)
        self.assertEqual(sys.getrefcount(mortal), _IMMORTAL_REFCNT)

if __name__ == "__main__":
    unittest.main()
