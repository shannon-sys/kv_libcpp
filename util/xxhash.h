// Copyright (c) 2018 Shannon Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#pragma once

#if defined (__cplusplus)
namespace shannon {
#endif

typedef enum { XXH_OK=0, XXH_ERROR } XXH_errorcode;

unsigned int XXH32 (const void* input, int len, unsigned int seed);

//****************************
// Advanced Hash Functions
//****************************

void*         XXH32_init   (unsigned int seed);
XXH_errorcode XXH32_update (void* state, const void* input, int len);
unsigned int  XXH32_digest (void* state);

int           XXH32_sizeofState();
XXH_errorcode XXH32_resetState(void* state, unsigned int seed);

#define       XXH32_SIZEOFSTATE 48
typedef struct { long long ll[(XXH32_SIZEOFSTATE+(sizeof(long long)-1))/sizeof(long long)]; } XXH32_stateSpace_t;

unsigned int XXH32_intermediateDigest (void* state);

#define XXH32_feed   XXH32_update
#define XXH32_result XXH32_digest
#define XXH32_getIntermediateResult XXH32_intermediateDigest

#if defined (__cplusplus)
}  // namespace shannon
#endif
