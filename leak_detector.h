// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_

namespace leak_detector {

// The top level leak detector is a singleton instance. Implement it as a
// namespace with init/shutdown functions rather than as a class with static
// member functions.

void Initialize();
void Shutdown();

bool IsInitialized();

}  // namespace leak_detector

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_
