/*
 * Lightbridge
 *
 * © 2017 paulsnar <paulsnar@paulsnar.lv>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define DEFAULT_STRAND_LENGTH 160

#define BYTES_LATCH_PRE 5
#define BYTES_LATCH_POST 5
#define BYTES_LATCH (BYTES_LATCH_PRE + BYTES_LATCH_POST)

#define BYTES_RGB(pos) ((pos) * 3)

#define BYTES_TOTAL(pos) (BYTES_LATCH + BYTES_RGB(pos))

#include <time.h>
#define SEC_TO_NSEC 1000000000
#define DEFAULT_SLEEP_SEC 0
#define DEFAULT_SLEEP_NSEC 15e7

#define MAX_LED_VAL 0x7F
