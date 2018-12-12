# Copyright 2010 Google Inc.
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

{
  'variables': {
    'pagespeed_root': '../..',
  },
  'targets': [
    {
      'target_name': 'pagespeed_jsminify',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'js_minify.cc',
      ],
      # TODO: we should fix the code so this is not needed.
      'msvs_disabled_warnings': [ 4018 ],
      'include_dirs': [
        '<(pagespeed_root)',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'pagespeed_javascript_gperf',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(pagespeed_root)',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
      ],
    },
    {
      'target_name': 'pagespeed_javascript_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'js',
        'instaweb_root':  '<(DEPTH)/pagespeed',
      },
      'sources': [
        'js_keywords.gperf',
      ],
      'includes': [
        '../../third_party/mod_pagespeed/src/net/instaweb/gperf.gypi',
      ]
    },
  ],
}
