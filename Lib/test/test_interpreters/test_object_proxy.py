import unittest

from test.support import import_helper
from test.support import threading_helper
# Raise SkipTest if subinterpreters not supported.
import_helper.import_module('_interpreters')
from concurrent.interpreters import share, SharedObjectProxy
from test.test_interpreters.utils import TestBase
from threading import Barrier, Thread, Lock
from concurrent import interpreters


class SharedObjectProxyTests(TestBase):
    def run_concurrently(self, func, num_threads=4):
        barrier = Barrier(num_threads)
        def thread():
            interp = interpreters.create()
            barrier.wait()
            func(interp)

        with threading_helper.catch_threading_exception() as cm:
            with threading_helper.start_threads((Thread(target=thread) for _ in range(num_threads))):
                pass

            if cm.exc_value is not None:
                raise cm.exc_value

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
        def thread(interp):
            for iteration in range(100):
                with self.subTest(iteration=iteration):
                    interp.exec("""if True:
                    from concurrent.interpreters import share

                    share(object())""")

        self.run_concurrently(thread)

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

    @threading_helper.requires_working_threading()
    def test_access_proxy_concurrently(self):
        class Test:
            def __init__(self):
                self.lock = Lock()
                self.value = 0

            def increment(self):
                with self.lock:
                    self.value += 1

        test = Test()
        proxy = share(test)

        def thread(interp):
            interp.prepare_main(proxy=proxy)
            for _ in range(100):
                interp.exec("proxy.increment()")
                interp.exec("assert isinstance(proxy.value, int)")

        self.run_concurrently(thread)
        self.assertEqual(test.value, 400)




if __name__ == '__main__':
    unittest.main()
