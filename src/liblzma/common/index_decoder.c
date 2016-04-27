///////////////////////////////////////////////////////////////////////////////
//
/// \file       index_decoder.c
/// \brief      Decodes the Index field
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "index.h"
#include "check.h"


struct lzma_coder_s {
	enum {
		SEQ_INDICATOR,
		SEQ_COUNT,
		SEQ_MEMUSAGE,
		SEQ_UNPADDED,
		SEQ_UNCOMPRESSED,
		SEQ_PADDING_INIT,
		SEQ_PADDING,
		SEQ_CRC32,
	} sequence;

	/// Memory usage limit
	uint64_t memlimit;

	/// Target Index
	lzma_index *index;

	/// Pointer give by the application, which is set after
	/// successful decoding.
	lzma_index **index_ptr;

	/// Number of Records left to decode.
	lzma_vli count;

	/// The most recent Unpadded Size field
	lzma_vli unpadded_size;

	/// The most recent Uncompressed Size field
	lzma_vli uncompressed_size;

	/// Position in integers
	size_t pos;

	/// CRC32 of the List of Records field
	uint32_t crc32;
};


static lzma_ret
index_decode(lzma_coder *coder, const lzma_allocator *allocator,
		const uint8_t *restrict in, size_t *restrict in_pos,
		size_t in_size,
		uint8_t *restrict out lzma_attribute((__unused__)),
		size_t *restrict out_pos lzma_attribute((__unused__)),
		size_t out_size lzma_attribute((__unused__)),
		lzma_action action lzma_attribute((__unused__)))
{
	// Similar optimization as in index_encoder.c
	const size_t in_start = *in_pos;
	lzma_ret ret = LZMA_OK;

	while (*in_pos < in_size)
	switch (coder->sequence) {
	case SEQ_INDICATOR:
		// Return LZMA_DATA_ERROR instead of e.g. LZMA_PROG_ERROR or
		// LZMA_FORMAT_ERROR, because a typical usage case for Index
		// decoder is when parsing the Stream backwards. If seeking
		// backward from the Stream Footer gives us something that
		// doesn't begin with Index Indicator, the file is considered
		// corrupt, not "programming error" or "unrecognized file
		// format". One could argue that the application should
		// verify the Index Indicator before trying to decode the
		// Index, but well, I suppose it is simpler this way.
		if (in[(*in_pos)++] != 0x00)
			return LZMA_DATA_ERROR;

		coder->sequence = SEQ_COUNT;
		break;

	case SEQ_COUNT:
		ret = lzma_vli_decode(&coder->count, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		coder->pos = 0;
		coder->sequence = SEQ_MEMUSAGE;

	// Fall through

	case SEQ_MEMUSAGE:
		if (lzma_index_memusage(1, coder->count) > coder->memlimit) {
			ret = LZMA_MEMLIMIT_ERROR;
			goto out;
		}

		// Tell the Index handling code how many Records this
		// Index has to allow it to allocate memory more efficiently.
		lzma_index_prealloc(coder->index, coder->count);

		ret = LZMA_OK;
		coder->sequence = coder->count == 0
				? SEQ_PADDING_INIT : SEQ_UNPADDED;
		break;

	case SEQ_UNPADDED:
	case SEQ_UNCOMPRESSED: {
		lzma_vli *size = coder->sequence == SEQ_UNPADDED
				? &coder->unpadded_size
				: &coder->uncompressed_size;

		ret = lzma_vli_decode(size, &coder->pos,
				in, in_pos, in_size);
		if (ret != LZMA_STREAM_END)
			goto out;

		ret = LZMA_OK;
		coder->pos = 0;

		if (coder->sequence == SEQ_UNPADDED) {
			// Validate that encoded Unpadded Size isn't too small
			// or too big.
			if (coder->unpadded_size < UNPADDED_SIZE_MIN
					|| coder->unpadded_size
						> UNPADDED_SIZE_MAX)
				return LZMA_DATA_ERROR;

			coder->sequence = SEQ_UNCOMPRESSED;
		} else {
			// Add the decoded Record to the Index.
			return_if_error(lzma_index_append(
					coder->index, allocator,
					coder->unpadded_size,
					coder->uncompressed_size));

			// Check if this was the last Record.
			coder->sequence = --coder->count == 0
					? SEQ_PADDING_INIT
					: SEQ_UNPADDED;
		}

		break;
	}

	case SEQ_PADDING_INIT:
		coder->pos = lzma_index_padding_size(coder->index);
		coder->sequence = SEQ_PADDING;

	// Fall through

	case SEQ_PADDING:
		if (coder->pos > 0) {
			--coder->pos;
			if (in[(*in_pos)++] != 0x00)
				return LZMA_DATA_ERROR;

			break;
		}

		// Finish the CRC32 calculation.
		coder->crc32 = lzma_crc32(in + in_start,
				*in_pos - in_start, coder->crc32);

		coder->sequence = SEQ_CRC32;

	// Fall through

	case SEQ_CRC32:
		do {
			if (*in_pos == in_size)
				return LZMA_OK;

			if (((coder->crc32 >> (coder->pos * 8)) & 0xFF)
					!= in[(*in_pos)++])
				return LZMA_DATA_ERROR;

		} while (++coder->pos < 4);

		// Decoding was successful, now we can let the application
		// see the decoded Index.
		*coder->index_ptr = coder->index;

		// Make index NULL so we don't free it unintentionally.
		coder->index = NULL;

		return LZMA_STREAM_END;

	default:
		assert(0);
		return LZMA_PROG_ERROR;
	}

out:
	// Update the CRC32,
	coder->crc32 = lzma_crc32(in + in_start,
			*in_pos - in_start, coder->crc32);

	return ret;
}


static void
index_decoder_end(lzma_coder *coder, const lzma_allocator *allocator)
{
	lzma_index_end(coder->index, allocator);
	lzma_free(coder, allocator);
	return;
}


static lzma_ret
index_decoder_memconfig(lzma_coder *coder, uint64_t *memusage,
		uint64_t *old_memlimit, uint64_t new_memlimit)
{
	*memusage = lzma_index_memusage(1, coder->count);
	*old_memlimit = coder->memlimit;

	if (new_memlimit != 0) {
		if (new_memlimit < *memusage)
			return LZMA_MEMLIMIT_ERROR;

		coder->memlimit = new_memlimit;
	}

	return LZMA_OK;
}


static lzma_ret
index_decoder_reset(lzma_coder *coder, const lzma_allocator *allocator,
		lzma_index **i, uint64_t memlimit)
{
	// Remember the pointer given by the application. We will set it
	// to point to the decoded Index only if decoding is successful.
	// Before that, keep it NULL so that applications can always safely
	// pass it to lzma_index_end() no matter did decoding succeed or not.
	coder->index_ptr = i;
	*i = NULL;

	// We always allocate a new lzma_index.
	coder->index = lzma_index_init(allocator);
	if (coder->index == NULL)
		return LZMA_MEM_ERROR;

	// Initialize the rest.
	coder->sequence = SEQ_INDICATOR;
	coder->memlimit = memlimit;
	coder->count = 0; // Needs to be initialized due to _memconfig().
	coder->pos = 0;
	coder->crc32 = 0;

	return LZMA_OK;
}


static lzma_ret
index_decoder_init(lzma_next_coder *next, const lzma_allocator *allocator,
		lzma_index **i, uint64_t memlimit)
{
	lzma_next_coder_init(&index_decoder_init, next, allocator);

	if (i == NULL || memlimit == 0)
		return LZMA_PROG_ERROR;

	if (next->coder == NULL) {
		next->coder = lzma_alloc(sizeof(lzma_coder), allocator);
		if (next->coder == NULL)
			return LZMA_MEM_ERROR;

		next->code = &index_decode;
		next->end = &index_decoder_end;
		next->memconfig = &index_decoder_memconfig;
		next->coder->index = NULL;
	} else {
		lzma_index_end(next->coder->index, allocator);
	}

	return index_decoder_reset(next->coder, allocator, i, memlimit);
}


extern LZMA_API(lzma_ret)
lzma_index_decoder(lzma_stream *strm, lzma_index **i, uint64_t memlimit)
{
	lzma_next_strm_init(index_decoder_init, strm, i, memlimit);

	strm->internal->supported_actions[LZMA_RUN] = true;
	strm->internal->supported_actions[LZMA_FINISH] = true;

	return LZMA_OK;
}


extern LZMA_API(lzma_ret)
lzma_index_buffer_decode(lzma_index **i, uint64_t *memlimit,
		const lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
{
	// Sanity checks
	if (i == NULL || memlimit == NULL
			|| in == NULL || in_pos == NULL || *in_pos > in_size)
		return LZMA_PROG_ERROR;

	// Initialize the decoder.
	lzma_coder coder;
	return_if_error(index_decoder_reset(&coder, allocator, i, *memlimit));

	// Store the input start position so that we can restore it in case
	// of an error.
	const size_t in_start = *in_pos;

	// Do the actual decoding.
	lzma_ret ret = index_decode(&coder, allocator, in, in_pos, in_size,
			NULL, NULL, 0, LZMA_RUN);

	if (ret == LZMA_STREAM_END) {
		ret = LZMA_OK;
	} else {
		// Something went wrong, free the Index structure and restore
		// the input position.
		lzma_index_end(coder.index, allocator);
		*in_pos = in_start;

		if (ret == LZMA_OK) {
			// The input is truncated or otherwise corrupt.
			// Use LZMA_DATA_ERROR instead of LZMA_BUF_ERROR
			// like lzma_vli_decode() does in single-call mode.
			ret = LZMA_DATA_ERROR;

		} else if (ret == LZMA_MEMLIMIT_ERROR) {
			// Tell the caller how much memory would have
			// been needed.
			*memlimit = lzma_index_memusage(1, coder.count);
		}
	}

	return ret;
}

struct lzma_index_parser_internal_s {
	/// Current state.
	enum {
		PARSE_INDEX_INITED,
		PARSE_INDEX_READ_FOOTER,
		PARSE_INDEX_READ_INDEX,
		PARSE_INDEX_READ_STREAM_HEADER
	} state;

	/// Current position in the file. We parse the file backwards so
	/// initialize it to point to the end of the file.
	off_t pos;

	/// The footer flags of the current XZ stream.
	lzma_stream_flags footer_flags;

	/// All Indexes decoded so far.
	lzma_index *combined_index;

	/// The Index currently being decoded.
	lzma_index *this_index;

	/// Padding of the stream currently being decoded.
	lzma_vli stream_padding;

	/// Size of the Index currently being decoded.
	lzma_vli index_size;

	/// Keep track of how much memory is being used for Index decoding.
	uint64_t memused;

	/// lzma_stream for the Index decoder.
	lzma_stream strm;

	/// Keep the buffer coming as the last member to so all data that is
	/// ever actually used fits in a few cache lines.
	uint8_t buf[8192];
};

static lzma_ret
parse_indexes_read(lzma_index_parser_data *info,
                   uint8_t *buf,
                   size_t size,
                   off_t pos)
{
	ssize_t read = info->read_callback(info->opaque, buf, size, pos);

	if (read < 0) {
		return LZMA_DATA_ERROR;
	}

	if ((size_t)read != size) {
		info->message = "Unexpected end of file";
		return LZMA_DATA_ERROR;
	}

	return LZMA_OK;
}

extern LZMA_API(lzma_ret)
lzma_parse_indexes_from_file(lzma_index_parser_data *info)
{
	lzma_ret ret;
	lzma_index_parser_internal *internal = info->internal;
	info->message = NULL;

	// Passing file_size == SIZE_MAX can be used to safely clean up
	// everything when I/O failed asynchronously.
	if (info->file_size == SIZE_MAX) {
		ret = LZMA_OPTIONS_ERROR;
		goto error;
	}

	if (info->memlimit == 0) {
		info->memlimit = UINT64_MAX;
	}

	if (internal == NULL) {
		if (info->memlimit <= sizeof(lzma_index_parser_internal)) {
			// We don't really have a good figure for how much
			// memory may be necessary. Set memlimit to 0 to
			// indicate that something is obviously inacceptable.
			info->memlimit = 0;
			return LZMA_MEMLIMIT_ERROR;
		}

		internal = lzma_alloc(sizeof(lzma_index_parser_internal),
			info->allocator);

		if (internal == NULL)
			return LZMA_MEM_ERROR;

		internal->state = PARSE_INDEX_INITED;
		internal->pos = info->file_size;
		internal->combined_index = NULL;
		internal->this_index = NULL;
		info->internal = internal;

		lzma_stream strm_ = LZMA_STREAM_INIT;
		memcpy(&internal->strm, &strm_, sizeof(lzma_stream));
	}

	// The header flags of the current stream are only ever used within a
	// call and don't need to go into the internals struct.
	lzma_stream_flags header_flags;

	int i;

	switch (internal->state) {
case PARSE_INDEX_INITED:
	if (info->file_size <= 0) {
		// These strings are fixed so they can be translated by the xz
		// command line utility.
		info->message = "File is empty";
		return LZMA_DATA_ERROR;
	}

	if (info->file_size < 2 * LZMA_STREAM_HEADER_SIZE) {
		info->message = "Too small to be a valid .xz file";
		return LZMA_DATA_ERROR;
	}

	// Each loop iteration decodes one Index.
	do {
		// Check that there is enough data left to contain at least
		// the Stream Header and Stream Footer. This check cannot
		// fail in the first pass of this loop.
		if (internal->pos < 2 * LZMA_STREAM_HEADER_SIZE) {
			ret = LZMA_DATA_ERROR;
			goto error;
		}

		internal->pos -= LZMA_STREAM_HEADER_SIZE;
		internal->stream_padding = 0;

		// Locate the Stream Footer. There may be Stream Padding which
		// we must skip when reading backwards.
		while (true) {
			if (internal->pos < LZMA_STREAM_HEADER_SIZE) {
				ret = LZMA_DATA_ERROR;
				goto error;
			}

			ret = parse_indexes_read(info,
					internal->buf,
					LZMA_STREAM_HEADER_SIZE,
					internal->pos);

			if (ret != LZMA_OK)
				goto error;
			internal->state = PARSE_INDEX_READ_FOOTER;
			if (info->async) return LZMA_OK;
case PARSE_INDEX_READ_FOOTER:

			// Stream Padding is always a multiple of four bytes.
			i = 2;
			if (((uint32_t *)internal->buf)[i] != 0)
				break;

			// To avoid calling the read callback for every four
			// bytes of Stream Padding, take advantage that we
			// read 12 bytes (LZMA_STREAM_HEADER_SIZE) already
			// and check them too before calling the read
			// callback again.
			do {
				internal->stream_padding += 4;
				internal->pos -= 4;
				--i;
			} while (i >= 0 && ((uint32_t *)internal->buf)[i] == 0);
		}

		// Decode the Stream Footer.
		ret = lzma_stream_footer_decode(&internal->footer_flags,
				internal->buf);
		if (ret != LZMA_OK) {
			goto error;
		}

		// Check that the Stream Footer doesn't specify something
		// that we don't support. This can only happen if the xz
		// version is older than liblzma and liblzma supports
		// something new.
		//
		// It is enough to check Stream Footer. Stream Header must
		// match when it is compared against Stream Footer with
		// lzma_stream_flags_compare().
		if (internal->footer_flags.version != 0) {
			ret = LZMA_OPTIONS_ERROR;
			goto error;
		}

		// Check that the size of the Index field looks sane.
		internal->index_size = internal->footer_flags.backward_size;
		if ((lzma_vli)(internal->pos) <
				internal->index_size +
				LZMA_STREAM_HEADER_SIZE) {
			ret = LZMA_DATA_ERROR;
			goto error;
		}

		// Set pos to the beginning of the Index.
		internal->pos -= internal->index_size;

		// See how much memory we can use for decoding this Index.
		uint64_t memlimit = info->memlimit;
		internal->memused = sizeof(lzma_index_parser_internal);
		if (internal->combined_index != NULL) {
			internal->memused = lzma_index_memused(
				internal->combined_index);
			assert(internal->memused <= memlimit);

			memlimit -= internal->memused;
		}

		// Decode the Index.
		ret = lzma_index_decoder(&internal->strm,
				&internal->this_index,
				memlimit);
		if (ret != LZMA_OK) {
			goto error;
		}

		do {
			// Don't give the decoder more input than the
			// Index size.
			internal->strm.avail_in = my_min(sizeof(internal->buf),
					internal->index_size);

			ret = parse_indexes_read(info,
					internal->buf,
					internal->strm.avail_in,
					internal->pos);

			if (ret != LZMA_OK)
				goto error;
			internal->state = PARSE_INDEX_READ_INDEX;
			if (info->async) return LZMA_OK;
case PARSE_INDEX_READ_INDEX:

			internal->pos += internal->strm.avail_in;
			internal->index_size -= internal->strm.avail_in;

			internal->strm.next_in = internal->buf;
			ret = lzma_code(&internal->strm, LZMA_RUN);

		} while (ret == LZMA_OK);

		// If the decoding seems to be successful, check also that
		// the Index decoder consumed as much input as indicated
		// by the Backward Size field.
		if (ret == LZMA_STREAM_END && (
			internal->index_size != 0 ||
			internal->strm.avail_in != 0)) {
			ret = LZMA_DATA_ERROR;
		}

		if (ret != LZMA_STREAM_END) {
			// LZMA_BUFFER_ERROR means that the Index decoder
			// would have liked more input than what the Index
			// size should be according to Stream Footer.
			// The message for LZMA_DATA_ERROR makes more
			// sense in that case.
			if (ret == LZMA_BUF_ERROR)
				ret = LZMA_DATA_ERROR;

			// If the error was too low memory usage limit,
			// indicate also how much memory would have been needed.
			if (ret == LZMA_MEMLIMIT_ERROR) {
				uint64_t needed = lzma_memusage(
					&internal->strm);
				if (UINT64_MAX - needed < internal->memused)
					needed = UINT64_MAX;
				else
					needed += internal->memused;

				info->memlimit = needed;
			}

			goto error;
		}

		// Decode the Stream Header and check that its Stream Flags
		// match the Stream Footer.
		internal->pos -= internal->footer_flags.backward_size;
		internal->pos -= LZMA_STREAM_HEADER_SIZE;
		if ((lzma_vli)(internal->pos) <
				lzma_index_total_size(internal->this_index)) {
			ret = LZMA_DATA_ERROR;
			goto error;
		}

		internal->pos -= lzma_index_total_size(internal->this_index);

		ret = parse_indexes_read(info,
				internal->buf,
				LZMA_STREAM_HEADER_SIZE,
				internal->pos);

		if (ret != LZMA_OK)
			goto error;

		internal->state = PARSE_INDEX_READ_STREAM_HEADER;
		if (info->async) return LZMA_OK;
case PARSE_INDEX_READ_STREAM_HEADER:

		ret = lzma_stream_header_decode(&header_flags, internal->buf);
		if (ret != LZMA_OK) {
			goto error;
		}

		ret = lzma_stream_flags_compare(&header_flags,
				&internal->footer_flags);
		if (ret != LZMA_OK) {
			goto error;
		}

		// Store the decoded Stream Flags into this_index. This is
		// needed so that we can print which Check is used in each
		// Stream.
		ret = lzma_index_stream_flags(internal->this_index,
				&internal->footer_flags);
		assert(ret == LZMA_OK);

		// Store also the size of the Stream Padding field. It is
		// needed to show the offsets of the Streams correctly.
		ret = lzma_index_stream_padding(internal->this_index,
				internal->stream_padding);
		assert(ret == LZMA_OK);

		if (internal->combined_index != NULL) {
			// Append the earlier decoded Indexes
			// after this_index.
			ret = lzma_index_cat(
					internal->this_index,
					internal->combined_index,
					NULL);
			if (ret != LZMA_OK) {
				goto error;
			}
		}

		internal->combined_index = internal->this_index;
		internal->this_index = NULL;

		info->stream_padding += internal->stream_padding;

	} while (internal->pos > 0);

	lzma_end(&internal->strm);

	// All OK. Make combined_index available to the caller.
	info->index = internal->combined_index;

	lzma_free(internal, info->allocator);
	info->internal = NULL;
	return LZMA_STREAM_END;
} // end switch(internal->state)

error:
	// Something went wrong, free the allocated memory.
	if (internal) {
		lzma_end(&internal->strm);
		lzma_index_end(internal->combined_index, NULL);
		lzma_index_end(internal->this_index, NULL);
		lzma_free(internal, info->allocator);
	}

	info->internal = NULL;

	// Doing this will prevent people from calling lzma_parse_indexes_from_file()
	// again without re-initializing.
	info->file_size = SIZE_MAX;
	return ret;
}
