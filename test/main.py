import sys
import unittest

if __name__ == '__main__':
    tests = unittest.defaultTestLoader.discover("test", pattern="test_*.py")
    runner = unittest.runner.TextTestRunner()
    result = runner.run(tests)
    sys.exit(not result.wasSuccessful())
