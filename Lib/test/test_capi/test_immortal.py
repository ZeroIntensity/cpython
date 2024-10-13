import unittest
from test.support import import_helper
import sys
_testcapi = import_helper.import_module('_testcapi')


class TestCAPI(unittest.TestCase):
    def test_immortal_builtins(self):
        _testcapi.test_immortal_builtins()

    def test_immortal_small_ints(self):
        _testcapi.test_immortal_small_ints()


_IMMORTAL_REFCNT = 4294967295


class TestUserImmortalObjects(unittest.TestCase):
    def immortalize(self, obj, *, already=False):
        refcnt = sys.getrefcount(obj)

        if already is True:
            self.assertEqual(refcnt, _IMMORTAL_REFCNT)
        else:
            self.assertNotEqual(refcnt, _IMMORTAL_REFCNT)

        _testcapi.immortalize_object(obj)
        self.assertEqual(sys.getrefcount(obj), 4294967295)

    def test_strings(self):
        self.immortalize(sys.intern("interned string"))
        self.immortalize(b"byte string")
        self.immortalize(b"a", already=True)  # Latin 1-byte string

        # Testing for strings that are not interned
        self.immortalize(str(b"whatever"))
        self.immortalize("not interned bytes".encode('utf-8'))


if __name__ == "__main__":
    unittest.main()
