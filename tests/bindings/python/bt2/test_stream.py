import unittest
from test_utils.test_utils import run_in_component_init


class StreamTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            return comp_self._create_trace_class()

        tc = run_in_component_init(f)
        self._sc = tc.create_stream_class(assigns_automatic_stream_id=False)
        trace = tc()
        self._stream = trace.create_stream(self._sc, id=1608)

    def tearDown(self):
        del self._sc
        del self._stream

    def test_attr_stream_class(self):
        self.assertEqual(self._stream.stream_class.addr, self._sc.addr)

    def test_attr_name(self):
        self._stream.name = 'my_stream'
        self.assertEqual(self._stream.name, 'my_stream')

    def test_attr_id(self):
        self.assertEqual(self._stream.id, 1608)

    def test_default_clock_always_known(self):
        pass

    def test_counter_snapshots(self):
        pass

    def test_packet_default_clock_value(self):
        pass
