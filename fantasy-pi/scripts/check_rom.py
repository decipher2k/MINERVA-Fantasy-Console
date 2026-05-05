# Copyright 2026 Dennis Michael Heine
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

import struct
import sys

with open(sys.argv[1], 'rb') as f:
    data = f.read()

print(f"ROM size: {len(data)} bytes")
print(f"Magic: {data[0:4]}")
print(f"Version: {struct.unpack('I', data[4:8])[0]:08X}")
print(f"Code size: {struct.unpack('I', data[8:12])[0]}")
print(f"Entry point: {struct.unpack('I', data[12:16])[0]}")
print(f"Asset count: {struct.unpack('I', data[16:20])[0]}")
print(f"First instruction bytes: {data[256:260].hex()}")
