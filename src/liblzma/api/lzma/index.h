/**
 * \file        lzma/index.h
 * \brief       Handling of .xz Index and related information
 */

/*
 * Author: Lasse Collin
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 *
 * See ../lzma.h for information about liblzma as a whole.
 */

#ifndef LZMA_H_INTERNAL
#	error Never include this file directly. Use <lzma.h> instead.
#endif


/**
 * \brief       Opaque data type to hold the Index(es) and other information
 *
 * lzma_index often holds just one .xz Index and possibly the Stream Flags
 * of the same Stream and size of the Stream Padding field. However,
 * multiple lzma_indexes can be concatenated with lzma_index_cat() and then
 * there may be information about multiple Streams in the same lzma_index.
 *
 * Notes about thread safety: Only one thread may modify lzma_index at
 * a time. All functions that take non-const pointer to lzma_index
 * modify it. As long as no thread is modifying the lzma_index, getting
 * information from the same lzma_index can be done from multiple threads
 * at the same time with functions that take a const pointer to
 * lzma_index or use lzma_index_iter. The same iterator must be used
 * only by one thread at a time, of course, but there can be as many
 * iterators for the same lzma_index as needed.
 */
typedef struct lzma_index_s lzma_index;


/**
 * \brief       Iterator to get information about Blocks and Streams
 */
typedef struct {
	struct {
		/**
		 * \brief       Pointer to Stream Flags
		 *
		 * This is NULL if Stream Flags have not been set for
		 * this Stream with lzma_index_stream_flags().
		 */
		const lzma_stream_flags *flags;

		const void *reserved_ptr1;
		const void *reserved_ptr2;
		const void *reserved_ptr3;

		/**
		 * \brief       Stream number in the lzma_index
		 *
		 * The first Stream is 1.
		 */
		lzma_vli number;

		/**
		 * \brief       Number of Blocks in the Stream
		 *
		 * If this is zero, the block structure below has
		 * undefined values.
		 */
		lzma_vli block_count;

		/**
		 * \brief       Compressed start offset of this Stream
		 *
		 * The offset is relative to the beginning of the lzma_index
		 * (i.e. usually the beginning of the .xz file).
		 */
		lzma_vli compressed_offset;

		/**
		 * \brief       Uncompressed start offset of this Stream
		 *
		 * The offset is relative to the beginning of the lzma_index
		 * (i.e. usually the beginning of the .xz file).
		 */
		lzma_vli uncompressed_offset;

		/**
		 * \brief       Compressed size of this Stream
		 *
		 * This includes all headers except the possible
		 * Stream Padding after this Stream.
		 */
		lzma_vli compressed_size;

		/**
		 * \brief       Uncompressed size of this Stream
		 */
		lzma_vli uncompressed_size;

		/**
		 * \brief       Size of Stream Padding after this Stream
		 *
		 * If it hasn't been set with lzma_index_stream_padding(),
		 * this defaults to zero. Stream Padding is always
		 * a multiple of four bytes.
		 */
		lzma_vli padding;

		lzma_vli reserved_vli1;
		lzma_vli reserved_vli2;
		lzma_vli reserved_vli3;
		lzma_vli reserved_vli4;
	} stream;

	struct {
		/**
		 * \brief       Block number in the file
		 *
		 * The first Block is 1.
		 */
		lzma_vli number_in_file;

		/**
		 * \brief       Compressed start offset of this Block
		 *
		 * This offset is relative to the beginning of the
		 * lzma_index (i.e. usually the beginning of the .xz file).
		 * Normally this is where you should seek in the .xz file
		 * to start decompressing this Block.
		 */
		lzma_vli compressed_file_offset;

		/**
		 * \brief       Uncompressed start offset of this Block
		 *
		 * This offset is relative to the beginning of the lzma_index
		 * (i.e. usually the beginning of the .xz file).
		 *
		 * When doing random-access reading, it is possible that
		 * the target offset is not exactly at Block boundary. One
		 * will need to compare the target offset against
		 * uncompressed_file_offset or uncompressed_stream_offset,
		 * and possibly decode and throw away some amount of data
		 * before reaching the target offset.
		 */
		lzma_vli uncompressed_file_offset;

		/**
		 * \brief       Block number in this Stream
		 *
		 * The first Block is 1.
		 */
		lzma_vli number_in_stream;

		/**
		 * \brief       Compressed start offset of this Block
		 *
		 * This offset is relative to the beginning of the Stream
		 * containing this Block.
		 */
		lzma_vli compressed_stream_offset;

		/**
		 * \brief       Uncompressed start offset of this Block
		 *
		 * This offset is relative to the beginning of the Stream
		 * containing this Block.
		 */
		lzma_vli uncompressed_stream_offset;

		/**
		 * \brief       Uncompressed size of this Block
		 *
		 * You should pass this to the Block decoder if you will
		 * decode this Block. It will allow the Block decoder to
		 * validate the uncompressed size.
		 */
		lzma_vli uncompressed_size;

		/**
		 * \brief       Unpadded size of this Block
		 *
		 * You should pass this to the Block decoder if you will
		 * decode this Block. It will allow the Block decoder to
		 * validate the unpadded size.
		 */
		lzma_vli unpadded_size;

		/**
		 * \brief       Total compressed size
		 *
		 * This includes all headers and padding in this Block.
		 * This is useful if you need to know how many bytes
		 * the Block decoder will actually read.
		 */
		lzma_vli total_size;

		lzma_vli reserved_vli1;
		lzma_vli reserved_vli2;
		lzma_vli reserved_vli3;
		lzma_vli reserved_vli4;

		const void *reserved_ptr1;
		const void *reserved_ptr2;
		const void *reserved_ptr3;
		const void *reserved_ptr4;
	} block;

	/*
	 * Internal data which is used to store the state of the iterator.
	 * The exact format may vary between liblzma versions, so don't
	 * touch these in any way.
	 */
	union {
		const void *p;
		size_t s;
		lzma_vli v;
	} internal[6];
} lzma_index_iter;


/**
 * \brief       Operation mode for lzma_index_iter_next()
 */
typedef enum {
	LZMA_INDEX_ITER_ANY             = 0,
		/**<
		 * \brief       Get the next Block or Stream
		 *
		 * Go to the next Block if the current Stream has at least
		 * one Block left. Otherwise go to the next Stream even if
		 * it has no Blocks. If the Stream has no Blocks
		 * (lzma_index_iter.stream.block_count == 0),
		 * lzma_index_iter.block will have undefined values.
		 */

	LZMA_INDEX_ITER_STREAM          = 1,
		/**<
		 * \brief       Get the next Stream
		 *
		 * Go to the next Stream even if the current Stream has
		 * unread Blocks left. If the next Stream has at least one
		 * Block, the iterator will point to the first Block.
		 * If there are no Blocks, lzma_index_iter.block will have
		 * undefined values.
		 */

	LZMA_INDEX_ITER_BLOCK           = 2,
		/**<
		 * \brief       Get the next Block
		 *
		 * Go to the next Block if the current Stream has at least
		 * one Block left. If the current Stream has no Blocks left,
		 * the next Stream with at least one Block is located and
		 * the iterator will be made to point to the first Block of
		 * that Stream.
		 */

	LZMA_INDEX_ITER_NONEMPTY_BLOCK  = 3
		/**<
		 * \brief       Get the next non-empty Block
		 *
		 * This is like LZMA_INDEX_ITER_BLOCK except that it will
		 * skip Blocks whose Uncompressed Size is zero.
		 */

} lzma_index_iter_mode;


/**
 * \brief       Calculate memory usage of lzma_index
 *
 * On disk, the size of the Index field depends on both the number of Records
 * stored and how big values the Records store (due to variable-length integer
 * encoding). When the Index is kept in lzma_index structure, the memory usage
 * depends only on the number of Records/Blocks stored in the Index(es), and
 * in case of concatenated lzma_indexes, the number of Streams. The size in
 * RAM is almost always significantly bigger than in the encoded form on disk.
 *
 * This function calculates an approximate amount of memory needed hold
 * the given number of Streams and Blocks in lzma_index structure. This
 * value may vary between CPU architectures and also between liblzma versions
 * if the internal implementation is modified.
 */
extern LZMA_API(uint64_t) lzma_index_memusage(
		lzma_vli streams, lzma_vli blocks) lzma_nothrow;


/**
 * \brief       Calculate the memory usage of an existing lzma_index
 *
 * This is a shorthand for lzma_index_memusage(lzma_index_stream_count(i),
 * lzma_index_block_count(i)).
 */
extern LZMA_API(uint64_t) lzma_index_memused(const lzma_index *i)
		lzma_nothrow;


/**
 * \brief       Allocate and initialize a new lzma_index structure
 *
 * \return      On success, a pointer to an empty initialized lzma_index is
 *              returned. If allocation fails, NULL is returned.
 */
extern LZMA_API(lzma_index *) lzma_index_init(const lzma_allocator *allocator)
		lzma_nothrow;


/**
 * \brief       Deallocate lzma_index
 *
 * If i is NULL, this does nothing.
 */
extern LZMA_API(void) lzma_index_end(
		lzma_index *i, const lzma_allocator *allocator) lzma_nothrow;


/**
 * \brief       Add a new Block to lzma_index
 *
 * \param       i                 Pointer to a lzma_index structure
 * \param       allocator         Pointer to lzma_allocator, or NULL to
 *                                use malloc()
 * \param       unpadded_size     Unpadded Size of a Block. This can be
 *                                calculated with lzma_block_unpadded_size()
 *                                after encoding or decoding the Block.
 * \param       uncompressed_size Uncompressed Size of a Block. This can be
 *                                taken directly from lzma_block structure
 *                                after encoding or decoding the Block.
 *
 * Appending a new Block does not invalidate iterators. For example,
 * if an iterator was pointing to the end of the lzma_index, after
 * lzma_index_append() it is possible to read the next Block with
 * an existing iterator.
 *
 * \return      - LZMA_OK
 *              - LZMA_MEM_ERROR
 *              - LZMA_DATA_ERROR: Compressed or uncompressed size of the
 *                Stream or size of the Index field would grow too big.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_append(
		lzma_index *i, const lzma_allocator *allocator,
		lzma_vli unpadded_size, lzma_vli uncompressed_size)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Set the Stream Flags
 *
 * Set the Stream Flags of the last (and typically the only) Stream
 * in lzma_index. This can be useful when reading information from the
 * lzma_index, because to decode Blocks, knowing the integrity check type
 * is needed.
 *
 * The given Stream Flags are copied into internal preallocated structure
 * in the lzma_index, thus the caller doesn't need to keep the *stream_flags
 * available after calling this function.
 *
 * \return      - LZMA_OK
 *              - LZMA_OPTIONS_ERROR: Unsupported stream_flags->version.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_stream_flags(
		lzma_index *i, const lzma_stream_flags *stream_flags)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Get the types of integrity Checks
 *
 * If lzma_index_stream_flags() is used to set the Stream Flags for
 * every Stream, lzma_index_checks() can be used to get a bitmask to
 * indicate which Check types have been used. It can be useful e.g. if
 * showing the Check types to the user.
 *
 * The bitmask is 1 << check_id, e.g. CRC32 is 1 << 1 and SHA-256 is 1 << 10.
 */
extern LZMA_API(uint32_t) lzma_index_checks(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Set the amount of Stream Padding
 *
 * Set the amount of Stream Padding of the last (and typically the only)
 * Stream in the lzma_index. This is needed when planning to do random-access
 * reading within multiple concatenated Streams.
 *
 * By default, the amount of Stream Padding is assumed to be zero bytes.
 *
 * \return      - LZMA_OK
 *              - LZMA_DATA_ERROR: The file size would grow too big.
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_stream_padding(
		lzma_index *i, lzma_vli stream_padding)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Get the number of Streams
 */
extern LZMA_API(lzma_vli) lzma_index_stream_count(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the number of Blocks
 *
 * This returns the total number of Blocks in lzma_index. To get number
 * of Blocks in individual Streams, use lzma_index_iter.
 */
extern LZMA_API(lzma_vli) lzma_index_block_count(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the size of the Index field as bytes
 *
 * This is needed to verify the Backward Size field in the Stream Footer.
 */
extern LZMA_API(lzma_vli) lzma_index_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the Stream
 *
 * If multiple lzma_indexes have been combined, this works as if the Blocks
 * were in a single Stream. This is useful if you are going to combine
 * Blocks from multiple Streams into a single new Stream.
 */
extern LZMA_API(lzma_vli) lzma_index_stream_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the Blocks
 *
 * This doesn't include the Stream Header, Stream Footer, Stream Padding,
 * or Index fields.
 */
extern LZMA_API(lzma_vli) lzma_index_total_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the total size of the file
 *
 * When no lzma_indexes have been combined with lzma_index_cat() and there is
 * no Stream Padding, this function is identical to lzma_index_stream_size().
 * If multiple lzma_indexes have been combined, this includes also the headers
 * of each separate Stream and the possible Stream Padding fields.
 */
extern LZMA_API(lzma_vli) lzma_index_file_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Get the uncompressed size of the file
 */
extern LZMA_API(lzma_vli) lzma_index_uncompressed_size(const lzma_index *i)
		lzma_nothrow lzma_attr_pure;


/**
 * \brief       Initialize an iterator
 *
 * \param       iter    Pointer to a lzma_index_iter structure
 * \param       i       lzma_index to which the iterator will be associated
 *
 * This function associates the iterator with the given lzma_index, and calls
 * lzma_index_iter_rewind() on the iterator.
 *
 * This function doesn't allocate any memory, thus there is no
 * lzma_index_iter_end(). The iterator is valid as long as the
 * associated lzma_index is valid, that is, until lzma_index_end() or
 * using it as source in lzma_index_cat(). Specifically, lzma_index doesn't
 * become invalid if new Blocks are added to it with lzma_index_append() or
 * if it is used as the destination in lzma_index_cat().
 *
 * It is safe to make copies of an initialized lzma_index_iter, for example,
 * to easily restart reading at some particular position.
 */
extern LZMA_API(void) lzma_index_iter_init(
		lzma_index_iter *iter, const lzma_index *i) lzma_nothrow;


/**
 * \brief       Rewind the iterator
 *
 * Rewind the iterator so that next call to lzma_index_iter_next() will
 * return the first Block or Stream.
 */
extern LZMA_API(void) lzma_index_iter_rewind(lzma_index_iter *iter)
		lzma_nothrow;


/**
 * \brief       Get the next Block or Stream
 *
 * \param       iter    Iterator initialized with lzma_index_iter_init()
 * \param       mode    Specify what kind of information the caller wants
 *                      to get. See lzma_index_iter_mode for details.
 *
 * \return      If next Block or Stream matching the mode was found, *iter
 *              is updated and this function returns false. If no Block or
 *              Stream matching the mode is found, *iter is not modified
 *              and this function returns true. If mode is set to an unknown
 *              value, *iter is not modified and this function returns true.
 */
extern LZMA_API(lzma_bool) lzma_index_iter_next(
		lzma_index_iter *iter, lzma_index_iter_mode mode)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Locate a Block
 *
 * If it is possible to seek in the .xz file, it is possible to parse
 * the Index field(s) and use lzma_index_iter_locate() to do random-access
 * reading with granularity of Block size.
 *
 * \param       iter    Iterator that was earlier initialized with
 *                      lzma_index_iter_init().
 * \param       target  Uncompressed target offset which the caller would
 *                      like to locate from the Stream
 *
 * If the target is smaller than the uncompressed size of the Stream (can be
 * checked with lzma_index_uncompressed_size()):
 *  - Information about the Stream and Block containing the requested
 *    uncompressed offset is stored into *iter.
 *  - Internal state of the iterator is adjusted so that
 *    lzma_index_iter_next() can be used to read subsequent Blocks or Streams.
 *  - This function returns false.
 *
 * If target is greater than the uncompressed size of the Stream, *iter
 * is not modified, and this function returns true.
 */
extern LZMA_API(lzma_bool) lzma_index_iter_locate(
		lzma_index_iter *iter, lzma_vli target) lzma_nothrow;


/**
 * \brief       Concatenate lzma_indexes
 *
 * Concatenating lzma_indexes is useful when doing random-access reading in
 * multi-Stream .xz file, or when combining multiple Streams into single
 * Stream.
 *
 * \param       dest      lzma_index after which src is appended
 * \param       src       lzma_index to be appended after dest. If this
 *                        function succeeds, the memory allocated for src
 *                        is freed or moved to be part of dest, and all
 *                        iterators pointing to src will become invalid.
 * \param       allocator Custom memory allocator; can be NULL to use
 *                        malloc() and free().
 *
 * \return      - LZMA_OK: lzma_indexes were concatenated successfully.
 *                src is now a dangling pointer.
 *              - LZMA_DATA_ERROR: *dest would grow too big.
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_cat(lzma_index *dest, lzma_index *src,
		const lzma_allocator *allocator)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Duplicate lzma_index
 *
 * \return      A copy of the lzma_index, or NULL if memory allocation failed.
 */
extern LZMA_API(lzma_index *) lzma_index_dup(
		const lzma_index *i, const lzma_allocator *allocator)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Initialize .xz Index encoder
 *
 * \param       strm        Pointer to properly prepared lzma_stream
 * \param       i           Pointer to lzma_index which should be encoded.
 *
 * The valid `action' values for lzma_code() are LZMA_RUN and LZMA_FINISH.
 * It is enough to use only one of them (you can choose freely; use LZMA_RUN
 * to support liblzma versions older than 5.0.0).
 *
 * \return      - LZMA_OK: Initialization succeeded, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_encoder(
		lzma_stream *strm, const lzma_index *i)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Initialize .xz Index decoder
 *
 * \param       strm        Pointer to properly prepared lzma_stream
 * \param       i           The decoded Index will be made available via
 *                          this pointer. Initially this function will
 *                          set *i to NULL (the old value is ignored). If
 *                          decoding succeeds (lzma_code() returns
 *                          LZMA_STREAM_END), *i will be set to point
 *                          to a new lzma_index, which the application
 *                          has to later free with lzma_index_end().
 * \param       memlimit    How much memory the resulting lzma_index is
 *                          allowed to require.
 *
 * The valid `action' values for lzma_code() are LZMA_RUN and LZMA_FINISH.
 * It is enough to use only one of them (you can choose freely; use LZMA_RUN
 * to support liblzma versions older than 5.0.0).
 *
 * \return      - LZMA_OK: Initialization succeeded, continue with lzma_code().
 *              - LZMA_MEM_ERROR
 *              - LZMA_MEMLIMIT_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_decoder(
		lzma_stream *strm, lzma_index **i, uint64_t memlimit)
		lzma_nothrow lzma_attr_warn_unused_result;


/**
 * \brief       Single-call .xz Index encoder
 *
 * \param       i         lzma_index to be encoded
 * \param       out       Beginning of the output buffer
 * \param       out_pos   The next byte will be written to out[*out_pos].
 *                        *out_pos is updated only if encoding succeeds.
 * \param       out_size  Size of the out buffer; the first byte into
 *                        which no data is written to is out[out_size].
 *
 * \return      - LZMA_OK: Encoding was successful.
 *              - LZMA_BUF_ERROR: Output buffer is too small. Use
 *                lzma_index_size() to find out how much output
 *                space is needed.
 *              - LZMA_PROG_ERROR
 *
 * \note        This function doesn't take allocator argument since all
 *              the internal data is allocated on stack.
 */
extern LZMA_API(lzma_ret) lzma_index_buffer_encode(const lzma_index *i,
		uint8_t *out, size_t *out_pos, size_t out_size) lzma_nothrow;


/**
 * \brief       Single-call .xz Index decoder
 *
 * \param       i           If decoding succeeds, *i will point to a new
 *                          lzma_index, which the application has to
 *                          later free with lzma_index_end(). If an error
 *                          occurs, *i will be NULL. The old value of *i
 *                          is always ignored and thus doesn't need to be
 *                          initialized by the caller.
 * \param       memlimit    Pointer to how much memory the resulting
 *                          lzma_index is allowed to require. The value
 *                          pointed by this pointer is modified if and only
 *                          if LZMA_MEMLIMIT_ERROR is returned.
 * \param       allocator   Pointer to lzma_allocator, or NULL to use malloc()
 * \param       in          Beginning of the input buffer
 * \param       in_pos      The next byte will be read from in[*in_pos].
 *                          *in_pos is updated only if decoding succeeds.
 * \param       in_size     Size of the input buffer; the first byte that
 *                          won't be read is in[in_size].
 *
 * \return      - LZMA_OK: Decoding was successful.
 *              - LZMA_MEM_ERROR
 *              - LZMA_MEMLIMIT_ERROR: Memory usage limit was reached.
 *                The minimum required memlimit value was stored to *memlimit.
 *              - LZMA_DATA_ERROR
 *              - LZMA_PROG_ERROR
 */
extern LZMA_API(lzma_ret) lzma_index_buffer_decode(lzma_index **i,
		uint64_t *memlimit, const lzma_allocator *allocator,
		const uint8_t *in, size_t *in_pos, size_t in_size)
		lzma_nothrow;

/**
 * \brief       Internal data structure
 *
 * The contents of this structure is not visible outside the library.
 */
typedef struct lzma_index_parser_internal_s lzma_index_parser_internal;

/**
 * \brief       Reading the indexes of an .xz file
 *
 * The lzma_index_parser_data data structure is passed to
 * lzma_parse_indexes_from_file(), which can be used to retrieve the index
 * information for a given .xz file.
 *
 * It should be initialized with LZMA_INDEX_PARSER_DATA_INIT,
 * and, minimally, the file_size and read_callback() members need to be set.
 *
 * The allocation of internals happens transparently upon usage and does not
 * need to be taken care of.
 * In the case of an error, lzma_parse_indexes_from_file() performs all
 * necessary cleanup.
 * In the case of success, the index member will be set and needs to be
 * freed using lzma_index_end after the caller is done with it. If a custom
 * allocator was set on this struct, it needs to be used for freeing the
 * resulting index, too.
 *
 * Reading the data from the underlying file may happen synchronously or
 * asynchronously, see the description of the read_callback().
 */
typedef struct {
	/**
	 * \brief       Combined Index of all Streams in the file
	 *
	 * This will be set to an lzma_index * when parsing the file was
	 * successful, as indicated by a LZMA_STREAM_END return status.
	 */
	lzma_index *index;

	/**
	 * \brief       Total amount of Stream Padding
	 *
	 * This will be set when the file was successfully read.
	 */
	size_t stream_padding;

	/**
	 * \brief       Callback for reading data from the input file
	 *
	 * This member needs to be set to a function that provides a slice of
	 * the input file.
	 *
	 * The opaque pointer will have the same value as the opaque pointer
	 * set on this struct.
	 *
	 * When being invoked, it should read count bytes from the underlying
	 * file, starting at the specified offset, into buf.
	 * The return value may be -1, in which case
	 * lzma_parse_indexes_from_file() will return with LZMA_DATA_ERROR.
	 * Otherwise, the number of read bytes should be returned. If this is
	 * not the number of requested bytes, it will be assumed that the file
	 * was truncated, and lzma_parse_indexes_from_file() will fail with
	 * LZMA_DATA_ERROR.
	 *
	 * It is possible to perform the underlying I/O operations in an
	 * asynchronous manner. To do so, set the async flag on this struct
	 * to true. After read_callback() is invoked,
	 * lzma_parse_indexes_from_file() will return immediately with
	 * LZMA_OK (unless the read_callback() return value indicates failure),
	 * and you are expected to call lzma_parse_indexes_from_file() with
	 * the same struct as soon as the buffer has been filled.
	 *
	 * If asynchronous reading is used and the underlying read operation
	 * fails, you should set file_size to SIZE_MAX and call
	 * lzma_parse_indexes_from_file() to trigger an error clean up all
	 * remaining internal state.
	 *
	 * You should not perform any operations on this structure until
	 * the data has been read in any case.
	 *
	 * This function is modelled after pread(2), which is a available on
	 * some platforms and can be easily wrapped to be used here.
	 */
	ssize_t (*LZMA_API_CALL read_callback)(void *opaque,
	                                       uint8_t *buf,
	                                       size_t count,
	                                       off_t offset);

	/// Opaque pointer that is passed to read_callback.
	void *opaque;

	/// Whether to return after calling read_callback and wait for
	/// another call. Defaults to synchronous operations.
	lzma_bool async;

	/** \brief       Callback for reading data from the input file
	 *
	 * This needs to be set to the size of the input file before all
	 * other operations. If this is set to SIZE_MAX, the parser will
	 * fail with LZMA_OPTIONS_ERROR. This can be used to clean up
	 * after a failed asynchronous read_callback().
	 *
	 * On error, this will be set to SIZE_MAX.
	 */
	size_t file_size;

	/** \brief       Memory limit for decoding the indexes.
	 *
	 * Set a memory limit for decoding. Default to UINT64_MAX for no limit.
	 * If this is set too low to allocate the internal data structure
	 * that is minimally required for parsing, this will be set to 0.
	 * If this is set too low to parse the underlying .xz file,
	 * this will be set to the amount of memory that would have
	 * been necessary for parsing the file.
	 */
	uint64_t memlimit;

	/// Message that may be set when additional information is available
	/// on error.
	const char *message;

	/**
	 * \brief       Custom memory allocation functions
	 *
	 * In most cases this is NULL which makes liblzma use
	 * the standard malloc() and free().
	 */
	const lzma_allocator *allocator;

	/**
	 * \brief       Data which is internal to the index parser.
	 *
	 * Do not touch. You can check whether this is NULL to see if this
	 * structure currently holds external resources, not counting the
	 * possible index member that is set on success.
	 */
	lzma_index_parser_internal* internal;

	/*
	 * Reserved space to allow possible future extensions without
	 * breaking the ABI. Excluding the initialization of this structure,
	 * you should not touch these, because the names of these variables
	 * may change.
	 */
	void *reserved_ptr1;
	void *reserved_ptr2;
	void *reserved_ptr3;
	void *reserved_ptr4;
	uint64_t reserved_int1;
	uint64_t reserved_int2;
	size_t reserved_int3;
	size_t reserved_int4;
	lzma_reserved_enum reserved_enum1;
	lzma_reserved_enum reserved_enum2;
} lzma_index_parser_data;

/**
 * \brief       Initialization for lzma_index_parser_data
 *
 * When you declare an instance of lzma_index_parser_data, you should
 * immediately initialize it to this value:
 *
 *     lzma_index_parser_data strm = LZMA_INDEX_PARSER_DATA_INIT;
 *
 * Anything which applies for LZMA_STREAM_INIT applies here, too.
 */
#define LZMA_INDEX_PARSER_DATA_INIT \
	{ NULL, 0, NULL, NULL, 0, 0, 0, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, 0, 0, 0, 0, \
	LZMA_RESERVED_ENUM, LZMA_RESERVED_ENUM }

/** \brief      Parse the Index(es) from the given .xz file
 *
 * Read metadata from the underlying file.
 * The info pointer should refer to a lzma_index_parser_data struct that
 * has been initialized using LZMA_INDEX_PARSER_DATA_INIT.
 *
 * This will call info->read_callback() multiple times to read parts of the
 * underlying .xz file and, upon success, fill info->index with an
 * lzma_index pointer that contains metadata for the whole file, accumulated
 * across multiple streams.
 *
 * \param      info      Pointer to a lzma_index_parser_data structure.
 *
 * \return     On success, LZMA_STREAM_END is returned.
 *             On error, another value is returned, and info->message may
 *             be set to provide additional information.
 *             If info->async is set, LZMA_OK may be returned to indicate
 *             that another call to lzma_parse_indexes_from_file() should be
 *             performed after the data has been read.
 */
extern LZMA_API(lzma_ret)
lzma_parse_indexes_from_file(lzma_index_parser_data *info) lzma_nothrow;
