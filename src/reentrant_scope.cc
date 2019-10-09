// Copyright 2019 Timothy Palpant

#include "reentrant_scope.h"

thread_local bool ReentrantScope::in_malloc_ = false;
