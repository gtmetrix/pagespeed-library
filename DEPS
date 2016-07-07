# Copyright 2012 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

vars = {
  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  "src/testing":
    Var("chromium_git") + "/chromium/src/testing@ba768d0e259684b5a1912d1330b7fd511df4f647",

  "src/third_party/chromium/src/build":
    Var("chromium_git") + "/chromium/src/build@4e3e69ad445b72e75b32c623003fd8843d6f17af",

  "src/third_party/chromium/src/base":
    Var("chromium_git") + "/chromium/src/base@f86bdec26aea6b2b056c89a83f5a4fafdcb96e73",

  "src/third_party/domain_registry_provider":
    "https://github.com/pagespeed/domain-registry-provider.git@e9b72eaef413335eb054a5982277cb2e42eaead7",

  "src/third_party/libharu/libharu":
    "https://github.com/libharu/libharu.git@7f9ff9a349061e36f3b301409c0e4ed91de19bb7",

  "src/third_party/modp_b64":
    Var("chromium_git") + "/chromium/src/third_party/modp_b64@42c1fe9d5a2d17370edf2debb365f6660e2aef3a",

  "src/third_party/protobuf":
    "https://src.chromium.org/svn/trunk/src/third_party/protobuf@161115",

  "src/third_party/protobuf/java":
    "https://github.com/google/protobuf/tags/v2.4.1/java/src/main/java",

  "src/third_party/protobuf/java/descriptor":
    File("https://github.com/google/protobuf/tags/v2.4.1/src/google/protobuf/descriptor.proto"),

  "src/tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang@b363ee90f841578d66b4c4f6481f0107bcf800e9",

  "src/googleurl":
    Var("chromium_git") + "/external/google-url@eb8b21b16f6e39375bf4048567d4844027e47186",

  "src/third_party/mod_pagespeed":
    "https://github.com/pagespeed/mod_pagespeed.git@65c07fc394d56ea7d74f6b65a7b3d12fb4ab1b6c",

  "src/testing/gmock":
    "https://github.com/google/googlemock.git@79a367eb217fcd47e2beaf8c0f87fe6d5926f739",

  "src/testing/gtest":
    "https://github.com/google/googletest.git@2147489625ea8071ca560462f19b1ceb8940a229",

  "src/tools/grit":
    Var("chromium_git") + "/external/grit-i18n@83717e82a9b5e0c629ff4f1078d50503ffd2ae75",

  "src/tools/gyp":
    Var("chromium_git") + "/external/gyp@523297f43e0c96a84e53306f8fddebeb483b27f1",
}


deps_os = {
  "win": {
#    "src/third_party/cygwin": From(Var("chromium_deps_root")),
#    "src/third_party/python_26": From(Var("chromium_deps_root")),
  },
  "mac": {
  },
  "unix": {
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "testing",
]


hooks = [
   {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang,
    # which takes ~20s, but clang speeds up builds by more than 20s.
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium"],
  },
]
