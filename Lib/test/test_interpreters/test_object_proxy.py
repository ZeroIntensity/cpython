import unittest

from test.support import import_helper
from test.support import threading_helper
# Raise SkipTest if subinterpreters not supported.
import_helper.import_module('_interpreters')
from concurrent.interpreters import share, SharedObjectProxy
from test.test_interpreters.utils import TestBase
from threading import Barrier, Thread
from concurrent import interpreters


class SharedObjectProxyTests(TestBase):
    def test_create(self):
        proxy = share(object())
        self.assertIsInstance(proxy, SharedObjectProxy)

        # Shareable objects should pass through
        for shareable in (None, True, False, 100, 10000, "hello", b"world", memoryview(b"test")):
            self.assertTrue(interpreters.is_shareable(shareable))
            with self.subTest(shareable=shareable):
                not_a_proxy = share(shareable)
                self.assertNotIsInstance(not_a_proxy, SharedObjectProxy)
                self.assertIs(not_a_proxy, shareable)

    @threading_helper.requires_working_threading()
    def test_create_concurrently(self):
        barrier = Barrier(4)
        def thread():
            interp = interpreters.create()
            barrier.wait()
            for iteration in range(100):
                with self.subTest(iteration=iteration):
                    interp.exec("""if True:
                    from concurrent.interpreters import share
                    import os

                    unshareable = open(os.devnull)
                    proxy = share(unshareable)
                    unshareable.close()""")

        with threading_helper.catch_threading_exception() as cm:
            with threading_helper.start_threads((Thread(target=thread) for _ in range(4))):
                pass

            if cm.exc_value is not None:
                raise cm.exc_value

    def test_access_proxy(self):
        class Test:
            def silly(self):
                return "silly"

        interp = interpreters.create()
        obj = Test()
        proxy = share(obj)
        obj.test = "silly"
        interp.prepare_main(proxy=proxy)
        interp.exec("assert proxy.test == 'silly'")
        interp.exec("assert isinstance(proxy.test, str)")
        interp.exec("""if True:
        from concurrent.interpreters import SharedObjectProxy
        method = proxy.silly
        assert isinstance(method, SharedObjectProxy)
        assert method() == 'silly'
        assert isinstance(method(), str)
        """)
        with self.assertRaises(interpreters.ExecutionFailed):
            interp.exec("proxy.noexist")


if __name__ == '__main__':
    unittest.main()
