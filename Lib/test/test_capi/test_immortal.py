import unittest
from test.support import import_helper
import sys
_testcapi = import_helper.import_module('_testcapi')
import itertools

class TestCAPI(unittest.TestCase):
    def test_immortal_builtins(self):
        _testcapi.test_immortal_builtins()

    def test_immortal_small_ints(self):
        _testcapi.test_immortal_small_ints()


_IMMORTAL_REFCNT = 4294967295


class SomeType:
    pass


class TestUserImmortalObjects(unittest.TestCase):
    def immortalize(self, obj, *, already=False):
        refcnt = sys.getrefcount(obj)

        if already is True:
            self.assertEqual(refcnt, _IMMORTAL_REFCNT)
        else:
            self.assertNotEqual(refcnt, _IMMORTAL_REFCNT)

        _testcapi.immortalize_object(obj)
        self.assertEqual(sys.getrefcount(obj), 4294967295)

        # Test that double-immortalization is a no-op
        _testcapi.immortalize_object(obj)

    def test_strings(self):
        # Mortal interned string
        self.immortalize(sys.intern("interned string"))

        self.immortalize(b"byte string")
        self.immortalize(b"a", already=True)  # Latin 1-byte string
        self.immortalize("whatever")
        self.immortalize("not interned bytes".encode('utf-8'))

    def sequence(self, constructor):
        self.immortalize(constructor((1, 2, 3, False)))
        self.immortalize(constructor(("hello", sys.intern("world"))))
        self.immortalize(constructor(
            ("hello", sys.intern("hello"), None, True)
        ))
        self.immortalize(constructor(("hello", SomeType(), 1, 2, 3, b"a", "")))

        # Some exotic types
        self.immortalize(constructor((SomeType, range)))

    def test_lists(self):
        self.immortalize([])
        self.sequence(list)

    def test_tuples(self):
        self.immortalize((), already=True)  # Interpreter constant
        self.sequence(tuple)

    def test_sets(self):
        self.immortalize(set())
        self.sequence(set)

    def test_dicts(self):
        self.immortalize({})
        self.sequence(lambda seq: {a: b for a, b in itertools.pairwise(seq)})

    def test_types(self):
        self.immortalize(SomeType)
        self.immortalize(range, already=True)
        self.immortalize(str, already=True)


if __name__ == "__main__":
    unittest.main()
