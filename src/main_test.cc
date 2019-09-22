// Copyright 2019 Timothy Palpant
//

#include <Python.h>
#include "gtest/gtest.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  Py_Initialize();
  PyEval_InitThreads();

  int rc = RUN_ALL_TESTS();

  Py_Finalize();
  return rc;
}
