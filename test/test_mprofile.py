import mprofile
import threading
import time
import unittest


def alloc_objects():
    l = []
    for i in range(1000):
        l.append(object())
        time.sleep(0.001)
    return l


class TestMProfiler(unittest.TestCase):
    def test_profile_one(self):
        mprofile.start()
        alloc_obj = object()
        snap = mprofile.take_snapshot()
        mprofile.stop()

        self.assertEqual(len(snap.traces), 1)
        frame = snap.traces[0].traceback[-1]
        self.assertTrue(frame.filename.endswith("test_mprofile.py"))
        self.assertEqual(frame.name, "test_profile_one")
        self.assertEqual(frame.firstlineno, 16)
        self.assertEqual(frame.lineno, 18)

    def test_profile(self):
        mprofile.start()
        large_alloc = [object() for _ in range(12)]
        snap = mprofile.take_snapshot()
        mprofile.stop()

        # Snapshot references should remain valid after stopping profiler.
        found_it = False
        for trace in snap.traces:
            for frame in trace.traceback:
                if frame.name == "test_profile":
                    found_it = True
        self.assertTrue(found_it)

    def test_profile_concurrent(self):
        mprofile.start()
        t1 = threading.Thread(target=alloc_objects)
        t1.start()
        time.sleep(0.001)
        t2 = threading.Thread(target=alloc_objects)
        t2.start()

        time.sleep(0.001)
        snap = mprofile.take_snapshot()
        t1.join()
        mprofile.stop()
        t2.join()

    def test_profile_1frame(self):
        mprofile.start(max_frames=1)
        alloc_obj = object()
        snap = mprofile.take_snapshot()
        mprofile.stop()

        self.assertEqual(len(snap.traces), 1)
        self.assertEqual(len(snap.traces[0].traceback), 1)

    def test_profile_sampler(self):
        n = 100000
        mprofile.start(sample_rate=1024)
        alloc_obj = [object() for _ in range(n)]
        snap = mprofile.take_snapshot()
        mprofile.stop()

        self.assertGreaterEqual(len(snap.traces), 0)
        self.assertLessEqual(len(snap.traces), n)
        self.assertGreaterEqual(len(snap.traces[0].traceback), 2)

        stats = snap.statistics("traceback")
        self.assertGreaterEqual(len(stats), 1)
        self.assertGreaterEqual(stats[0].count, len(snap.traces))
        self.assertGreaterEqual(stats[0].size, sum(t.size for t in snap.traces))

if __name__ == '__main__':
    unittest.main()
