# Copyright 2011 Google Inc.
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
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'pagespeed_json_dom',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(pagespeed_root)/pagespeed/core/core.gyp:pagespeed_core',
      ],
      'sources': [
        'json_dom.cc',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
        '<(pagespeed_root)/pagespeed/core/core.gyp:pagespeed_core',
      ],
    },
    {
      'target_name': 'pagespeed_resource_coordinate_finder',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(pagespeed_root)/pagespeed/core/core.gyp:pagespeed_core',
        # We inherit this dependency via an export_dependent_settings
        # from pagespeed_core, however gyp does not currently
        # propagate hard_dependency to export_dependent_settings, so
        # we must declare this dependency directly here. See
        # http://code.google.com/p/gyp/issues/detail?id=248 for the bug
        # that tracks this issue.
        '<(pagespeed_root)/pagespeed/proto/proto_gen.gyp:pagespeed_genproto',
      ],
      'sources': [
        'resource_coordinate_finder.cc',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
        '<(pagespeed_root)/pagespeed/core/core.gyp:pagespeed_core',
      ],
    },
  ],
}
