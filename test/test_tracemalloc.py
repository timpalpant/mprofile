import contextlib
import gc
import os
import sys
import mprofile
import subprocess
import unittest
from unittest.mock import patch

try:
    import _testcapi
except ImportError:
    _testcapi = None


EMPTY_STRING_SIZE = sys.getsizeof(b'')
INVALID_NFRAME = (-1, 2**30)


def get_frames(nframe, lineno_delta):
    frames = []
    frame = sys._getframe(1)
    for index in range(nframe):
        code = frame.f_code
        lineno = frame.f_lineno + lineno_delta
        frames.append((code.co_name, code.co_filename, code.co_firstlineno, lineno))
        lineno_delta = 0
        frame = frame.f_back
        if frame is None:
            break
    return tuple(frames)

def allocate_bytes(size):
    nframe = mprofile.get_traceback_limit()
    bytes_len = (size - EMPTY_STRING_SIZE)
    frames = get_frames(nframe, 1)
    data = b'x' * bytes_len
    return data, mprofile.Traceback(frames)

def create_snapshots():
    traceback_limit = 2

    # _mprofile._get_traces() returns a list of (size,
    # traceback_frames) tuples. traceback_frames is a tuple of
    # (name, filename, start_line, line_number) tuples.
    raw_traces = [
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),

        (2, (('test', 'a.py', 1, 5), ('test', 'b.py', 1, 4))),

        (66, (('test', 'b.py', 1, 1),)),

        (7, (('test', '<unknown>', 1, 0),)),
    ]
    snapshot = mprofile.Snapshot(raw_traces, traceback_limit)

    raw_traces2 = [
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
        (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),

        (2, (('test', 'a.py', 1, 5), ('test', 'b.py', 1, 4))),
        (5000, (('test', 'a.py', 1, 5), ('test', 'b.py', 1, 4))),

        (400, (('test', 'c.py', 1, 578),)),
    ]
    snapshot2 = mprofile.Snapshot(raw_traces2, traceback_limit)

    return (snapshot, snapshot2)

def frame(name, filename, firstlineno, lineno):
    return mprofile.Frame((name, filename, firstlineno, lineno))

def traceback(*frames):
    frames = tuple(('test', f[0], 1, f[1]) for f in frames)
    return mprofile.Traceback(frames)

def traceback_lineno(filename, lineno):
    return traceback((filename, lineno))

def traceback_filename(filename):
    frames = (('', filename, 0, 0), )
    return mprofile.Traceback(frames)


class TestMProfileEnabled(unittest.TestCase):
    def setUp(self):
        if mprofile.is_tracing():
            self.skipTest("mprofile must be stopped before the test")

        mprofile.start(1)

    def tearDown(self):
        mprofile.stop()

    def test_get_mprofile_memory(self):
        data = [allocate_bytes(123) for count in range(1000)]
        size = mprofile.get_tracemalloc_memory()
        self.assertGreaterEqual(size, 0)

        mprofile.clear_traces()
        size2 = mprofile.get_tracemalloc_memory()
        self.assertGreaterEqual(size2, 0)
        self.assertLessEqual(size2, size)

    def test_get_object_traceback(self):
        mprofile.clear_traces()
        obj_size = 12345
        obj, obj_traceback = allocate_bytes(obj_size)
        traceback = mprofile.get_object_traceback(obj)
        self.assertEqual(traceback, obj_traceback)

    @unittest.skip("This test fails because there's no way to hook _Py_NewReference")
    def test_new_reference(self):
        mprofile.clear_traces()
        gc.collect()

        # Create a list and "destroy it": put it in the PyListObject free list
        obj = []
        obj = None

        # Create a list which should reuse the previously created empty list
        obj = []

        nframe = mprofile.get_traceback_limit()
        frames = get_frames(nframe, -3)
        obj_traceback = mprofile.Traceback(frames)

        print("Getting reference")
        import sys
        sys.stdout.flush()
        traceback = mprofile.get_object_traceback(obj)
        self.assertIsNotNone(traceback)
        self.assertEqual(traceback, obj_traceback)

    def test_set_traceback_limit(self):
        obj_size = 10

        mprofile.stop()
        self.assertRaises(ValueError, mprofile.start, -1)

        mprofile.stop()
        mprofile.start(10)
        obj2, obj2_traceback = allocate_bytes(obj_size)
        traceback = mprofile.get_object_traceback(obj2)
        self.assertEqual(len(traceback), 10)
        self.assertEqual(traceback, obj2_traceback)

        mprofile.stop()
        mprofile.start(1)
        obj, obj_traceback = allocate_bytes(obj_size)
        traceback = mprofile.get_object_traceback(obj)
        self.assertEqual(len(traceback), 1)
        self.assertEqual(traceback, obj_traceback)

    def find_trace(self, traces, traceback):
        for trace in traces:
            if trace[1] == traceback._frames:
                return trace

        self.fail("trace not found")

    def test_get_traces(self):
        mprofile.clear_traces()
        obj_size = 12345
        obj, obj_traceback = allocate_bytes(obj_size)

        traces = mprofile._get_traces()
        trace = self.find_trace(traces, obj_traceback)

        self.assertIsInstance(trace, tuple)
        size, traceback = trace
        self.assertEqual(size, obj_size)
        self.assertEqual(traceback, obj_traceback._frames)

        mprofile.stop()
        self.assertEqual(mprofile._get_traces(), [])

    def test_get_traces_intern_traceback(self):
        # dummy wrappers to get more useful and identical frames in the traceback
        def allocate_bytes2(size):
            return allocate_bytes(size)
        def allocate_bytes3(size):
            return allocate_bytes2(size)
        def allocate_bytes4(size):
            return allocate_bytes3(size)

        # Ensure that two identical tracebacks are not duplicated
        mprofile.stop()
        mprofile.start(4)
        obj_size = 123
        obj1, obj1_traceback = allocate_bytes4(obj_size)
        obj2, obj2_traceback = allocate_bytes4(obj_size)

        traces = mprofile._get_traces()

        obj1_traceback._frames = tuple(reversed(obj1_traceback._frames))
        obj2_traceback._frames = tuple(reversed(obj2_traceback._frames))

        trace1 = self.find_trace(traces, obj1_traceback)
        trace2 = self.find_trace(traces, obj2_traceback)
        size1, traceback1 = trace1
        size2, traceback2 = trace2
        self.assertIs(traceback2, traceback1)

    def test_get_traced_memory(self):
        # Python allocates some internals objects, so the test must tolerate
        # a small difference between the expected size and the real usage
        max_error = 2048

        # allocate one object
        obj_size = 1024 * 1024
        mprofile.clear_traces()
        obj, obj_traceback = allocate_bytes(obj_size)
        size, peak_size = mprofile.get_traced_memory()
        self.assertGreaterEqual(size, obj_size)
        self.assertGreaterEqual(peak_size, size)

        self.assertLessEqual(size - obj_size, max_error)
        self.assertLessEqual(peak_size - size, max_error)

        # destroy the object
        obj = None
        size2, peak_size2 = mprofile.get_traced_memory()
        self.assertLess(size2, size)
        self.assertGreaterEqual(size - size2, obj_size - max_error)
        self.assertGreaterEqual(peak_size2, peak_size)

        # clear_traces() must reset traced memory counters
        mprofile.clear_traces()
        self.assertEqual(mprofile.get_traced_memory(), (0, 0))

        # allocate another object
        obj, obj_traceback = allocate_bytes(obj_size)
        size, peak_size = mprofile.get_traced_memory()
        self.assertGreaterEqual(size, obj_size)

        # stop() also resets traced memory counters
        mprofile.stop()
        self.assertEqual(mprofile.get_traced_memory(), (0, 0))

    def test_clear_traces(self):
        obj, obj_traceback = allocate_bytes(12345)
        traceback = mprofile.get_object_traceback(obj)
        self.assertIsNotNone(traceback)

        mprofile.clear_traces()
        traceback2 = mprofile.get_object_traceback(obj)
        self.assertIsNone(traceback2)

    def test_is_tracing(self):
        mprofile.stop()
        self.assertFalse(mprofile.is_tracing())

        mprofile.start()
        self.assertTrue(mprofile.is_tracing())

    def test_snapshot(self):
        obj, source = allocate_bytes(123)

        # take a snapshot
        snapshot = mprofile.take_snapshot()

        # mprofile must be tracing memory allocations to take a snapshot
        mprofile.stop()
        with self.assertRaises(RuntimeError) as cm:
            mprofile.take_snapshot()
        self.assertEqual(str(cm.exception),
                         "the mprofile module must be tracing memory "
                         "allocations to take a snapshot")

    def fork_child(self):
        if not mprofile.is_tracing():
            return 2

        obj_size = 12345
        obj, obj_traceback = allocate_bytes(obj_size)
        traceback = mprofile.get_object_traceback(obj)
        if traceback is None:
            return 3

        # everything is fine
        return 0

    @unittest.skipUnless(hasattr(os, 'fork'), 'need os.fork()')
    def test_fork(self):
        # check that mprofile is still working after fork
        pid = os.fork()
        if not pid:
            # child
            exitcode = 1
            try:
                exitcode = self.fork_child()
            finally:
                os._exit(exitcode)
        else:
            pid2, status = os.waitpid(pid, 0)
            self.assertTrue(os.WIFEXITED(status))
            exitcode = os.WEXITSTATUS(status)
            self.assertEqual(exitcode, 0)


class TestSnapshot(unittest.TestCase):
    maxDiff = 4000

    def test_create_snapshot(self):
        raw_traces = [(5, (('test', 'a.py', 1, 2),))]

        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(mprofile, 'is_tracing',
                                             return_value=True))
            stack.enter_context(patch.object(mprofile, 'get_traceback_limit',
                                             return_value=5))
            stack.enter_context(patch.object(mprofile, 'get_sample_rate',
                                             return_value=1))
            stack.enter_context(patch.object(mprofile, '_get_traces',
                                             return_value=raw_traces))

            snapshot = mprofile.take_snapshot()
            self.assertEqual(snapshot.traceback_limit, 5)
            self.assertEqual(len(snapshot.traces), 1)
            self.assertEqual(snapshot.sample_rate, 1)
            trace = snapshot.traces[0]
            self.assertEqual(trace.size, 5)
            self.assertEqual(len(trace.traceback), 1)
            self.assertEqual(trace.traceback[0].filename, 'a.py')
            self.assertEqual(trace.traceback[0].lineno, 2)

    def test_filter_traces(self):
        snapshot, snapshot2 = create_snapshots()
        filter1 = mprofile.Filter(False, "b.py")
        filter2 = mprofile.Filter(True, "a.py", 2)
        filter3 = mprofile.Filter(True, "a.py", 5)

        original_traces = list(snapshot.traces._traces)

        # exclude b.py
        snapshot3 = snapshot.filter_traces((filter1,))
        self.assertEqual(snapshot3.traces._traces, [
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (2, (('test', 'a.py', 1, 5), ('test', 'b.py', 1, 4))),
            (7, (('test', '<unknown>', 1, 0),)),
        ])

        # filter_traces() must not touch the original snapshot
        self.assertEqual(snapshot.traces._traces, original_traces)

        # only include two lines of a.py
        snapshot4 = snapshot3.filter_traces((filter2, filter3))
        self.assertEqual(snapshot4.traces._traces, [
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (10, (('test', 'a.py', 1, 2), ('test', 'b.py', 1, 4))),
            (2, (('test', 'a.py', 1, 5), ('test', 'b.py', 1, 4))),
        ])

        # No filter: just duplicate the snapshot
        snapshot5 = snapshot.filter_traces(())
        self.assertIsNot(snapshot5, snapshot)
        self.assertIsNot(snapshot5.traces, snapshot.traces)
        self.assertEqual(snapshot5.traces, snapshot.traces)

        self.assertRaises(TypeError, snapshot.filter_traces, filter1)

    def test_snapshot_group_by_line(self):
        snapshot, snapshot2 = create_snapshots()
        tb_0 = traceback_lineno('<unknown>', 0)
        tb_a_2 = traceback_lineno('a.py', 2)
        tb_a_5 = traceback_lineno('a.py', 5)
        tb_b_1 = traceback_lineno('b.py', 1)
        tb_c_578 = traceback_lineno('c.py', 578)

        # stats per file and line
        stats1 = snapshot.statistics('lineno')
        self.assertEqual(stats1, [
            mprofile.Statistic(tb_b_1, 66, 1),
            mprofile.Statistic(tb_a_2, 30, 3),
            mprofile.Statistic(tb_0, 7, 1),
            mprofile.Statistic(tb_a_5, 2, 1),
        ])

        # stats per file and line (2)
        stats2 = snapshot2.statistics('lineno')
        self.assertEqual(stats2, [
            mprofile.Statistic(tb_a_5, 5002, 2),
            mprofile.Statistic(tb_c_578, 400, 1),
            mprofile.Statistic(tb_a_2, 30, 3),
        ])

        # stats diff per file and line
        statistics = snapshot2.compare_to(snapshot, 'lineno')
        self.assertEqual(statistics, [
            mprofile.StatisticDiff(tb_a_5, 5002, 5000, 2, 1),
            mprofile.StatisticDiff(tb_c_578, 400, 400, 1, 1),
            mprofile.StatisticDiff(tb_b_1, 0, -66, 0, -1),
            mprofile.StatisticDiff(tb_0, 0, -7, 0, -1),
            mprofile.StatisticDiff(tb_a_2, 30, 0, 3, 0),
        ])

    def test_snapshot_group_by_file(self):
        snapshot, snapshot2 = create_snapshots()
        tb_0 = traceback_filename('<unknown>')
        tb_a = traceback_filename('a.py')
        tb_b = traceback_filename('b.py')
        tb_c = traceback_filename('c.py')

        # stats per file
        stats1 = snapshot.statistics('filename')
        self.assertEqual(stats1, [
            mprofile.Statistic(tb_b, 66, 1),
            mprofile.Statistic(tb_a, 32, 4),
            mprofile.Statistic(tb_0, 7, 1),
        ])

        # stats per file (2)
        stats2 = snapshot2.statistics('filename')
        self.assertEqual(stats2, [
            mprofile.Statistic(tb_a, 5032, 5),
            mprofile.Statistic(tb_c, 400, 1),
        ])

        # stats diff per file
        diff = snapshot2.compare_to(snapshot, 'filename')
        self.assertEqual(diff, [
            mprofile.StatisticDiff(tb_a, 5032, 5000, 5, 1),
            mprofile.StatisticDiff(tb_c, 400, 400, 1, 1),
            mprofile.StatisticDiff(tb_b, 0, -66, 0, -1),
            mprofile.StatisticDiff(tb_0, 0, -7, 0, -1),
        ])

    def test_snapshot_group_by_traceback(self):
        snapshot, snapshot2 = create_snapshots()

        # stats per file
        tb1 = traceback(('a.py', 2), ('b.py', 4))
        tb2 = traceback(('a.py', 5), ('b.py', 4))
        tb3 = traceback(('b.py', 1))
        tb4 = traceback(('<unknown>', 0))
        stats1 = snapshot.statistics('traceback')
        self.assertEqual(stats1, [
            mprofile.Statistic(tb3, 66, 1),
            mprofile.Statistic(tb1, 30, 3),
            mprofile.Statistic(tb4, 7, 1),
            mprofile.Statistic(tb2, 2, 1),
        ])

        # stats per file (2)
        tb5 = traceback(('c.py', 578))
        stats2 = snapshot2.statistics('traceback')
        self.assertEqual(stats2, [
            mprofile.Statistic(tb2, 5002, 2),
            mprofile.Statistic(tb5, 400, 1),
            mprofile.Statistic(tb1, 30, 3),
        ])

        # stats diff per file
        diff = snapshot2.compare_to(snapshot, 'traceback')
        self.assertEqual(diff, [
            mprofile.StatisticDiff(tb2, 5002, 5000, 2, 1),
            mprofile.StatisticDiff(tb5, 400, 400, 1, 1),
            mprofile.StatisticDiff(tb3, 0, -66, 0, -1),
            mprofile.StatisticDiff(tb4, 0, -7, 0, -1),
            mprofile.StatisticDiff(tb1, 30, 0, 3, 0),
        ])

        self.assertRaises(ValueError,
                          snapshot.statistics, 'traceback', cumulative=True)

    def test_snapshot_group_by_cumulative(self):
        snapshot, snapshot2 = create_snapshots()
        tb_0 = traceback_filename('<unknown>')
        tb_a = traceback_filename('a.py')
        tb_b = traceback_filename('b.py')
        tb_0_0 = traceback_lineno('<unknown>', 0)
        tb_a_2 = traceback_lineno('a.py', 2)
        tb_a_5 = traceback_lineno('a.py', 5)
        tb_b_1 = traceback_lineno('b.py', 1)
        tb_b_4 = traceback_lineno('b.py', 4)

        # per file
        stats = snapshot.statistics('filename', True)
        self.assertEqual(stats, [
            mprofile.Statistic(tb_b, 98, 5),
            mprofile.Statistic(tb_a, 32, 4),
            mprofile.Statistic(tb_0, 7, 1),
        ])

        # per line
        stats = snapshot.statistics('lineno', True)
        self.assertEqual(stats, [
            mprofile.Statistic(tb_b_1, 66, 1),
            mprofile.Statistic(tb_b_4, 32, 4),
            mprofile.Statistic(tb_a_2, 30, 3),
            mprofile.Statistic(tb_0_0, 7, 1),
            mprofile.Statistic(tb_a_5, 2, 1),
        ])

    def test_trace_format(self):
        snapshot, snapshot2 = create_snapshots()
        trace = snapshot.traces[0]
        self.assertEqual(str(trace), 'b.py:4: 10 B')
        traceback = trace.traceback
        self.assertEqual(str(traceback), 'b.py:4')
        frame = traceback[0]
        self.assertEqual(str(frame), 'b.py:4')

    def test_statistic_format(self):
        snapshot, snapshot2 = create_snapshots()
        stats = snapshot.statistics('lineno')
        stat = stats[0]
        self.assertEqual(str(stat),
                         'b.py:1: size=66 B, count=1, average=66 B')

    def test_statistic_diff_format(self):
        snapshot, snapshot2 = create_snapshots()
        stats = snapshot2.compare_to(snapshot, 'lineno')
        stat = stats[0]
        self.assertEqual(str(stat),
                         'a.py:5: size=5002 B (+5000 B), count=2 (+1), average=2501 B')

    def test_slices(self):
        snapshot, snapshot2 = create_snapshots()
        self.assertEqual(snapshot.traces[:2],
                         (snapshot.traces[0], snapshot.traces[1]))

        traceback = snapshot.traces[0].traceback
        self.assertEqual(traceback[:2],
                         (traceback[0], traceback[1]))

    def test_format_traceback(self):
        snapshot, snapshot2 = create_snapshots()
        def getline(filename, lineno):
            return '  <%s, %s>' % (filename, lineno)
        with unittest.mock.patch('mprofile.linecache.getline',
                                 side_effect=getline):
            tb = snapshot.traces[0].traceback
            self.assertEqual(tb.format(),
                             ['  File "b.py", line 4',
                              '    <b.py, 4>',
                              '  File "a.py", line 2',
                              '    <a.py, 2>'])

            self.assertEqual(tb.format(limit=1),
                             ['  File "a.py", line 2',
                              '    <a.py, 2>'])

            self.assertEqual(tb.format(limit=-1),
                             ['  File "b.py", line 4',
                              '    <b.py, 4>'])

            self.assertEqual(tb.format(most_recent_first=True),
                             ['  File "a.py", line 2',
                              '    <a.py, 2>',
                              '  File "b.py", line 4',
                              '    <b.py, 4>'])

            self.assertEqual(tb.format(limit=1, most_recent_first=True),
                             ['  File "a.py", line 2',
                              '    <a.py, 2>'])

            self.assertEqual(tb.format(limit=-1, most_recent_first=True),
                             ['  File "b.py", line 4',
                              '    <b.py, 4>'])


class TestFilters(unittest.TestCase):
    maxDiff = 2048

    def test_filter_attributes(self):
        # test default values
        f = mprofile.Filter(True, "abc")
        self.assertEqual(f.inclusive, True)
        self.assertEqual(f.filename_pattern, "abc")
        self.assertIsNone(f.lineno)
        self.assertEqual(f.all_frames, False)

        # test custom values
        f = mprofile.Filter(False, "test.py", 123, True)
        self.assertEqual(f.inclusive, False)
        self.assertEqual(f.filename_pattern, "test.py")
        self.assertEqual(f.lineno, 123)
        self.assertEqual(f.all_frames, True)

        # parameters passed by keyword
        f = mprofile.Filter(inclusive=False, filename_pattern="test.py", lineno=123, all_frames=True)
        self.assertEqual(f.inclusive, False)
        self.assertEqual(f.filename_pattern, "test.py")
        self.assertEqual(f.lineno, 123)
        self.assertEqual(f.all_frames, True)

        # read-only attribute
        self.assertRaises(AttributeError, setattr, f, "filename_pattern", "abc")

    def test_filter_match(self):
        # filter without line number
        f = mprofile.Filter(True, "abc")
        self.assertTrue(f._match_frame("abc", 0))
        self.assertTrue(f._match_frame("abc", 5))
        self.assertTrue(f._match_frame("abc", 10))
        self.assertFalse(f._match_frame("12356", 0))
        self.assertFalse(f._match_frame("12356", 5))
        self.assertFalse(f._match_frame("12356", 10))

        f = mprofile.Filter(False, "abc")
        self.assertFalse(f._match_frame("abc", 0))
        self.assertFalse(f._match_frame("abc", 5))
        self.assertFalse(f._match_frame("abc", 10))
        self.assertTrue(f._match_frame("12356", 0))
        self.assertTrue(f._match_frame("12356", 5))
        self.assertTrue(f._match_frame("12356", 10))

        # filter with line number > 0
        f = mprofile.Filter(True, "abc", 5)
        self.assertFalse(f._match_frame("abc", 0))
        self.assertTrue(f._match_frame("abc", 5))
        self.assertFalse(f._match_frame("abc", 10))
        self.assertFalse(f._match_frame("12356", 0))
        self.assertFalse(f._match_frame("12356", 5))
        self.assertFalse(f._match_frame("12356", 10))

        f = mprofile.Filter(False, "abc", 5)
        self.assertTrue(f._match_frame("abc", 0))
        self.assertFalse(f._match_frame("abc", 5))
        self.assertTrue(f._match_frame("abc", 10))
        self.assertTrue(f._match_frame("12356", 0))
        self.assertTrue(f._match_frame("12356", 5))
        self.assertTrue(f._match_frame("12356", 10))

        # filter with line number 0
        f = mprofile.Filter(True, "abc", 0)
        self.assertTrue(f._match_frame("abc", 0))
        self.assertFalse(f._match_frame("abc", 5))
        self.assertFalse(f._match_frame("abc", 10))
        self.assertFalse(f._match_frame("12356", 0))
        self.assertFalse(f._match_frame("12356", 5))
        self.assertFalse(f._match_frame("12356", 10))

        f = mprofile.Filter(False, "abc", 0)
        self.assertFalse(f._match_frame("abc", 0))
        self.assertTrue(f._match_frame("abc", 5))
        self.assertTrue(f._match_frame("abc", 10))
        self.assertTrue(f._match_frame("12356", 0))
        self.assertTrue(f._match_frame("12356", 5))
        self.assertTrue(f._match_frame("12356", 10))

    def test_filter_match_filename(self):
        def fnmatch(inclusive, filename, pattern):
            f = mprofile.Filter(inclusive, pattern)
            return f._match_frame(filename, 0)

        self.assertTrue(fnmatch(True, "abc", "abc"))
        self.assertFalse(fnmatch(True, "12356", "abc"))
        self.assertFalse(fnmatch(True, "<unknown>", "abc"))

        self.assertFalse(fnmatch(False, "abc", "abc"))
        self.assertTrue(fnmatch(False, "12356", "abc"))
        self.assertTrue(fnmatch(False, "<unknown>", "abc"))

    def test_filter_match_filename_joker(self):
        def fnmatch(filename, pattern):
            filter = mprofile.Filter(True, pattern)
            return filter._match_frame(filename, 0)

        # empty string
        self.assertFalse(fnmatch('abc', ''))
        self.assertFalse(fnmatch('', 'abc'))
        self.assertTrue(fnmatch('', ''))
        self.assertTrue(fnmatch('', '*'))

        # no *
        self.assertTrue(fnmatch('abc', 'abc'))
        self.assertFalse(fnmatch('abc', 'abcd'))
        self.assertFalse(fnmatch('abc', 'def'))

        # a*
        self.assertTrue(fnmatch('abc', 'a*'))
        self.assertTrue(fnmatch('abc', 'abc*'))
        self.assertFalse(fnmatch('abc', 'b*'))
        self.assertFalse(fnmatch('abc', 'abcd*'))

        # a*b
        self.assertTrue(fnmatch('abc', 'a*c'))
        self.assertTrue(fnmatch('abcdcx', 'a*cx'))
        self.assertFalse(fnmatch('abb', 'a*c'))
        self.assertFalse(fnmatch('abcdce', 'a*cx'))

        # a*b*c
        self.assertTrue(fnmatch('abcde', 'a*c*e'))
        self.assertTrue(fnmatch('abcbdefeg', 'a*bd*eg'))
        self.assertFalse(fnmatch('abcdd', 'a*c*e'))
        self.assertFalse(fnmatch('abcbdefef', 'a*bd*eg'))

        # replace .pyc suffix with .py
        self.assertTrue(fnmatch('a.pyc', 'a.py'))
        self.assertTrue(fnmatch('a.py', 'a.pyc'))

        if os.name == 'nt':
            # case insensitive
            self.assertTrue(fnmatch('aBC', 'ABc'))
            self.assertTrue(fnmatch('aBcDe', 'Ab*dE'))

            self.assertTrue(fnmatch('a.pyc', 'a.PY'))
            self.assertTrue(fnmatch('a.py', 'a.PYC'))
        else:
            # case sensitive
            self.assertFalse(fnmatch('aBC', 'ABc'))
            self.assertFalse(fnmatch('aBcDe', 'Ab*dE'))

            self.assertFalse(fnmatch('a.pyc', 'a.PY'))
            self.assertFalse(fnmatch('a.py', 'a.PYC'))

        if os.name == 'nt':
            # normalize alternate separator "/" to the standard separator "\"
            self.assertTrue(fnmatch(r'a/b', r'a\b'))
            self.assertTrue(fnmatch(r'a\b', r'a/b'))
            self.assertTrue(fnmatch(r'a/b\c', r'a\b/c'))
            self.assertTrue(fnmatch(r'a/b/c', r'a\b\c'))
        else:
            # there is no alternate separator
            self.assertFalse(fnmatch(r'a/b', r'a\b'))
            self.assertFalse(fnmatch(r'a\b', r'a/b'))
            self.assertFalse(fnmatch(r'a/b\c', r'a\b/c'))
            self.assertFalse(fnmatch(r'a/b/c', r'a\b\c'))

        self.assertTrue(fnmatch('a.pyo', 'a.py'))

    def test_filter_match_trace(self):
        t1 = (("test", "a.py", 1, 2), ("test", "b.py", 1, 3))
        t2 = (("test", "b.py", 1, 4), ("test", "b.py", 1, 5))
        t3 = (("test", "c.py", 1, 5), ("test", '<unknown>', 0, 0))
        unknown = (("test", '<unknown>', 0, 0),)

        f = mprofile.Filter(True, "b.py", all_frames=True)
        self.assertTrue(f._match_traceback(t1))
        self.assertTrue(f._match_traceback(t2))
        self.assertFalse(f._match_traceback(t3))
        self.assertFalse(f._match_traceback(unknown))

        f = mprofile.Filter(True, "b.py", all_frames=False)
        self.assertFalse(f._match_traceback(t1))
        self.assertTrue(f._match_traceback(t2))
        self.assertFalse(f._match_traceback(t3))
        self.assertFalse(f._match_traceback(unknown))

        f = mprofile.Filter(False, "b.py", all_frames=True)
        self.assertFalse(f._match_traceback(t1))
        self.assertFalse(f._match_traceback(t2))
        self.assertTrue(f._match_traceback(t3))
        self.assertTrue(f._match_traceback(unknown))

        f = mprofile.Filter(False, "b.py", all_frames=False)
        self.assertTrue(f._match_traceback(t1))
        self.assertFalse(f._match_traceback(t2))
        self.assertTrue(f._match_traceback(t3))
        self.assertTrue(f._match_traceback(unknown))

        f = mprofile.Filter(False, "<unknown>", all_frames=False)
        self.assertTrue(f._match_traceback(t1))
        self.assertTrue(f._match_traceback(t2))
        self.assertTrue(f._match_traceback(t3))
        self.assertFalse(f._match_traceback(unknown))

        f = mprofile.Filter(True, "<unknown>", all_frames=True)
        self.assertFalse(f._match_traceback(t1))
        self.assertFalse(f._match_traceback(t2))
        self.assertTrue(f._match_traceback(t3))
        self.assertTrue(f._match_traceback(unknown))

        f = mprofile.Filter(False, "<unknown>", all_frames=True)
        self.assertTrue(f._match_traceback(t1))
        self.assertTrue(f._match_traceback(t2))
        self.assertFalse(f._match_traceback(t3))
        self.assertFalse(f._match_traceback(unknown))


class TestCommandLine(unittest.TestCase):
    def test_env_var_disabled_by_default(self):
        # not tracing by default
        code = 'import mprofile; print(mprofile.is_tracing())'
        stdout = subprocess.check_output([sys.executable, "-c", code])
        stdout = stdout.rstrip()
        self.assertEqual(stdout, b'False')

    def test_env_var_enabled_at_startup(self):
        # tracing at startup
        code = 'import mprofile; print(mprofile.is_tracing())'
        stdout = subprocess.check_output([sys.executable, "-c", code], env={"MPROFILERATE": "1"})
        stdout = stdout.rstrip()
        self.assertEqual(stdout, b'True')

    def test_env_limit(self):
        # start and set the number of frames
        code = 'import mprofile; print(mprofile.get_traceback_limit())'
        stdout = subprocess.check_output([sys.executable, "-c", code],
                                         env={"MPROFILERATE": "1", "MPROFILEFRAMES": "10"})
        stdout = stdout.rstrip()
        self.assertEqual(stdout, b'10')

    def check_env_var_invalid(self, nframe):
        code = 'import mprofile; print(mprofile.is_tracing())'
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            subprocess.check_output([sys.executable, "-c", code],
                                    env={"MPROFILERATE": "1", "MPROFILEFRAMES": str(nframe)},
                                    stderr=subprocess.STDOUT)

        stdout = cm.exception.output
        if b'ValueError: the number of frames must be in range' in stdout:
            return
        if b'MPROFILEFRAMES: invalid number of frames' in stdout:
            return
        self.fail("unexpected output: {}".format(stdout))


    def test_env_var_invalid(self):
        for nframe in INVALID_NFRAME:
            with self.subTest(nframe=nframe):
                self.check_env_var_invalid(nframe)

    @unittest.skipIf(_testcapi is None, 'need _testcapi')
    def test_pymem_alloc0(self):
        # Issue #21639: Check that PyMem_Malloc(0) with mprofile enabled
        # does not crash.
        code = 'import _testcapi; _testcapi.test_pymem_alloc0(); 1'
        stdout = subprocess.check_output([sys.executable, "-c", code],
                                         env={"MPROFILERATE": "1"})
