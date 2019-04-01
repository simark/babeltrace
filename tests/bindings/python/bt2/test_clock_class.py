import unittest
import uuid
import copy
import bt2
from collections import OrderedDict
from test_utils.test_utils import run_in_component_init


# Raise "create a graph to generate event notification so we can change and test the clock value using the
# Bt_event_set_clock_value"

class ClockClassOffsetTestCase(unittest.TestCase):
    def test_create_default(self):
        cco = bt2.ClockClassOffset()
        self.assertEqual(cco.seconds, 0)
        self.assertEqual(cco.cycles, 0)

    def test_create(self):
        cco = bt2.ClockClassOffset(23, 4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_kwargs(self):
        cco = bt2.ClockClassOffset(seconds=23, cycles=4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_invalid_seconds(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset('hello', 4871232)

    def test_create_invalid_cycles(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset(23, 'hello')

    def test_eq(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 42)
        self.assertEqual(cco1, cco2)

    def test_ne_seconds(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(24, 42)
        self.assertNotEqual(cco1, cco2)

    def test_ne_cycles(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 43)
        self.assertNotEqual(cco1, cco2)

    def test_eq_invalid(self):
        self.assertFalse(bt2.ClockClassOffset() == 23)


class ClockClassTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            return comp_self._create_clock_class('salut', 1000000)

        self._cc = run_in_component_init(f)

    def tearDown(self):
        del self._cc

    def test_create_default(self):
        self.assertEqual(self._cc.name, 'salut')

    def test_create_full(self):
        my_uuid = uuid.uuid1()

        def f(comp_self):
            return comp_self._create_clock_class(name='name', description='some description',
                                                 frequency=1001, precision=176,
                                                 offset=bt2.ClockClassOffset(45, 3),
                                                 origin_is_unix_epoch=True, uuid=my_uuid)

        cc = run_in_component_init(f)

        self.assertEqual(cc.name, 'name')
        self.assertEqual(cc.description, 'some description')
        self.assertEqual(cc.frequency, 1001)
        self.assertEqual(cc.precision, 176)
        self.assertEqual(cc.offset, bt2.ClockClassOffset(45, 3))
        self.assertEqual(cc.origin_is_unix_epoch, True)
        self.assertEqual(cc.uuid, copy.deepcopy(my_uuid))

    def test_create_no_uuid(self):
        self.assertIsNone(self._cc.uuid)

    def test_assign_name(self):
        self._cc.name = 'the_clock'
        self.assertEqual(self._cc.name, 'the_clock')

    def test_assign_invalid_name(self):
        with self.assertRaises(TypeError):
            self._cc.name = 23

    def test_assign_description(self):
        self._cc.description = 'hi people'
        self.assertEqual(self._cc.description, 'hi people')

    def test_assign_invalid_description(self):
        with self.assertRaises(TypeError):
            self._cc.description = 23

    def test_assign_frequency(self):
        self._cc.frequency = 987654321
        self.assertEqual(self._cc.frequency, 987654321)

    def test_assign_invalid_frequency(self):
        with self.assertRaises(TypeError):
            self._cc.frequency = 'lel'

    def test_assign_precision(self):
        self._cc.precision = 12
        self.assertEqual(self._cc.precision, 12)

    def test_assign_invalid_precision(self):
        with self.assertRaises(TypeError):
            self._cc.precision = 'lel'

    def test_assign_offset(self):
        self._cc.offset = bt2.ClockClassOffset(12, 56)
        self.assertEqual(self._cc.offset, bt2.ClockClassOffset(12, 56))

    def test_assign_invalid_offset(self):
        with self.assertRaises(TypeError):
            self._cc.offset = object()

    def test_assign_absolute(self):
        self._cc.origin_is_unix_epoch = True
        self.assertTrue(self._cc.origin_is_unix_epoch)

    def test_assign_not_absolute(self):
        self._cc.origin_is_unix_epoch = False
        self.assertFalse(self._cc.origin_is_unix_epoch)

    def test_assign_invalid_absolute(self):
        with self.assertRaises(TypeError):
            self._cc.origin_is_unix_epoch = 23

    def test_assign_uuid(self):
        the_uuid = uuid.uuid1()
        self._cc.uuid = the_uuid
        self.assertEqual(self._cc.uuid, the_uuid)

    def test_assign_invalid_uuid(self):
        with self.assertRaises(TypeError):
            self._cc.uuid = object()


class ClockValueTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            cc = comp_self._create_clock_class('my_cc', 1000,
                                               offset=bt2.ClockClassOffset(45, 354))
            tc = comp_self._create_trace_class()

            return (cc, tc)

        _cc, _tc = run_in_component_init(f)
        _trace = _tc()
        _sc = _tc.create_stream_class()
        _sc.default_clock_class = _cc
        _ec = _sc.create_event_class(name='salut')
        _stream = _trace.create_stream(_sc)
        _packet = _stream.create_packet()
        self._packet = _packet
        self._stream = _stream
        self._ec = _ec
        self._cc = _cc

        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                self._at = 0

            def __next__(self):
                if self._at == 0:
                    notif = self._create_stream_beginning_message(_stream)
                elif self._at == 1:
                    notif = self._create_packet_beginning_message(_packet, 100)
                elif self._at == 2:
                    notif = self._create_event_message(_ec, _packet, 123)
                elif self._at == 3:
                    notif = self._create_packet_end_message(_packet)
                elif self._at == 4:
                    notif = self._create_stream_end_message(_stream)
                else:
                    raise bt2.Stop

                self._at += 1
                return notif

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        self._graph = bt2.Graph()
        self._src_comp = self._graph.add_source_component(MySrc, 'my_source')
        self._msg_iter = self._graph.create_output_port_message_iterator(
            self._src_comp.output_ports['out'])

        for i, msg in enumerate(self._msg_iter):
            if i == 2:
                self._msg = msg
                break

    def tearDown(self):
        del self._cc
        del self._msg

    def test_create_default(self):
        self.assertEqual(
            self._msg.default_clock_snapshot.clock_class.addr, self._cc.addr)
        self.assertEqual(self._msg.default_clock_snapshot.cycles, 123)

    def test_clock_class(self):
        self.assertEqual(
            self._msg.default_clock_snapshot.clock_class.addr, self._cc.addr)

    def test_create_invalid_cycles_type(self):
        with self.assertRaises(TypeError):
            self._cc('yes')

    def test_ns_from_origin(self):
        s_from_origin = 45 + ((354 + 123) / 1000)
        ns_from_origin = int(s_from_origin * 1e9)
        self.assertEqual(
            self._msg.default_clock_snapshot.ns_from_origin, ns_from_origin)

    def test_eq_int(self):
        self.assertEqual(self._msg.default_clock_snapshot, 123)

    def test_eq_invalid(self):
        self.assertFalse(self._msg.default_clock_snapshot == 23)
