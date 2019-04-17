import unittest
import bt2
import os

TEST_CTF_TRACES_PATH = os.environ['TEST_CTF_TRACES_PATH']


def sort_by_name(trace):
    return trace['name']


def sort_by_class_id_and_id(stream):
    return (stream['class-id'], stream['id'] if 'id' in stream else None)


class MergeByUUIDTest(unittest.TestCase):

    def test_no_stream_id(self):
        p = os.path.join(TEST_CTF_TRACES_PATH, 'session-rotation', 'no-stream-id')

        ctf_fs = bt2.find_plugin('ctf').source_component_classes['fs']

        query_exec = bt2.QueryExecutor()
        traces = query_exec.query(ctf_fs, 'trace-info', {
            'paths': [p]
        })

        self.assertEqual(len(traces), 3)

        traces = sorted(traces, key=sort_by_name)

        # Trace A (combination of a1 and a2)
        trace_a = traces[0]
        self.assertEqual(trace_a['name'], '12345678-1234-5678-1234-567812345678')
        self.assertTrue(str(trace_a['path']).endswith('/session-rotation/no-stream-id/a1'))

        # Streams didn't get merged, because we don't have stream instance ids.
        a_streams = sorted(trace_a['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(a_streams), 2)

        self.assertEqual(a_streams[0]['class-id'], 0)
        self.assertEqual(len(a_streams[0]['paths']), 1)
        self.assertTrue(str(a_streams[0]['paths'][0]).endswith('a1/the_stream_class-0-0'))

        self.assertEqual(a_streams[1]['class-id'], 0)
        self.assertEqual(len(a_streams[1]['paths']), 1)
        self.assertTrue(str(a_streams[1]['paths'][0]).endswith('a2/the_stream_class-0-0'))

        # Trace B
        trace_b = traces[1]
        self.assertEqual(trace_b['name'], 'no-stream-id/b')
        self.assertTrue(str(trace_b['path']).endswith('/session-rotation/no-stream-id/b'))

        b_streams = sorted(trace_b['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(b_streams), 1)
        self.assertEqual(len(b_streams[0]['paths']), 1)
        self.assertTrue(str(b_streams[0]['paths'][0]).endswith('b/the_stream_class-0-0'))

        # Trace C
        trace_c = traces[2]
        self.assertEqual(trace_c['name'], 'no-stream-id/c')
        self.assertTrue(str(trace_c['path']).endswith('/session-rotation/no-stream-id/c'))

        c_streams = sorted(trace_c['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(c_streams), 1)
        self.assertEqual(len(c_streams[0]['paths']), 1)
        self.assertTrue(str(c_streams[0]['paths'][0]).endswith('c/the_stream_class-0-0'))

    def test_with_stream_id(self):
        p = os.path.join(TEST_CTF_TRACES_PATH, 'session-rotation', 'with-stream-id')

        ctf_fs = bt2.find_plugin('ctf').source_component_classes['fs']

        query_exec = bt2.QueryExecutor()
        traces = query_exec.query(ctf_fs, 'trace-info', {
            'paths': [p]
        })

        self.assertEqual(len(traces), 3)

        traces = sorted(traces, key=sort_by_name)

        # Trace A (combination of a1 and a2)
        trace_a = traces[0]
        self.assertEqual(trace_a['name'], '12345678-1234-5678-1234-567812345678')
        self.assertTrue(str(trace_a['path']).endswith('/session-rotation/with-stream-id/a1'))

        a_streams = sorted(trace_a['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(a_streams), 3)

        a_stream_1_11 = a_streams[0]
        self.assertEqual(a_stream_1_11['class-id'], 1)
        self.assertEqual(a_stream_1_11['id'], 11)
        self.assertEqual(len(a_stream_1_11['paths']), 2)
        self.assertTrue(str(a_stream_1_11['paths'][0]).endswith('a1/the_stream_class_1-1-11'))
        self.assertTrue(str(a_stream_1_11['paths'][1]).endswith('a2/the_stream_class_1-1-11'))

        a_stream_1_22 = a_streams[1]
        self.assertEqual(a_stream_1_22['class-id'], 1)
        self.assertEqual(a_stream_1_22['id'], 22)
        self.assertEqual(len(a_stream_1_22['paths']), 1)
        self.assertTrue(str(a_stream_1_22['paths'][0]).endswith('a1/the_stream_class_1-1-22'))

        a_stream_2_11 = a_streams[2]
        self.assertEqual(a_stream_2_11['class-id'], 2)
        self.assertEqual(a_stream_2_11['id'], 11)
        self.assertEqual(len(a_stream_2_11['paths']), 1)
        self.assertTrue(str(a_stream_2_11['paths'][0]).endswith('a2/the_stream_class_2-2-11'))

        # Trace B
        trace_b = traces[1]
        self.assertEqual(trace_b['name'], 'with-stream-id/b')
        self.assertTrue(str(trace_b['path']).endswith('/session-rotation/with-stream-id/b'))

        b_streams = sorted(trace_b['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(b_streams), 1)
        self.assertEqual(len(b_streams[0]['paths']), 1)
        self.assertTrue(str(b_streams[0]['paths'][0]).endswith('b/the_stream_class_1-1-11'))

        # Trace C
        trace_c = traces[2]
        self.assertEqual(trace_c['name'], 'with-stream-id/c')
        self.assertTrue(str(trace_c['path']).endswith('/session-rotation/with-stream-id/c'))

        c_streams = sorted(trace_c['streams'], key=sort_by_class_id_and_id)
        self.assertEqual(len(c_streams), 1)
        self.assertEqual(len(c_streams[0]['paths']), 1)
        self.assertTrue(str(c_streams[0]['paths'][0]).endswith('c/the_stream_class_1-1-11'))
