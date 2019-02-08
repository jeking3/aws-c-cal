/*
 * Copyright 2010-2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <aws/cal/hmac.h>

#include <CommonCrypto/CommonHMAC.h>

static void s_destroy(struct aws_hmac *hmac);
static int s_update(struct aws_hmac *hmac, struct aws_byte_cursor *to_hmac);
static int s_finalize(struct aws_hmac *hmac, struct aws_byte_buf *output);

static struct aws_hmac_vtable s_sha256_hmac_vtable = {
    .destroy = s_destroy,
    .update = s_update,
    .finalize = s_finalize,
    .alg_name = "SHA256 HMAC",
    .provider = "CommonCrypto",
};

struct cc_hmac {
  struct aws_hmac hmac;
  CCHmacContext cc_hmac_ctx;
};

struct aws_hmac *aws_sha256_hmac_default_new(struct aws_allocator *allocator,
                                             struct aws_byte_cursor *secret) {
  assert(secret->ptr);

  struct cc_hmac *cc_hmac = aws_mem_acquire(allocator, sizeof(struct cc_hmac));

  if (!cc_hmac) {
    return NULL;
  }

  cc_hmac->hmac.allocator = allocator;
  cc_hmac->hmac.vtable = &s_sha256_hmac_vtable;
  cc_hmac->hmac.impl = cc_hmac;
  cc_hmac->hmac.digest_size = AWS_SHA256_HMAC_LEN;

  CCHmacInit(&cc_hmac->cc_hmac_ctx, kCCHmacAlgSHA256, secret->ptr,
             (CC_LONG)secret->len);

  return &cc_hmac->hmac;
}

static void s_destroy(struct aws_hmac *hmac) {
  struct cc_hmac *ctx = hmac->impl;
  aws_mem_release(hmac->allocator, ctx);
}

static int s_update(struct aws_hmac *hmac, struct aws_byte_cursor *to_hmac) {
  struct cc_hmac *ctx = hmac->impl;

  CCHmacUpdate(&ctx->cc_hmac_ctx, to_hmac->ptr, (CC_LONG)to_hmac->len);
  return AWS_OP_SUCCESS;
}

static int s_finalize(struct aws_hmac *hmac, struct aws_byte_buf *output) {
  struct cc_hmac *ctx = hmac->impl;

  size_t buffer_len = output->capacity - output->len;

  if (buffer_len < hmac->digest_size) {
    return aws_raise_error(AWS_ERROR_SHORT_BUFFER);
  }

  CCHmacFinal(&ctx->cc_hmac_ctx, output->buffer + output->len);
  output->len += buffer_len;
  return AWS_OP_SUCCESS;
}
