/* rhash.c - implementation of LibRHash library calls
 *
 * Copyright (c) 2008, Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE  INCLUDING ALL IMPLIED WARRANTIES OF  MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT,  OR CONSEQUENTIAL DAMAGES  OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,  NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF  OR IN CONNECTION  WITH THE USE  OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* modifier for Windows DLL */
#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(RHASH_EXPORTS)
# define RHASH_API __declspec(dllexport)
#endif

/* macros for large file support, must be defined before any include file */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include "rhash.h"
#include "algorithms.h"
#include "byte_order.h"
#include "hex.h"
#include "plug_openssl.h"
#include "torrent.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
# include <io.h>
#endif

#define STATE_ACTIVE  0xb01dbabe
#define STATE_STOPPED 0xdeadbeef
#define STATE_DELETED 0xdecea5ed
#define IS_BAD_STATE(s) ((s) != STATE_ACTIVE && (s) != STATE_STOPPED)
#define RCTX_AUTO_FINAL 0x1
#define RCTX_FINALIZED  0x2
#define RCTX_FINALIZED_MASK (RCTX_AUTO_FINAL | RCTX_FINALIZED)
#define RHPR_FORMAT (RHPR_RAW | RHPR_HEX | RHPR_BASE32 | RHPR_BASE64)
#define RHPR_MODIFIER (RHPR_UPPERCASE | RHPR_URLENCODE | RHPR_REVERSE)

#define IS_VALID_EXTENDED_HASH_ID(id) (GET_EXTENDED_HASH_ID_INDEX(id) < RHASH_HASH_COUNT)
#define HAS_ZERO_OR_ONE_BIT(id) (((id) & ((id) - 1)) == 0)
#define IS_VALID_HASH_MASK(bitmask) ((bitmask) != 0 && ((bitmask) & ~RHASH_LOW_HASHES_MASK) == 0)
#define IS_VALID_HASH_ID(id) (IS_EXTENDED_HASH_ID(id) ? IS_VALID_EXTENDED_HASH_ID(id) : \
	IS_VALID_HASH_MASK(id) && HAS_ZERO_OR_ONE_BIT(id))
#define EXTENDED_HASH_ID_FROM_BIT64(bit) ((unsigned)RHASH_EXTENDED_BIT ^ rhash_ctz64(bit))

/* each hash function context must be aligned to DEFAULT_ALIGNMENT bytes */
#define GET_CTX_ALIGNED(size) ALIGN_SIZE_BY((size), DEFAULT_ALIGNMENT)
#define GET_EXPORT_ALIGNED(size) ALIGN_SIZE_BY((size), 8)

RHASH_API void rhash_library_init(void)
{
	rhash_init_algorithms();
#ifdef USE_OPENSSL
	rhash_plug_openssl();
#endif
}

RHASH_API int rhash_count(void)
{
	return rhash_info_size;
}

/* LOW-LEVEL LIBRHASH INTERFACE */

/**
 * Allocate and initialize RHash context for calculating a single or multiple hash functions.
 * The context after usage must be freed by calling rhash_free().
 *
 * @param count the size of the hash_ids array, the count must be greater than zero
 * @param hash_ids array of identifiers of hash functions. Each element must
 *        be an identifier of one hash function
 * @param need_init initialize context for each hash function
 * @return initialized rhash context, NULL on fail with error code stored in errno
 */
static rhash_context_ext* rhash_alloc_multi(size_t count, const unsigned hash_ids[], int need_init)
{
	rhash_context_ext* rctx = NULL; /* allocated rhash context */
	const size_t header_size = GET_CTX_ALIGNED(sizeof(rhash_context_ext) + sizeof(rhash_vector_item) * count);
	size_t ctx_size_sum = 0;   /* size of hash contexts to store in rctx */
	size_t i;
	char* phash_ctx;
	uint64_t hash_bitmask = 0;

	if (count < 1) {
		errno = EINVAL;
		return NULL;
	}
	if (count == 1 && hash_ids[0] == RHASH_ALL_HASHES)
		hash_ids = rhash_get_all_hash_ids(hash_ids[0], &count);
	for (i = 0; i < count; i++) {
		const rhash_hash_info* info = rhash_hash_info_by_id(hash_ids[i]);
		if (!info) {
			errno = EINVAL;
			return NULL;
		}
		assert(IS_EXTENDED_HASH_ID(info->info->hash_id));
		assert(IS_VALID_EXTENDED_HASH_ID(info->info->hash_id));
		hash_bitmask |= I64(1) << GET_EXTENDED_HASH_ID_INDEX(info->info->hash_id);

		/* align context sizes and sum up */
		ctx_size_sum += GET_CTX_ALIGNED(info->context_size);
	}

	/* allocate rhash context with enough memory to store contexts of all selected hash functions */
	rctx = (rhash_context_ext*)rhash_aligned_alloc(DEFAULT_ALIGNMENT, header_size + ctx_size_sum);
	if (rctx == NULL)
		return NULL;

	/* initialize common fields of the rhash context */
	memset(rctx, 0, header_size);
	rctx->rc.hash_mask = hash_bitmask;
	rctx->flags = RCTX_AUTO_FINAL; /* turn on auto-final by default */
	rctx->state = STATE_ACTIVE;
	rctx->hash_vector_size = (unsigned)count;

	/* calculate aligned pointer >= (&rctx->vector[count]) */
	phash_ctx = (char*)rctx + header_size;
	assert(phash_ctx >= (char*)&rctx->vector[count]);
	assert(phash_ctx < ((char*)&rctx->vector[count] + DEFAULT_ALIGNMENT));

	for (i = 0; i < count; i++) {
		const rhash_hash_info* info = rhash_hash_info_by_id(hash_ids[i]);
		assert(info != NULL);
		assert(info->context_size > 0);
		assert(info->init != NULL);
		assert(IS_PTR_ALIGNED_BY(phash_ctx, DEFAULT_ALIGNMENT)); /* hash context is aligned */

		rctx->vector[i].hash_info = info;
		rctx->vector[i].context = phash_ctx;

		/* BTIH initialization is a bit complicated, so store the context pointer for later usage */
		if (info->info->hash_id == EXTENDED_BTIH)
			rctx->bt_ctx = phash_ctx;
		phash_ctx += GET_CTX_ALIGNED(info->context_size);

		/* initialize the i-th hash context */
		if (need_init)
			info->init(rctx->vector[i].context);
	}
	return rctx;
}

RHASH_API rhash rhash_init_multi(size_t count, const unsigned hash_ids[])
{
	rhash_context_ext* ectx = rhash_alloc_multi(count, hash_ids, 1);
	return &ectx->rc; /* return initialized rhash context */
}

RHASH_API rhash rhash_init(unsigned hash_id)
{
	if (hash_id == RHASH_ALL_HASHES || hash_id == RHASH_LOW_HASHES_MASK) {
		size_t count;
		const unsigned* hash_ids = rhash_get_all_hash_ids(hash_id, &count);
		return rhash_init_multi(count, hash_ids);
	}
	if (!IS_EXTENDED_HASH_ID(hash_id) && !IS_VALID_HASH_MASK(hash_id)) {
		errno = EINVAL;
		return NULL;
	}
	if (IS_EXTENDED_HASH_ID(hash_id) || HAS_ZERO_OR_ONE_BIT(hash_id)) {
		return rhash_init_multi(1, &hash_id);
	} else {
		/* handle the deprecated case, when hash_id is a bitwise union of several hash function identifiers */
		size_t count;
		unsigned hash_ids[32];
		unsigned id = hash_id & -hash_id; /* get the trailing bit */
		for (count = 0; id <= hash_id; id = id << 1) {
			assert(id != 0);
			if (hash_id & id)
				hash_ids[count++] = id;
		}
		assert(count > 1);
		return rhash_init_multi(count, hash_ids);
	}
}

void rhash_free(rhash ctx)
{
	rhash_context_ext* const ectx = (rhash_context_ext*)ctx;
	unsigned i;

	if (ctx == 0) return;
	ectx->state = STATE_DELETED; /* mark memory block as being removed */

	/* clean the hash functions, which require additional clean up */
	for (i = 0; i < ectx->hash_vector_size; i++) {
		const struct rhash_hash_info* info = ectx->vector[i].hash_info;
		if (info->cleanup != 0) {
			info->cleanup(ectx->vector[i].context);
		}
	}
	rhash_aligned_free(ectx);
}

RHASH_API void rhash_reset(rhash ctx)
{
	rhash_context_ext* const ectx = (rhash_context_ext*)ctx;
	unsigned i;

	assert(ectx->hash_vector_size > 0);
	assert(ectx->hash_vector_size <= RHASH_HASH_COUNT);

	/* re-initialize every hash in a loop */
	for (i = 0; i < ectx->hash_vector_size; i++) {
		const struct rhash_hash_info* info = ectx->vector[i].hash_info;
		if (info->cleanup != 0) {
			info->cleanup(ectx->vector[i].context);
		}

		assert(info->init != NULL);
		info->init(ectx->vector[i].context);
	}
	ectx->flags &= ~RCTX_FINALIZED; /* clear finalized state */
	ectx->state = STATE_ACTIVE; /* re-activate the structure */
	ctx->msg_size = 0;
}

RHASH_API int rhash_update(rhash ctx, const void* message, size_t length)
{
	rhash_context_ext* const ectx = (rhash_context_ext*)ctx;
	unsigned i;

	assert(ectx->hash_vector_size <= RHASH_HASH_COUNT);
	if (ectx->state != STATE_ACTIVE) return 0; /* do nothing if canceled */

	ctx->msg_size += length;

	/* call update method for every algorithm */
	for (i = 0; i < ectx->hash_vector_size; i++) {
		const struct rhash_hash_info* info = ectx->vector[i].hash_info;
		assert(info->update != 0);
		info->update(ectx->vector[i].context, message, length);
	}
	return 0; /* no error processing at the moment */
}

RHASH_API int rhash_final(rhash ctx, unsigned char* first_result)
{
	unsigned i = 0;
	unsigned char buffer[130];
	unsigned char* out = (first_result ? first_result : buffer);
	rhash_context_ext* const ectx = (rhash_context_ext*)ctx;
	assert(ectx->hash_vector_size <= RHASH_HASH_COUNT);

	/* skip final call if already finalized and auto-final is on */
	if ((ectx->flags & RCTX_FINALIZED_MASK) ==
		(RCTX_AUTO_FINAL | RCTX_FINALIZED)) return 0;

	/* call final method for every algorithm */
	for (i = 0; i < ectx->hash_vector_size; i++) {
		const struct rhash_hash_info* info = ectx->vector[i].hash_info;
		assert(info->final != 0);
		assert(info->info->digest_size < sizeof(buffer));
		info->final(ectx->vector[i].context, out);
		out = buffer;
	}
	ectx->flags |= RCTX_FINALIZED;
	return 0; /* no error processing at the moment */
}

/**
 * Header block for rhash context import/export.
 */
typedef struct export_header
{
	uint32_t state;
	uint16_t hash_vector_size;
	uint16_t flags;
	uint64_t msg_size;
} export_header;

/**
 * Process export error. Returns 0 and set errno to EINVAL.
 *
 * @return NULL
 */
static size_t export_error_einval(void)
{
	errno = EINVAL;
	return 0;
}

/**
 * Process import error. Returns NULL and set errno to EINVAL.
 *
 * @return NULL
 */
static rhash import_error_einval(void)
{
	errno = EINVAL;
	return NULL;
}

RHASH_API size_t rhash_export(rhash ctx, void* out, size_t size)
{
#if !defined(NO_IMPORT_EXPORT)
	size_t export_size;
	size_t i;
	rhash_context_ext* const ectx = (rhash_context_ext*)ctx;
	export_header* header = (export_header*)out;
	unsigned* hash_ids = NULL;
	if (!ctx || (out && size < sizeof(export_header)) || IS_BAD_STATE(ectx->state))
		return export_error_einval();
	export_size = sizeof(export_header) + sizeof(unsigned) * ectx->hash_vector_size;
	if (out != NULL) {
		memset(out, 0, size);
		header->state = ectx->state;
		header->hash_vector_size = (uint16_t)(ectx->hash_vector_size);
		header->flags = (uint16_t)(ectx->flags);
		header->msg_size = ctx->msg_size;
		hash_ids = (unsigned*)(void*)(header + 1);
	}
	for (i = 0; i < ectx->hash_vector_size; i++) {
		void* src_context = ectx->vector[i].context;
		const struct rhash_hash_info* hash_info = ectx->vector[i].hash_info;
		unsigned is_special = (hash_info->info->flags & F_SPCEXP);
		size_t item_size;
		if (out != NULL) {
			if (size <= export_size)
				return export_error_einval();
			hash_ids[i] = hash_info->info->hash_id;
			if (is_special) {
				char* dst_item;
				size_t left_size;
				export_size = GET_EXPORT_ALIGNED(export_size);
				dst_item = (char*)out + export_size;
				left_size = size - export_size;
				item_size = rhash_export_alg(hash_info->info->hash_id,
					src_context, dst_item, left_size);
				if (!item_size)
					return export_error_einval();
			} else {
				char* dst_item = (char*)out + export_size;
				item_size = hash_info->context_size;
				if (size < (export_size + item_size))
					return export_error_einval();
				memcpy(dst_item, src_context, item_size);
			}
		} else {
			if (is_special) {
				export_size = GET_EXPORT_ALIGNED(export_size);
				item_size = rhash_export_alg(
					hash_info->info->hash_id, src_context, NULL, 0);
			} else
				item_size = hash_info->context_size;
		}
		export_size += item_size;
	}
	if (export_size < size)
		return export_error_einval();
	return export_size;
#else
	return export_error_einval();
#endif /* !defined(NO_IMPORT_EXPORT) */
}

RHASH_API rhash rhash_import(const void* in, size_t size)
{
#if !defined(NO_IMPORT_EXPORT)
	const export_header* header = (const export_header*)in;
	size_t i;
	size_t imported_size;
	const unsigned* hash_ids;
	const char* src_item;
	rhash_context_ext* ectx;
	if (!header || IS_BAD_STATE(header->state) || size < sizeof(export_header))
		return import_error_einval();
	imported_size = sizeof(export_header) + sizeof(unsigned) * header->hash_vector_size;
	if (!header->hash_vector_size || size < imported_size)
		return import_error_einval();
	hash_ids = (const unsigned*)(const void*)(header + 1);
	ectx = (rhash_context_ext*)rhash_alloc_multi(header->hash_vector_size, hash_ids, 0);
	if (!ectx)
		return NULL; /* errno must be set by the previous function */
	ectx->state = header->state;
	ectx->hash_vector_size = header->hash_vector_size;
	ectx->flags = header->flags;
	ectx->rc.msg_size = header->msg_size;
	for (i = 0; i < ectx->hash_vector_size; i++) {
		void* dst_context = ectx->vector[i].context;
		const struct rhash_hash_info* hash_info = ectx->vector[i].hash_info;
		unsigned is_special = (hash_info->info->flags & F_SPCEXP);
		size_t item_size;

		if (is_special) {
			size_t left_size;
			imported_size = GET_EXPORT_ALIGNED(imported_size);
			src_item = (const char*)in + imported_size;
			left_size = size - imported_size;
			assert(size >= imported_size);
			item_size = rhash_import_alg(hash_ids[i], dst_context, src_item, left_size);
			imported_size += item_size;
			if (!item_size || size < imported_size) {
				ectx->hash_vector_size = (unsigned)i + 1; /* clean only initialized contextes */
				rhash_free(&ectx->rc);
				return import_error_einval();
			}
		} else {
			src_item = (const char*)in + imported_size;
			item_size = hash_info->context_size;
			imported_size += item_size;
			if (size < imported_size) {
				ectx->hash_vector_size = (unsigned)i + 1;
				rhash_free(&ectx->rc);
				return import_error_einval();
			}
			memcpy(dst_context, src_item, item_size);
		}
	}
	return &ectx->rc;
#else
	return import_error_einval();
#endif /* !defined(NO_IMPORT_EXPORT) */
}

/**
 * Validate and convert hash_id to EXTENDED_HASH_ID.
 *
 * @param hash_id id of one hash algorithm
 * @return hash_id in its extended form if it is valid, 0 otherwise
 */
static unsigned convert_to_extended_hash_id(unsigned hash_id)
{
	if (IS_EXTENDED_HASH_ID(hash_id))
	{
		if (IS_VALID_EXTENDED_HASH_ID(hash_id))
			return hash_id;
	}
	else if (IS_VALID_HASH_MASK(hash_id) && HAS_ZERO_OR_ONE_BIT(hash_id))
	{
		return RHASH_EXTENDED_BIT ^ (rhash_ctz(hash_id));
	}
	return 0; /* invalid hash_id detected */
}

/**
 * Find rhash_vector_item by the given hash_id in rhash context.
 * The context must include the hash algorithm corresponding to the hash_id.
 *
 * @param ectx extended rhash context
 * @param hash_id id of hash algorithm, or zero for the first hash algorithm in the rhash context
 * @return item of the rhash context if the hash algorithm has been found, NULL otherwise
 */
static rhash_vector_item* rhash_get_info(rhash_context_ext* ectx, unsigned hash_id)
{
	assert(ectx);
	assert(ectx->hash_vector_size > 0 && ectx->hash_vector_size <= RHASH_HASH_COUNT);

	if (hash_id == 0) {
		return &ectx->vector[0]; /* get the first hash */
	} else {
		unsigned i;
		hash_id = convert_to_extended_hash_id(hash_id);
		if (!hash_id)
			return NULL;
		for (i = 0; i < ectx->hash_vector_size; i++) {
			rhash_vector_item* item = &ectx->vector[i];
			assert(item->hash_info != NULL);
			assert(item->hash_info->info != NULL);
			if (item->hash_info->info->hash_id == hash_id)
				return item;
		}
	}
	return NULL; /* hash_id not found */
}

/**
 * Store message digest from the given rhash context item.
 *
 * @param item rhash context item, containing a message digest
 * @param result buffer to put the message digest into
 */
static void rhash_put_digest(rhash_vector_item* item, unsigned char* result)
{
	const struct rhash_hash_info* info = item->hash_info;
	unsigned char* digest = ((unsigned char*)item->context + info->digest_diff);

	if (info->info->flags & F_SWAP32) {
		assert((info->info->digest_size & 3) == 0);
		/* NB: the next call is correct only for multiple of 4 byte size */
		rhash_swap_copy_str_to_u32(result, 0, digest, info->info->digest_size);
	} else if (info->info->flags & F_SWAP64) {
		rhash_swap_copy_u64_to_str(result, digest, info->info->digest_size);
	} else {
		memcpy(result, digest, info->info->digest_size);
	}
}

RHASH_API void rhash_set_callback(rhash ctx, rhash_callback_t callback, void* callback_data)
{
	((rhash_context_ext*)ctx)->callback = callback;
	((rhash_context_ext*)ctx)->callback_data = callback_data;
}

/* HIGH-LEVEL LIBRHASH INTERFACE */

RHASH_API int rhash_msg(unsigned hash_id, const void* message, size_t length, unsigned char* result)
{
	rhash ctx;
	ctx = rhash_init(hash_id);
	if (ctx == NULL) return -1;
	rhash_update(ctx, message, length);
	rhash_final(ctx, result);
	rhash_free(ctx);
	return 0;
}

/**
 * Universal file I/O context for buffered file reading.
 */
struct file_update_context {
	union {
		FILE* file_fd; /* Standard C file stream pointer (for fopen/fread) */
		int int_fd;    /* POSIX file descriptor (for open/read) */
	};
	unsigned char* buffer; /* Data buffer for read operations */
	size_t buffer_size;    /* Size of the data buffer */
};

#if defined(_WIN32)
/* For Windows define ssize_t, which is Posix, but not in standard C */
# define ssize_t intptr_t
# define READ_SIZE_TYPE unsigned
#else
# define READ_SIZE_TYPE size_t
#endif

/**
 * Read data from a C file stream into the context buffer using fread().
 *
 * @param fctx file context containing a file descriptor and buffer
 * @param data_size number of bytes to read
 * @return number of bytes read on success, -1 on fail with error code stored in errno
 */
static ssize_t read_file_fd_impl(struct file_update_context *fctx, size_t data_size)
{
	size_t read_size;
	if (feof(fctx->file_fd))
		return 0;
	assert(data_size <= fctx->buffer_size);
	read_size = fread(fctx->buffer, 1, data_size, fctx->file_fd);
	return (ferror(fctx->file_fd) ? -1 : (ssize_t)read_size);
}

/**
 * Read data from a POSIX file descriptor into the context buffer using read().
 *
 * @param fctx file context containing a file descriptor and buffer
 * @param data_size number of bytes to read
 * @return number of bytes read on success, -1 on fail with error code stored in errno
 */
static ssize_t read_int_fd_impl(struct file_update_context *fctx, size_t data_size)
{
	assert(data_size <= fctx->buffer_size);
	return read(fctx->int_fd, fctx->buffer, (READ_SIZE_TYPE)data_size);
}

/**
 * File read operation callback signature.
 *
 * @param fctx file context with configured buffer and file handle
 * @param data_size requested read size
 * @return bytes read (0 = EOF), or -1 on error (must set errno)
 */
typedef ssize_t (*read_file_func)(struct file_update_context *fctx, size_t data_size);

/**
 * Internal implementation for hashing file/stream data.
 * Used by rhash_update_fd() and rhash_file_update().
 *
 * @param ectx extended rhash context (must be initialized)
 * @param fctx configured file context (buffer must be pre-allocated)
 * @param read_func callback for reading data
 * @param data_size maximum bytes to hash (RHASH_MAX_FILE_SIZE for entire file)
 * @return 0 on success, -1 on fail with error code stored in errno
 */
static int rhash_file_update_impl(
	struct rhash_context_ext* const ectx,
	struct file_update_context* const fctx,
	read_file_func read_func,
	unsigned long long data_size)
{
	const size_t buffer_size = 256 * 1024;
	size_t read_size = buffer_size;
	ssize_t length = 0;
	if (ectx == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (ectx->state != STATE_ACTIVE)
		return 0; /* do nothing if canceled */
	fctx->buffer_size = buffer_size;
	fctx->buffer = (unsigned char*)rhash_aligned_alloc(DEFAULT_ALIGNMENT, buffer_size);
	if (!fctx->buffer) {
		return -1; /* errno is set to ENOMEM according to UNIX 98 */
	}
	while (data_size > (size_t)length) {
		data_size -= (size_t)length;
		if (data_size < read_size)
			read_size = (size_t)data_size;
		length = read_func(fctx, read_size);
		if (length <= 0 || ectx->state != STATE_ACTIVE)
			break;
		rhash_update(&ectx->rc, fctx->buffer, (size_t)length);
		if (ectx->callback) {
			((rhash_callback_t)ectx->callback)(ectx->callback_data, ectx->rc.msg_size);
		}
	}
	rhash_aligned_free(fctx->buffer);
	return (length < 0 ? -1 : 0);
}

RHASH_API int rhash_update_fd(rhash ctx, int fd, unsigned long long data_size)
{
	struct file_update_context fctx;
	memset(&fctx, 0, sizeof(fctx));
	fctx.int_fd = fd;
	return rhash_file_update_impl((rhash_context_ext*)ctx,
		&fctx, read_int_fd_impl, data_size);
}

RHASH_API int rhash_file_update(rhash ctx, FILE* fd)
{
	struct file_update_context fctx;
	memset(&fctx, 0, sizeof(fctx));
	fctx.file_fd = fd;
	return rhash_file_update_impl((rhash_context_ext*)ctx,
		&fctx, read_file_fd_impl, RHASH_MAX_FILE_SIZE);
}

#ifdef _WIN32
# define FOPEN_MODE "rbS"
#else
# define FOPEN_MODE "rb"
#endif

RHASH_API int rhash_file(unsigned hash_id, const char* filepath, unsigned char* result)
{
	FILE* fd;
	rhash ctx;
	int res;

	ctx = rhash_init(hash_id);
	if (!ctx) {
		return -1;
	}
	fd = fopen(filepath, FOPEN_MODE);
	if (!fd) {
		rhash_free(ctx);
		return -1;
	}
	res = rhash_file_update(ctx, fd); /* hash the file */
	fclose(fd);
	if (res >= 0)
		rhash_final(ctx, result);
	rhash_free(ctx);
	return res;
}

#ifdef _WIN32 /* windows only function */
#include <share.h>

RHASH_API int rhash_wfile(unsigned hash_id, const wchar_t* filepath, unsigned char* result)
{
	FILE* fd;
	rhash ctx;
	int res;

	ctx = rhash_init(hash_id);
	if (!ctx) {
		return -1;
	}
	fd = _wfsopen(filepath, L"rbS", _SH_DENYWR);
	if (!fd) {
		rhash_free(ctx);
		return -1;
	}
	res = rhash_file_update(ctx, fd); /* hash the file */
	fclose(fd);
	if (res >= 0)
		rhash_final(ctx, result);
	rhash_free(ctx);
	return res;
}
#endif

/* RHash information functions */

RHASH_API int rhash_is_base32(unsigned hash_id)
{
	/* fast method is just to test a bit-mask */
	return ((hash_id & (RHASH_TTH | RHASH_AICH)) != 0);
}

/**
 * Returns information about a hash function by its hash_id.
 *
 * @param hash_id the id of hash algorithm
 * @return pointer to the rhash_info structure containing the information
 */
static const rhash_info* rhash_info_by_id(unsigned hash_id)
{
	const rhash_hash_info* hash_info = rhash_hash_info_by_id(hash_id);
	return (hash_info ? hash_info->info : NULL);
}

RHASH_API int rhash_get_digest_size(unsigned hash_id)
{
	const rhash_info* info = rhash_info_by_id(hash_id);
	return (info ? (int)info->digest_size : -1);
}

RHASH_API int rhash_get_hash_length(unsigned hash_id)
{
	const rhash_info* info = rhash_info_by_id(hash_id);
	return (int)(info ? (info->flags & F_BS32 ?
		BASE32_LENGTH(info->digest_size) : info->digest_size * 2) : 0);
}

RHASH_API const char* rhash_get_name(unsigned hash_id)
{
	const rhash_info* info = rhash_info_by_id(hash_id);
	return (info ? info->name : NULL);
}

RHASH_API const char* rhash_get_magnet_name(unsigned hash_id)
{
	const rhash_info* info = rhash_info_by_id(hash_id);
	return (info ? info->magnet_name : NULL);
}

static size_t rhash_get_magnet_url_size(const char* filepath,
	rhash_context_ext* ectx, uint64_t hash_mask, int flags)
{
	size_t size = 0; /* count terminating '\0' */

	/* RHPR_NO_MAGNET, RHPR_FILESIZE */
	if ((flags & RHPR_NO_MAGNET) == 0) {
		size += 8;
	}

	if ((flags & RHPR_FILESIZE) != 0) {
		uint64_t num = ectx->rc.msg_size;

		size += 4;
		if (num == 0) size++;
		else {
			for (; num; num /= 10, size++);
		}
	}

	if (filepath) {
		size += 4 + rhash_urlencode(NULL, filepath, strlen(filepath), 0);
	}
	if (!hash_mask) {
		return size;
	}

	/* loop through hash values */
	for (; hash_mask; hash_mask = hash_mask & (hash_mask - 1)) {
		uint64_t bit = hash_mask & -(int)hash_mask;
		unsigned hash_id = EXTENDED_HASH_ID_FROM_BIT64(bit);
		const char* name = rhash_get_magnet_name(hash_id);
		if (!name)
			continue;
		size += (7 + 2) + strlen(name);
		size += rhash_print(NULL, &ectx->rc, hash_id,
			(bit & RHASH_SHA1 ? RHPR_BASE32 : 0));
	}

	return size;
}

static size_t rhash_print_magnet_impl(char* output, size_t out_size, const char* filepath,
	rhash_context_ext* const ectx, int flags, uint64_t hash_mask)
{
	int i;
	const char* begin = output;

	hash_mask &= ectx->rc.hash_mask;

	if (output == NULL) {
		return rhash_get_magnet_url_size(filepath, ectx, hash_mask, flags);
	}
	if (out_size != RHASH_ERROR) {
		size_t prefix_size = rhash_get_magnet_url_size(filepath, ectx, 0, flags);
		if (out_size < prefix_size) {
			errno = ENOMEM;
			return 0;
		}
		out_size -= prefix_size;
	}

	/* RHPR_NO_MAGNET, RHPR_FILESIZE */
	if ((flags & RHPR_NO_MAGNET) == 0) {
		strcpy(output, "magnet:?");
		output += 8;
	}

	if ((flags & RHPR_FILESIZE) != 0) {
		strcpy(output, "xl=");
		output += 3;
		output += rhash_sprintI64(output, ectx->rc.msg_size);
		*(output++) = '&';
	}

	flags &= RHPR_UPPERCASE;
	if (filepath) {
		strcpy(output, "dn=");
		output += 3;
		output += rhash_urlencode(output, filepath, strlen(filepath), flags);
		*(output++) = '&';
	}

	for (i = 0; i <= 1; i++) {
		static const uint64_t print_first = (RHASH_ED2K | RHASH_AICH);
		uint64_t hash = (!i ? hash_mask & print_first : hash_mask & ~print_first);

		/* loop through hash values */
		for (; hash; hash = hash & (hash - 1)) {
			uint64_t bit = hash & -(int)hash;
			unsigned hash_id = EXTENDED_HASH_ID_FROM_BIT64(bit);
			const char* magnet_name = rhash_get_magnet_name(hash_id);
			size_t name_length;
			if (!magnet_name)
				continue; /* silently skip unsupported hash_id */
			name_length = strlen(magnet_name);
			if (out_size != RHASH_ERROR) {
				size_t hash_part_size = (7 + 2) + name_length +
					rhash_print(NULL, &ectx->rc, hash_id,
						(bit & RHASH_SHA1 ? RHPR_BASE32 : 0));
				if (out_size < hash_part_size) {
					errno = ENOMEM;
					return 0;
				}
				out_size -= hash_part_size;
			}
			strcpy(output, "xt=urn:");
			output += 7;
			strcpy(output, magnet_name);
			output += name_length;
			*(output++) = ':';
			output += rhash_print(output, &ectx->rc, hash_id,
				(bit & RHASH_SHA1 ? flags | RHPR_BASE32 : flags));
			*(output++) = '&';
		}
	}
	output[-1] = '\0'; /* terminate the line */
	return (output - begin);
}

RHASH_API size_t rhash_print_magnet_multi(char* output, size_t out_size, const char* filepath,
	rhash context, int flags, size_t count, const unsigned hash_ids[])
{
	uint64_t hash_mask = 0;
	if (!context) {
		errno = EINVAL;
		return 0;
	}
	if (count == 0 || (count == 1 && hash_ids[0] == RHASH_ALL_HASHES)) {
		hash_mask = RHASH_LOW_HASHES_MASK;
	} else {
		size_t i;
		for (i = 0; i < count; i++) {
			unsigned hash_id = hash_ids[i];
			if (!IS_VALID_HASH_ID(hash_id)) {
				errno = EINVAL;
				return 0;
			}
			if (IS_EXTENDED_HASH_ID(hash_id))
				hash_mask |= I64(1) << GET_EXTENDED_HASH_ID_INDEX(hash_id);
			else
				hash_mask |= hash_id;
		}
	}
	return rhash_print_magnet_impl(output, out_size, filepath,
		(rhash_context_ext*)context, flags, hash_mask);
}

RHASH_API size_t rhash_print_magnet(char* output, const char* filepath,
	rhash context, unsigned hash_mask, int flags)
{
	if (hash_mask == RHASH_ALL_HASHES)
		hash_mask = RHASH_LOW_HASHES_MASK;
	if (!context || IS_EXTENDED_HASH_ID(hash_mask) || !hash_mask) {
		errno = EINVAL;
		return 0;
	}
	return rhash_print_magnet_impl(output, RHASH_ERROR, filepath,
		(rhash_context_ext*)context, flags, (uint64_t)hash_mask);
}


/* HASH SUM OUTPUT INTERFACE */

size_t rhash_print_bytes(char* output, const unsigned char* bytes, size_t size, int flags)
{
	size_t result_length;
	int upper_case = (flags & RHPR_UPPERCASE);
	int format = (flags & ~RHPR_MODIFIER);

	switch (format) {
	case RHPR_HEX:
		result_length = size * 2;
		rhash_byte_to_hex(output, bytes, size, upper_case);
		break;
	case RHPR_BASE32:
		result_length = BASE32_LENGTH(size);
		rhash_byte_to_base32(output, bytes, size, upper_case);
		break;
	case RHPR_BASE64:
		result_length = rhash_base64_url_encoded_helper(output, bytes, size, (flags & RHPR_URLENCODE), upper_case);
		break;
	default:
		if (flags & RHPR_URLENCODE) {
			result_length = rhash_urlencode(output, (char*)bytes, size, upper_case);
		} else {
			memcpy(output, bytes, size);
			result_length = size;
		}
		break;
	}
	return result_length;
}

RHASH_API size_t rhash_print(char* output, rhash context, unsigned hash_id, int flags)
{
	rhash_context_ext* ectx = (rhash_context_ext*)context;
	rhash_vector_item* item = rhash_get_info(ectx, hash_id);
	const rhash_info* info;
	unsigned char digest[80];
	size_t digest_size;

	if (!item || !item->hash_info || !item->hash_info->info) return 0;

	info = item->hash_info->info;
	digest_size = info->digest_size;
	assert(digest_size <= 64);

	flags &= (RHPR_FORMAT | RHPR_MODIFIER);
	if ((flags & RHPR_FORMAT) == 0) {
		/* use default format if not specified by flags */
		flags |= (info->flags & RHASH_INFO_BASE32 ? RHPR_BASE32 : RHPR_HEX);
	}

	if (output == NULL) {
		size_t multiplier = (flags & RHPR_URLENCODE ? 3 : 1);
		switch (flags & RHPR_FORMAT) {
		case RHPR_HEX:
			return (digest_size * 2);
		case RHPR_BASE32:
			return BASE32_LENGTH(digest_size);
		case RHPR_BASE64:
			return BASE64_LENGTH(digest_size) * multiplier;
		default:
			return digest_size * multiplier;
		}
	}

	/* finalize context if not yet finalized and auto-final is on */
	if ((ectx->flags & RCTX_FINALIZED_MASK) == RCTX_AUTO_FINAL) {
		rhash_final(context, NULL);
	}

	rhash_put_digest(item, digest);

	if ((flags & ~RHPR_UPPERCASE) == (RHPR_REVERSE | RHPR_HEX)) {
		/* reverse the digest */
		unsigned char* p = digest;
		unsigned char* r = digest + digest_size - 1;
		char tmp;
		for (; p < r; p++, r--) {
			tmp = *p;
			*p = *r;
			*r = tmp;
		}
	}

	return rhash_print_bytes(output, digest, digest_size, flags);
}

#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(RHASH_EXPORTS)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved);
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
	(void)hModule;
	(void)reserved;
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		rhash_library_init();
		break;
	case DLL_PROCESS_DETACH:
		/*rhash_library_free();*/
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}
#endif

/* Helper macro */
#define ENSURE_THAT(condition) while(!(condition)) { return RHASH_ERROR; }

static rhash_uptr_t rhash_get_algorithms_impl(const rhash_context_ext* ctx, size_t count, unsigned* data)
{
	size_t i;
	if (count != 0 && data != 0) {
		ENSURE_THAT(ctx->hash_vector_size <= count);
		for (i = 0; i < ctx->hash_vector_size; i++)
			data[i] = ctx->vector[i].hash_info->info->hash_id;
	}
	return ctx->hash_vector_size;
}

static size_t hash_bitmask_to_array(unsigned bitmask, size_t count, unsigned* data)
{
	unsigned bits_count;
	bitmask &= ((unsigned)RHASH_EXTENDED_BIT - 1);
	bits_count = rhash_popcount(bitmask);
	if (count > 0 && data != 0) {
		size_t index = 0;
		ENSURE_THAT(bits_count <= count);
		for (unsigned bit = 1; bit <= bitmask; bit = bit << 1) {
			if ((bitmask & bit) != 0)
				data[index++] = bit;
		}
		ENSURE_THAT(bits_count == index);
	}
	return bits_count;
}

#if defined(USE_OPENSSL) || defined(OPENSSL_RUNTIME)
static unsigned ids_array_to_hash_bitmask(size_t count, unsigned* data)
{
	unsigned bitmask = 0;
	size_t i;
	for (i = 0; i < count; i++) {
		if (!IS_EXTENDED_HASH_ID(data[i]))
			bitmask |= data[i];
		else if (data[i] == RHASH_ALL_HASHES)
			bitmask |= RHASH_LOW_HASHES_MASK;
		else
			bitmask |= 1 << GET_EXTENDED_HASH_ID_INDEX(data[i]);
	}
	return bitmask;
}
#endif

RHASH_API size_t rhash_ctrl(rhash context, int cmd, size_t size, void* data)
{
	/* cast to extented rhash context */
	rhash_context_ext* const ctx = (rhash_context_ext*)context;
	switch (cmd) {
	case RMSG_GET_CONTEXT:
		{
			unsigned i;
			unsigned hash_id = convert_to_extended_hash_id((unsigned)size);
			ENSURE_THAT(hash_id);
			ENSURE_THAT(data);
			for (i = 0; i < ctx->hash_vector_size; i++) {
				const struct rhash_hash_info* info = ctx->vector[i].hash_info;
				if (info->info->hash_id == hash_id) {
					*(void**)data = ctx->vector[i].context;
					return 0;
				}
			}
			return RHASH_ERROR;
		}
	case RMSG_CANCEL:
		/* mark rhash context as canceled, in a multithreaded program */
		atomic_compare_and_swap(&ctx->state, STATE_ACTIVE, STATE_STOPPED);
		break;
	case RMSG_IS_CANCELED:
		return (ctx->state == STATE_STOPPED);
	case RMSG_GET_FINALIZED:
		return ((ctx->flags & RCTX_FINALIZED) != 0);
	case RMSG_SET_AUTOFINAL:
		ctx->flags &= ~RCTX_AUTO_FINAL;
		if (size)
			ctx->flags |= RCTX_AUTO_FINAL;
		break;

	case RMSG_HAS_CPU_FEATURE:
		return (size_t)has_cpu_feature((unsigned)size);
	case RMSG_GET_ALL_ALGORITHMS:
		if (data && size) {
			const unsigned* hash_ids;
			ENSURE_THAT(size >= RHASH_HASH_COUNT);
			hash_ids = rhash_get_all_hash_ids(RHASH_ALL_HASHES, &size);
			memcpy(data, hash_ids, size * sizeof(*hash_ids));
		}
		return RHASH_HASH_COUNT;
	case RMSG_GET_CTX_ALGORITHMS:
		ENSURE_THAT(ctx);
		return rhash_get_algorithms_impl(ctx, size, (unsigned*)data);

	case RMSG_GET_OPENSSL_SUPPORTED:
		return hash_bitmask_to_array(
			rhash_get_openssl_supported_hash_mask(), size, (unsigned*)data);
	case RMSG_GET_OPENSSL_AVAILABLE:
		return hash_bitmask_to_array(
			rhash_get_openssl_available_hash_mask(), size, (unsigned*)data);
	case RMSG_GET_OPENSSL_ENABLED:
		return hash_bitmask_to_array(
			rhash_get_openssl_enabled_hash_mask(), size, (unsigned*)data);
	case RMSG_SET_OPENSSL_ENABLED:
		ENSURE_THAT(data || !size);
		rhash_set_openssl_enabled_hash_mask(ids_array_to_hash_bitmask(size, (unsigned*)data));
		break;

	case RMSG_GET_LIBRHASH_VERSION:
		return RHASH_XVERSION;
	default:
		return RHASH_ERROR; /* unknown message */
	}
	return 0;
}

/* Convert a rhash_uptr_t to a void* pointer. */
#define UPTR2PVOID(u) ((void*)((u) + 0))

/* Deprecated function to control rhash, use rhash_ctrl() instead */
RHASH_API rhash_uptr_t rhash_transmit(unsigned msg_id, void* dst, rhash_uptr_t ldata, rhash_uptr_t rdata)
{
	rhash ctx = (rhash)dst;
	(void)rdata;

	switch (msg_id) {
	case RMSG_CANCEL:
	case RMSG_IS_CANCELED:
	case RMSG_GET_FINALIZED:
	case RMSG_SET_AUTOFINAL:
	case RMSG_GET_LIBRHASH_VERSION:
		return rhash_ctrl(ctx, msg_id, (unsigned)ldata, UPTR2PVOID(rdata));

	/* Legacy messages, which operate openssl hash masks directly */
#ifdef USE_OPENSSL
	case RMSG_SET_OPENSSL_MASK:
		rhash_set_openssl_enabled_hash_mask((unsigned)ldata);
		break;
	case RMSG_GET_OPENSSL_MASK:
		return rhash_get_openssl_enabled_hash_mask();
#endif
	case RMSG_GET_OPENSSL_SUPPORTED_MASK:
		return rhash_get_openssl_supported_hash_mask();
	case RMSG_GET_OPENSSL_AVAILABLE_MASK:
		return rhash_get_openssl_available_hash_mask();

	default:
		return RHASH_ERROR; /* unknown message */
	}
	return 0;
}
