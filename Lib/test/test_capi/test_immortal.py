import unittest
from test.support import import_helper

_testcapi = import_helper.import_module('_testcapi')


class TestCAPI(unittest.TestCase):
    def test_immortal_builtins(self):
        _testcapi.test_immortal_builtins()

    def test_immortal_small_ints(self):
        _testcapi.test_immortal_small_ints()

    def test_user_immortal_objects(self):
        class Spam:
            pass

        _testcapi.immortalize_object(Spam())

if __name__ == "__main__":
    unittest.main()
